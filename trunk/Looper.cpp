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

typedef struct //8 byte struct
{
  int time;
  byte bStatus;
  byte bChannel;
  byte note;
  byte velocity;
} tNoteEvent;


#define MAX_SAMPLE 32 //Max events in sample (32 notes)
#define MAX_SLOTS 4
byte slotIdx = 0;

typedef struct
{
  tNoteEvent aNoteEvents[MAX_SAMPLE];
  
  byte noteIdx;
  byte sampleSize;
  bool firstNotePressOk;
  bool firstNoteReleaseOk;

  byte replayIdx;
  int replayTimer;
} tLooperSlot;

tLooperMode   looperMode;
tLooperStatus looperStatus;
tLooperSlot   aSlots[MAX_SLOTS];

void changeLooperModeCb(int button, tButtonStatus event, int duration); //Auto/Manual
void changeLooperStatusCb(int button, tButtonStatus event, int duration); //Start Recording/Manual play loop
void replayPreviousLoopCb(int button, tButtonStatus event, int duration); //RePlay previous loop on current slot

void dumpLoopCb(int button, tButtonStatus event, int duration); //Dump loop contents
void slotSelectCb (int knob, int value, tKnobRotate rot);


byte NoteCb(byte channel, byte note, byte velocity, byte onOff);
void SetMode(tLooperMode mode);
void SetStatus(tLooperStatus lstatus);

void ResetLoop(int slot);
void ResetPlay(byte playIdx = 0);



void LooperSetup()
{
  int i;
  MIDIRegisterNoteCb(NoteCb);

  memset(aSlots, 0x00, MAX_SLOTS*sizeof(tLooperSlot));
  for (i = 0; i < MAX_SLOTS; i++)
    ResetLoop(i);

  ControlsRegisterButtonCallback(0, eButtonStatus_Released, 0, changeLooperModeCb); //Auto / Manual
#ifdef _DEBUG
  ControlsRegisterButtonCallback(0, eButtonStatus_Released, 1000, dumpLoopCb); //Debug : Dump notes
#endif
  ControlsRegisterButtonCallback(1, eButtonStatus_Released, 0, changeLooperStatusCb); //Record / Play / Idle

  ControlsRegisterButtonCallback(2, eButtonStatus_Released, 0, replayPreviousLoopCb); //RePlay previous loop on current slot

  ControlsRegisterKnobCallback(0, slotSelectCb); //Select slot for loop
  ControlsNotifyKnob(0); //Force update for init
  

  SetMode(eLooperAuto);
  SetStatus(eLooperIdle);
}

void LooperUpdate()
{
  int t, i;
  
  if (looperStatus != eLooperPlaying) //Nothing to do
    return;
    
  t = millis();
  // Play recorded loops

  for (i = 0; i < MAX_SLOTS; i++)
  {
    if (aSlots[i].sampleSize)
    {
      //A (0ms)  B (10ms) C (5ms) D (1ms) E (100ms) A (0ms) ...
      if ((t - aSlots[i].replayTimer) >= aSlots[i].aNoteEvents[aSlots[i].replayIdx].time)
      {
        Serial.write((aSlots[i].aNoteEvents[aSlots[i].replayIdx].bStatus << 4) | aSlots[i].aNoteEvents[aSlots[i].replayIdx].bChannel);
        Serial.write(aSlots[i].aNoteEvents[aSlots[i].replayIdx].note);
        Serial.write(aSlots[i].aNoteEvents[aSlots[i].replayIdx].velocity);
        aSlots[i].replayTimer = t;
        aSlots[i].replayIdx ++;
      }
      if (aSlots[i].replayIdx == aSlots[i].sampleSize) aSlots[i].replayIdx = 0;
    }
  }
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
  if (aSlots[slotIdx].noteIdx == MAX_SAMPLE) //Pattern too long
  {
    SetStatus(eLooperIdle);
    DisplayWriteStr("Too Long", 1, 6);
    ResetLoop(slotIdx);
    return false;
  }
    
  //Detect loops
  if (aSlots[slotIdx].noteIdx > 3) //Press+Release 1st note, Press 2nd note
  {
    if (!aSlots[slotIdx].firstNotePressOk) //Check note 1 => A B C D + A
    {
      if (note == aSlots[slotIdx].aNoteEvents[0].note) //Current note is first note of sample
        aSlots[slotIdx].firstNotePressOk = true;
    }
    else if (aSlots[slotIdx].firstNotePressOk && !aSlots[slotIdx].firstNoteReleaseOk)
    {
      if (note == aSlots[slotIdx].aNoteEvents[1].note)
        aSlots[slotIdx].firstNoteReleaseOk = true;
      else
        aSlots[slotIdx].firstNotePressOk = false;
    }
    else //First note already matched (+release), second note ?  => A B C D A + B
    {
      //TODO : also check time elapsed between Note1/Note0 and millis() = aNoteEvents[noteIdx-1] + epsilon;
      if (note == aSlots[slotIdx].aNoteEvents[2].note) //2nd note also matches --> Loooop !!
      {
        if (looperMode   == eLooperAuto)
        {
          aSlots[slotIdx].sampleSize = aSlots[slotIdx].noteIdx - 1; //Remove previous note (1st note repeated) => A B C D
          looperStatus = eLooperPlaying;
          DisplayWriteStr("Looping ! [    ]", 1, 0);
          DisplayWriteInt(aSlots[slotIdx].sampleSize/2, 1, 12);
          ResetPlay(3);
          
          //Store delta between events     
          // 17100 17200 17300 17400 (2 notes pressed for 100ms and released for 100ms)    
          int i;
          for (i = aSlots[slotIdx].sampleSize - 1; i > 0; i--)
            aSlots[slotIdx].aNoteEvents[i].time -= aSlots[slotIdx].aNoteEvents[i-1].time;
          aSlots[slotIdx].aNoteEvents[0].time = 0;
          
          return false;
        }
        else
        {
          aSlots[slotIdx].sampleSize = aSlots[slotIdx].noteIdx - 2; //FIXME : Not tested...
          DisplayWriteStr("LoopOk !  [    ]", 1, 0);
          DisplayWriteInt(aSlots[slotIdx].sampleSize/2, 12, 1);
          //Store delta between events     
          // 17100 17200 17300 17400 (2 notes pressed for 100ms and released for 100ms)    
          int i;
          for (i = aSlots[slotIdx].sampleSize - 1; i > 0; i--)
            aSlots[slotIdx].aNoteEvents[i].time -= aSlots[slotIdx].aNoteEvents[i-1].time;
          aSlots[slotIdx].aNoteEvents[0].time = 0;
        }
      }
      else
      {
        // A B C D A E
        aSlots[slotIdx].firstNotePressOk = false; //restart from scratch
        aSlots[slotIdx].firstNoteReleaseOk = false; //restart from scratch
      }
    }
  }

  aSlots[slotIdx].aNoteEvents[aSlots[slotIdx].noteIdx].bStatus = onOff?0x09:0x08;
  aSlots[slotIdx].aNoteEvents[aSlots[slotIdx].noteIdx].bChannel = channel;
  aSlots[slotIdx].aNoteEvents[aSlots[slotIdx].noteIdx].note = note;
  aSlots[slotIdx].aNoteEvents[aSlots[slotIdx].noteIdx].time = millis();
  aSlots[slotIdx].aNoteEvents[aSlots[slotIdx].noteIdx].velocity = velocity;
  aSlots[slotIdx].noteIdx ++;
 
  return false;
}

void SetMode(tLooperMode mode)
{
  looperMode = mode;
  DisplayWriteStr((looperMode==eLooperManual)?"M":"A", 0, 15);
}


void ResetLoop(int slot)
{
  aSlots[slot].firstNotePressOk = false;
  aSlots[slot].firstNoteReleaseOk = false;
  aSlots[slot].sampleSize = 0;
  aSlots[slot].noteIdx = 0;
  memset(aSlots[slot].aNoteEvents, 0x00, MAX_SAMPLE*sizeof(tNoteEvent));

}

void ResetPlay(byte playIdx)
{
  aSlots[slotIdx].replayIdx = playIdx;
  aSlots[slotIdx].replayTimer = millis();
}


void SetStatus(tLooperStatus lstatus)
{
  switch (lstatus)
  {
    case eLooperRecording:
      looperStatus = eLooperRecording;
      DisplayWriteStr("Recording...    ", 1, 0);
      ResetLoop(slotIdx);
    break;
    case eLooperPlaying:
      looperStatus = eLooperPlaying;
      DisplayWriteStr("Looping !       ", 1, 0);
      ResetPlay(slotIdx);
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
    if (aSlots[slotIdx].sampleSize) //Manual enable
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
  if (!aSlots[slotIdx].sampleSize) //No loop
  {
    DisplayWriteStr("No sample", 1, 6);
    return;
  }
  SetStatus(eLooperPlaying);
}

#ifdef _DEBUG
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
  DisplayWriteInt(aSlots[slotIdx].sampleSize, 0, 13);
  delay(1000);

  
  if (aSlots[slotIdx].sampleSize) DisplayWriteStr("[ ]   n       ms", 0, 0);
  for (i = 0; i < aSlots[slotIdx].sampleSize; i++)
  {
    //[4] R n67 1234ms
    DisplayWriteInt(i, 0, 1);
    DisplayWriteStr(aSlots[slotIdx].aNoteEvents[i].bStatus==0x09?"P":"R", 0, 4);
    DisplayWriteInt(aSlots[slotIdx].aNoteEvents[i].note, 0, 7);
    DisplayWriteInt(aSlots[slotIdx].aNoteEvents[i].time, 0, 10);
    delay(1000);
  }
  delay(1000);
  DisplayClear();
}
#endif

void slotSelectCb (int knob, int value, tKnobRotate rot)
{
  byte v = MAX_SLOTS - (value * MAX_SLOTS/1024) - 1;
  
  if (v == slotIdx)
    return;
  
  slotIdx = v;
  DisplayWriteStr("[ ]", 0, 0);
  DisplayWriteInt(slotIdx, 0, 1);
}
