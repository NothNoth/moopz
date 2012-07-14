#include "MIDIProcessor.h"
#include "Controls.h"
#include "Display.h"



typedef enum
{
  eLooperManual,   //Detect loops and wait for manual ack
  eLooperAuto      //Detect loops and auto run 
} tLooperMode;

typedef enum
{
  eLooperIdle,      //Do nothing
  eLooperPlaying,   //Replay recorded notes
  eLooperRecording  //Record notes and detect loops
} tLooperStatus;

typedef struct
{
  int time;
  byte bStatus;
  byte bChannel;
  byte note;
  byte velocity;
} tNoteEvent;


#define MAX_SAMPLE 64
tNoteEvent aNoteEvents[MAX_SAMPLE];
tLooperMode   looperMode;
tLooperStatus looperStatus;

byte noteIdx;
byte sampleSize;
bool firstNotePressOk;
bool firstNoteReleaseOk;


byte replayIdx;
int replayTimer;
byte slotIdx;

void changeLooperModeCb(int button, tButtonStatus event, int duration); //Auto/Manual
void changeLooperStatusCb(int button, tButtonStatus event, int duration); //Start Recording/Manual play loop
void dumpLoopCb(int button, tButtonStatus event, int duration); //Dump loop contents
void slotSelectCb (int knob, int value, tKnobRotate rot);


byte NoteCb(byte channel, byte note, byte velocity, byte onOff);
void SetMode(tLooperMode mode);
void SetStatus(tLooperStatus lstatus);

void ResetLoop();
void ResetPlay();



void LooperSetup()
{
  MIDIRegisterNoteCb(NoteCb);
  ControlsRegisterButtonCallback(2, eButtonStatus_Released, 0, changeLooperModeCb); //Auto / Manual
  ControlsRegisterButtonCallback(1, eButtonStatus_Released, 0, changeLooperStatusCb); //Record / Play / Idle
  ControlsRegisterButtonCallback(0, eButtonStatus_Released, 1000, dumpLoopCb); //Debug : Dump notes

  ControlsRegisterKnobCallback(0, slotSelectCb); //Select slot for loop
  ControlsNotifyKnob(0); //Force update for init
  
  memset(aNoteEvents, 0x00, MAX_SAMPLE*sizeof(tNoteEvent));
  slotIdx = 0;
  
  ResetLoop();
  SetMode(eLooperAuto);
  SetStatus(eLooperIdle);
}

void LooperUpdate()
{
  int t;
  if (looperStatus != eLooperPlaying) //Nothing to do
    return;
    
  t = millis();

//A (0ms)  B (10ms) C (5ms) D (1ms) E (100ms) A (0ms) ...
  if ((t - replayTimer) >= aNoteEvents[replayIdx].time)
  {
    Serial.write((aNoteEvents[replayIdx].bStatus << 4) | aNoteEvents[replayIdx].bChannel);
    Serial.write(aNoteEvents[replayIdx].note);
    Serial.write(aNoteEvents[replayIdx].velocity);
    replayTimer = t;
    replayIdx ++;
  }
  /*
  else
  {
    DisplayBlinkGreen();
    DisplayWriteStr("Next in         ", 0, 0);
    DisplayWriteInt(aNoteEvents[replayIdx].time - (t - replayTimer), 8, 0);
  }*/
  if (replayIdx > sampleSize) replayIdx = 0;
  
}


//Called when Note (on/off) received
//Return : Silent ?
byte NoteCb(byte channel, byte note, byte velocity, byte onOff)
{
  DisplayBlinkRed();
  
  /*
  DisplayWriteStr("Note            ", 0, 1);
  DisplayWriteStr(onOff?"On ":"Off", 5, 1);
  DisplayWriteInt(note, 9, 1);
  DisplayWriteInt(velocity, 12, 1);
*/

  if (looperStatus == eLooperIdle) //Simply replay received note
    return false;

  if (looperStatus == eLooperPlaying) //Simply replay received note FIXME : stop passthrough when just starting replay ?
    return false;
    
  //Recording
  if (noteIdx == MAX_SAMPLE) //Pattern too long
  {
    SetStatus(eLooperIdle);
    DisplayWriteStr("Too Long", 6, 1);
    ResetLoop();
    return false;
  }
    
  //Detect loops
  if (noteIdx > 3) //Press+Release 1st note, Press 2nd note
  {
    if (!firstNotePressOk) //Check note 1 => A B C D + A
    {
      if (note == aNoteEvents[0].note) //Current note is first note of sample
        firstNotePressOk = true;
    }
    else if (firstNotePressOk && !firstNoteReleaseOk)
    {
      if (note == aNoteEvents[1].note)
        firstNoteReleaseOk = true;
      else
        firstNotePressOk = false;
    }
    else //First note already matched (+release), second note ?  => A B C D A + B
    {
      //TODO : also check time elapsed between Note1/Note0 and millis() = aNoteEvents[noteIdx-1] + epsilon;
      if (note == aNoteEvents[2].note) //2nd note also matches --> Loooop !!
      {
        if (looperMode   == eLooperAuto)
        {
          sampleSize = noteIdx - 2; //Remove previous note (1st note repeated) => A B C D
          looperStatus = eLooperPlaying;
          DisplayWriteStr("Looping ! [    ]", 0, 1);
          DisplayWriteInt(sampleSize/2, 12, 1);
          ResetPlay();
          return true; //Do not play this note
        }
        else
        {
          sampleSize = noteIdx - 2; //FIXME : Not tested...
          DisplayWriteStr("LoopOk !  [    ]", 0, 1);
          DisplayWriteInt(sampleSize/2, 12, 1);
        }
      }
      else
      {
        // A B C D A E
        firstNotePressOk = false; //restart from scratch
        firstNoteReleaseOk = false; //restart from scratch
      }
    }
  }

  aNoteEvents[noteIdx].bStatus = onOff?0x09:0x08;
  aNoteEvents[noteIdx].bChannel = channel;
  aNoteEvents[noteIdx].note = note;
  aNoteEvents[noteIdx].time = (!noteIdx)?0:(millis()-aNoteEvents[noteIdx-1].time); //Save time since previous note
  aNoteEvents[noteIdx].velocity = velocity;
  noteIdx ++;
 
  return false;
}

void SetMode(tLooperMode mode)
{
  looperMode = mode;
  DisplayWriteStr((looperMode==eLooperManual)?"M":"A", 15, 0);
}


void ResetLoop()
{
  firstNotePressOk = false;
  firstNoteReleaseOk = false;
  sampleSize = 0;
  noteIdx = 0;
}

void ResetPlay()
{
  replayIdx = 0;
  replayTimer = millis();
}


void SetStatus(tLooperStatus lstatus)
{
  switch (lstatus)
  {
    case eLooperRecording:
      looperStatus = eLooperRecording;
      DisplayWriteStr("Recording...    ", 0, 1);
      ResetLoop();
    break;
    case eLooperPlaying:
      looperStatus = eLooperPlaying;
      DisplayWriteStr("Looping !       ", 0, 1);
      ResetPlay();
    break;
    case eLooperIdle:
      looperStatus = eLooperIdle;
      DisplayWriteStr("Idle.           ", 0, 1);
    break;
  }
}

// ######## CALLBACKS #########

//Called when mode button pressed
void changeLooperModeCb(int button, tButtonStatus event, int duration) //Change mode
{
  switch (looperMode)
  {
    case eLooperManual:
      SetMode(eLooperAuto);
    break;
    case eLooperAuto:
      SetMode(eLooperManual);
    break;
  }
}

void changeLooperStatusCb(int button, tButtonStatus event, int duration) //Change status (start recording)
{
  if (looperStatus == eLooperIdle) //Start recording
  {
    SetStatus(eLooperRecording);
  }
  else if (looperStatus == eLooperRecording) //Loop (if loop found)
  {
    if (sampleSize) //Manual enable
    {
      SetStatus(eLooperPlaying);
    }
    else //No loop
    {
      SetStatus(eLooperIdle);
      DisplayWriteStr("No sample", 6, 1);
    }
  }
  else if (looperStatus == eLooperPlaying) //Stop playing
  {
    SetStatus(eLooperIdle);
  }
}

void dumpLoopCb(int button, tButtonStatus event, int duration)
{
  int i;
  DisplayClear();
  
  DisplayWriteStr("Debug ...      ", 0, 0);
  delay(1000);
  DisplayWriteStr("Mode :         ", 0, 0);
  DisplayWriteStr(looperMode==eLooperManual?"Manual":"Auto", 0, 7);
  delay(1000);
  DisplayWriteStr("Status :       ", 0, 0);
  DisplayWriteStr(looperStatus==eLooperIdle?"Idle":(looperStatus==eLooperPlaying?"Playing":"Recording"), 0, 9);
  delay(1000);
  DisplayWriteStr("SampleSize :   ", 0, 0);
  DisplayWriteInt(sampleSize, 0, 13);
  delay(1000);

  
  if (sampleSize) DisplayWriteStr("[ ]   n       m", 0, 0);
  for (i = 0; i < sampleSize; i++)
  {
    //[4] R n67 1234ms
    DisplayWriteInt(i, 0, 1);
    DisplayWriteStr(aNoteEvents[i].bStatus==0x09?"P":"R", 0, 4);
    DisplayWriteInt(aNoteEvents[i].note, 0, 7);
    DisplayWriteInt(aNoteEvents[i].time, 0, 10);
    delay(1000);
  }
}

void slotSelectCb (int knob, int value, tKnobRotate rot)
{
  byte v = value * 10/1024;
  
  if (v == slotIdx)
    return;
  
  slotIdx = v;
  DisplayWriteStr("[ ]", 0, 0);
  DisplayWriteInt(slotIdx, 1, 0);
}
