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

#define MAX_SLOTS 10
byte slotIdx;

void changeLooperModeCb(int button, tButtonStatus event, int duration); //Auto/Manual
void changeLooperStatusCb(int button, tButtonStatus event, int duration); //Start Recording/Manual play loop
void replayPreviousLoopCb(int button, tButtonStatus event, int duration); //RePlay previous loop on current slot

void dumpLoopCb(int button, tButtonStatus event, int duration); //Dump loop contents
void slotSelectCb (int knob, int value, tKnobRotate rot);


byte NoteCb(byte channel, byte note, byte velocity, byte onOff);
void SetMode(tLooperMode mode);
void SetStatus(tLooperStatus lstatus);

void ResetLoop();
void ResetPlay(byte playIdx = 0);



void LooperSetup()
{
  MIDIRegisterNoteCb(NoteCb);
  ControlsRegisterButtonCallback(0, eButtonStatus_Released, 0, changeLooperModeCb); //Auto / Manual
  ControlsRegisterButtonCallback(0, eButtonStatus_Released, 1000, dumpLoopCb); //Debug : Dump notes
  ControlsRegisterButtonCallback(1, eButtonStatus_Released, 0, changeLooperStatusCb); //Record / Play / Idle

  ControlsRegisterButtonCallback(2, eButtonStatus_Released, 0, replayPreviousLoopCb); //RePlay previous loop on current slot

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
  
  
  // Play recorded loop
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
  if (replayIdx == sampleSize) replayIdx = 0;
  
}


//Called when Note (on/off) received
//Return : Silent ?
byte NoteCb(byte channel, byte note, byte velocity, byte onOff)
{
  DisplayBlinkRed();
  
  if (looperStatus == eLooperIdle) //Simply replay received note
    return false;

  if (looperStatus == eLooperPlaying) //Simply replay received note FIXME : stop passthrough when just starting replay ?
    return false;
    
  //Recording
  if (noteIdx == MAX_SAMPLE) //Pattern too long
  {
    SetStatus(eLooperIdle);
    DisplayWriteStr("Too Long", 1, 6);
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
          sampleSize = noteIdx - 1; //Remove previous note (1st note repeated) => A B C D
          looperStatus = eLooperPlaying;
          DisplayWriteStr("Looping ! [    ]", 1, 0);
          DisplayWriteInt(sampleSize/2, 1, 12);
          ResetPlay(3);
          
          //Store delta between events     
          // 17100 17200 17300 17400 (2 notes pressed for 100ms and released for 100ms)    
          int i;
          for (i = sampleSize - 1; i > 0; i--)
            aNoteEvents[i].time -= aNoteEvents[i-1].time;
          aNoteEvents[0].time = 0;
          
          return false;
        }
        else
        {
          sampleSize = noteIdx - 2; //FIXME : Not tested...
          DisplayWriteStr("LoopOk !  [    ]", 1, 0);
          DisplayWriteInt(sampleSize/2, 12, 1);
          //Store delta between events     
          // 17100 17200 17300 17400 (2 notes pressed for 100ms and released for 100ms)    
          int i;
          for (i = sampleSize - 1; i > 0; i--)
            aNoteEvents[i].time -= aNoteEvents[i-1].time;
          aNoteEvents[0].time = 0;
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
  aNoteEvents[noteIdx].time = millis();
  aNoteEvents[noteIdx].velocity = velocity;
  noteIdx ++;
 
  return false;
}

void SetMode(tLooperMode mode)
{
  looperMode = mode;
  DisplayWriteStr((looperMode==eLooperManual)?"M":"A", 0, 15);
}


void ResetLoop()
{
  firstNotePressOk = false;
  firstNoteReleaseOk = false;
  sampleSize = 0;
  noteIdx = 0;
}

void ResetPlay(byte playIdx)
{
  replayIdx = playIdx;
  replayTimer = millis();
}


void SetStatus(tLooperStatus lstatus)
{
  switch (lstatus)
  {
    case eLooperRecording:
      looperStatus = eLooperRecording;
      DisplayWriteStr("Recording...    ", 1, 0);
      ResetLoop();
    break;
    case eLooperPlaying:
      looperStatus = eLooperPlaying;
      DisplayWriteStr("Looping !       ", 1, 0);
      ResetPlay();
    break;
    case eLooperIdle:
      looperStatus = eLooperIdle;
      DisplayWriteStr("Idle.           ", 1, 0);
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
      DisplayWriteStr("No sample", 1, 6);
    }
  }
  else if (looperStatus == eLooperPlaying) //Stop playing
  {
    SetStatus(eLooperIdle);
  }
}

 //RePlay previous loop on current slot
void replayPreviousLoopCb(int button, tButtonStatus event, int duration)
{
  if (!sampleSize) //No loop
  {
    DisplayWriteStr("No sample", 1, 6);
    return;
  }
  SetStatus(eLooperPlaying);
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

  
  if (sampleSize) DisplayWriteStr("[ ]   n       ms", 0, 0);
  for (i = 0; i < sampleSize; i++)
  {
    //[4] R n67 1234ms
    DisplayWriteInt(i, 0, 1);
    DisplayWriteStr(aNoteEvents[i].bStatus==0x09?"P":"R", 0, 4);
    DisplayWriteInt(aNoteEvents[i].note, 0, 7);
    DisplayWriteInt(aNoteEvents[i].time, 0, 10);
    delay(1000);
  }
  delay(1000);
  DisplayClear();
}

void slotSelectCb (int knob, int value, tKnobRotate rot)
{
  byte v = MAX_SLOTS - (value * MAX_SLOTS/1024) - 1;
  
  if (v == slotIdx)
    return;
  
  slotIdx = v;
  DisplayWriteStr("[ ]", 0, 0);
  DisplayWriteInt(slotIdx, 0, 1);
}
