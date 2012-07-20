#include "MIDIProcessor.h"
#include "Controls.h"
#include "Display.h"


#define _DEBUG

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
  byte secondNote;
  
  byte replayIdx;
  int  replayTimer;
  byte bChannel;
  tLooperStatus slotStatus;
} tLooperSlot;

tLooperMode   looperMode;
tLooperStatus looperStatus;
tLooperSlot   aSlots[MAX_SLOTS];
int           displayTimeout;

void changeLooperModeCb(int button, tButtonStatus event, int duration); //Auto/Manual
void slotPlayMuteCb(int button, tButtonStatus event, int duration); //Start/stop play
void slotRecordCb(int button, tButtonStatus event, int duration); //Start Recording

void generalPlayStopCb(int button, tButtonStatus event, int duration); //RePlay previous loop on current slot

void dumpLoopCb(int button, tButtonStatus event, int duration); //Dump loop contents
void slotSelectCb (int knob, int value, tKnobRotate rot);


byte NoteCb(byte channel, byte note, byte velocity, byte onOff);
void SetGlobalMode(tLooperMode mode);
void SetStatus(tLooperStatus lstatus);

void ResetLoop(int slot);
void ResetPlay(byte playIdx = 0);

void RefreshDisplay(const char * msg = NULL);



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
  ControlsRegisterButtonCallback(1, eButtonStatus_Released, 0, slotPlayMuteCb); //Play / Idle
  ControlsRegisterButtonCallback(1, eButtonStatus_Released, 1000, slotRecordCb); //Record 

  ControlsRegisterButtonCallback(2, eButtonStatus_Released, 0, generalPlayStopCb); //RePlay previous loop on current slot

  ControlsRegisterKnobCallback(0, slotSelectCb); //Select slot for loop
  ControlsNotifyKnob(0); //Force update for init
  

  SetGlobalMode(eLooperAuto);
  SetStatus(eLooperIdle);
  displayTimeout = 0;
  RefreshDisplay();
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
    if (aSlots[i].sampleSize && (aSlots[i].slotStatus == eLooperPlaying))
    {
      //A (0ms)  B (10ms) C (5ms) D (1ms) E (100ms) A (0ms) ...
      if ((t - aSlots[i].replayTimer) >= aSlots[i].aNoteEvents[aSlots[i].replayIdx].time)
      {
        Serial.write(((aSlots[i].aNoteEvents[aSlots[i].replayIdx].velocity)?0x90:0x80) | aSlots[i].bChannel);
        Serial.write(aSlots[i].aNoteEvents[aSlots[i].replayIdx].note);
        Serial.write(aSlots[i].aNoteEvents[aSlots[i].replayIdx].velocity);
        aSlots[i].replayTimer = t;
        aSlots[i].replayIdx ++;
      }
      if (aSlots[i].replayIdx == aSlots[i].sampleSize) aSlots[i].replayIdx = 0;
    }
  }
  if (displayTimeout && (t - displayTimeout > 2000))
    RefreshDisplay();
}


//Called when Note (on/off) received
//Return : Silent ?
byte NoteCb(byte channel, byte note, byte velocity, byte onOff)
{
  DisplayBlinkRed();

  if (aSlots[slotIdx].slotStatus == eLooperIdle) //Simply replay received note
    return false;

  if (aSlots[slotIdx].slotStatus == eLooperPlaying) //Simply replay received note FIXME : stop passthrough when just starting replay ?
    return false;
    
  //Recording
  if (aSlots[slotIdx].noteIdx == MAX_SAMPLE) //Pattern too long
  {
    aSlots[slotIdx].slotStatus = eLooperIdle;
    RefreshDisplay((char*)"TooLong");
    ResetLoop(slotIdx);
    return false;
  }
  
  if (!onOff && !aSlots[slotIdx].noteIdx) //Loop may not start with a NoteOff event ...
    return false;
   
 
  //Detect loops
  DisplayBlinkGreen();
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
      if (note == aSlots[slotIdx].aNoteEvents[aSlots[slotIdx].secondNote].note) //2nd note also matches --> Loooop !!
      {
        if (looperMode   == eLooperAuto)
        {
          aSlots[slotIdx].sampleSize = aSlots[slotIdx].noteIdx - 2; //Remove previous note (1st note repeated) => A B C D
          aSlots[slotIdx].slotStatus = eLooperPlaying;
          looperStatus = eLooperPlaying;
          RefreshDisplay("Loop !");
          ResetPlay(3);
          
          //Store delta between events     
          // 17100 17200 17300 17400 (2 notes pressed for 100ms and released for 100ms)    
          int i;
          for (i = aSlots[slotIdx].sampleSize - 1; i > 0; i--)
            aSlots[slotIdx].aNoteEvents[i].time -= aSlots[slotIdx].aNoteEvents[i-1].time;
          aSlots[slotIdx].aNoteEvents[0].time = 0;
          
          return false;
        }
        else  //Manual mode
        {
          //Store longest loop found for now
          aSlots[slotIdx].sampleSize = aSlots[slotIdx].noteIdx - 2;
          RefreshDisplay("LoopRdy");
        }
      }
      else
      {
        // A B C D A E
        aSlots[slotIdx].firstNotePressOk   = false; //restart from scratch
        aSlots[slotIdx].firstNoteReleaseOk = false; //restart from scratch
      }
    }
  }

  if (aSlots[slotIdx].noteIdx == 0) //Ignore channel change during record ..
    aSlots[slotIdx].bChannel = channel;
    
  if ((aSlots[slotIdx].secondNote == 0) && aSlots[slotIdx].noteIdx && onOff) //Note first note and NoteOn
    aSlots[slotIdx].secondNote = aSlots[slotIdx].noteIdx;

  aSlots[slotIdx].aNoteEvents[aSlots[slotIdx].noteIdx].note = note;
  aSlots[slotIdx].aNoteEvents[aSlots[slotIdx].noteIdx].time = millis();
  aSlots[slotIdx].aNoteEvents[aSlots[slotIdx].noteIdx].velocity = onOff?velocity:0;
  aSlots[slotIdx].noteIdx ++;
 
  return false;
}



// ######## SLOT SPECIFIC FUNCTIONS #########
//Reset loop contents on current slot
void ResetLoop(int slot)
{
  aSlots[slot].firstNotePressOk   = false;
  aSlots[slot].firstNoteReleaseOk = false;
  aSlots[slot].sampleSize         = 0;
  aSlots[slot].noteIdx            = 0;
  aSlots[slot].secondNote         = 0;
  aSlots[slot].bChannel           = 0;

  memset(aSlots[slot].aNoteEvents, 0x00, MAX_SAMPLE*sizeof(tNoteEvent));

}
//Reset play indexes
void ResetPlay(byte playIdx)
{
  aSlots[slotIdx].replayIdx = playIdx;
  aSlots[slotIdx].replayTimer = millis();
}

// ######## GENERAL LOOPER FUNCTIONS #########
//Change looper status (playing/stop)
void SetStatus(tLooperStatus lstatus)
{
  switch (lstatus)
  {
    case eLooperPlaying:
      looperStatus = eLooperPlaying;
      RefreshDisplay();
    break;
    case eLooperIdle:
      looperStatus = eLooperIdle;
      RefreshDisplay();
    break;
  }
}

//Change Auto/Manual
void SetGlobalMode(tLooperMode mode)
{
  looperMode = mode;
  RefreshDisplay();
}

//Main Display method
void RefreshDisplay(const char * msg) //7chars max
{
  DisplayClear();

  //1st line
  DisplayWriteStr((looperMode==eLooperManual)?" Man":"Auto", 0, 12);
  switch (looperStatus)
  {
    case eLooperIdle:
      DisplayWriteStr("Idle", 0, 0);
    break;
    case eLooperPlaying:
      DisplayWriteStr("Play", 0, 0);
    break;
  }
  
  //2nd line
  DisplayWriteStr("Sl", 1, 0);
  DisplayWriteInt(slotIdx+1, 1, 2);
  switch (aSlots[slotIdx].slotStatus)
  {
    case eLooperIdle:
      if (!aSlots[slotIdx].sampleSize)
        DisplayWriteStr("Empt", 1, 12);
      else
        DisplayWriteStr("Mute", 1, 12);
    break;
    case eLooperPlaying:
      DisplayWriteStr("Play", 1, 12);
    break;
    case eLooperRecording:
      DisplayWriteStr("Rec.", 1, 12);
    break;
  }

  //Custom message
  if (msg)
  {
    DisplayWriteStr(msg, 1, 4);
    displayTimeout = millis();
  }
  else
  {
    displayTimeout = 0;
  }
}


// ######## BUTTON CALLBACKS #########
//## Button 1 : Change global mode (Play/Stop)
void generalPlayStopCb(int button, tButtonStatus event, int duration)
{
  if (looperStatus == eLooperIdle)
    SetStatus(eLooperPlaying);
  else
    SetStatus(eLooperIdle);
}



//## Button 2 : Change slot mode (Play/Stop) or acknowledge loop on manual mode
void slotPlayMuteCb(int button, tButtonStatus event, int duration) //Change status (start recording)
{
  if ((aSlots[slotIdx].slotStatus == eLooperIdle) && aSlots[slotIdx].sampleSize) //Start playing
  {
    aSlots[slotIdx].slotStatus = eLooperPlaying;
    looperStatus = eLooperPlaying;
  }
  else if (aSlots[slotIdx].slotStatus == eLooperPlaying) //Loop (if loop found)
  {
    aSlots[slotIdx].slotStatus = eLooperIdle;
  }
  else if ((aSlots[slotIdx].slotStatus == eLooperRecording) && (looperMode == eLooperManual)) //Manual ack
  {
    if (aSlots[slotIdx].sampleSize) //Manual enable
    {
      //Store delta between events     
      // 17100 17200 17300 17400 (2 notes pressed for 100ms and released for 100ms)    
      int i;
      for (i = aSlots[slotIdx].sampleSize - 1; i > 0; i--)
        aSlots[slotIdx].aNoteEvents[i].time -= aSlots[slotIdx].aNoteEvents[i-1].time;
      aSlots[slotIdx].aNoteEvents[0].time = 0;
      ResetPlay(0);      //TODO: start playing at appropriate note !          

      aSlots[slotIdx].slotStatus = eLooperPlaying;
      looperStatus = eLooperPlaying;
    }
    else //No loop
    {
      RefreshDisplay("NoLoop!");
      return;
    }
  }
  else
  {
    aSlots[slotIdx].slotStatus = eLooperIdle;
    RefreshDisplay("NoLoop!");
    return;
  }
  RefreshDisplay();
}

//## Button 2 (long press) : Switch current slot to Recording status
void slotRecordCb(int button, tButtonStatus event, int duration) //Start Recording
{
  ResetLoop(slotIdx);
  aSlots[slotIdx].slotStatus = eLooperRecording;
  RefreshDisplay();
}

//## Button 3 : Change looper mode (Auto/manual ack)
void changeLooperModeCb(int button, tButtonStatus event, int duration) //Change mode
{
  switch (looperMode)
  {
    case eLooperManual:
      SetGlobalMode(eLooperAuto);
    break;
    case eLooperAuto:
      SetGlobalMode(eLooperManual);
    break;
  }
}

// ######## KNOBS CALLBACKS #########
//## Knob 1 : Select slot
void slotSelectCb (int knob, int value, tKnobRotate rot)
{
  byte v = MAX_SLOTS - (value * MAX_SLOTS/1024) - 1;
  
  if (v == slotIdx)
    return;
  
  slotIdx = v;
  RefreshDisplay();
}


#ifdef _DEBUG
void dumpLoopCb(int button, tButtonStatus event, int duration)
{
  int i;
  DisplayClear();
  
  DisplayWriteStr("Debug ...      ", 0, 0);
  delay(1000);
  DisplayWriteStr("Mode :         ", 0, 0);
  DisplayWriteStr((looperMode==eLooperManual)?"Manual":"Auto", 0, 7);
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
    DisplayWriteStr(aSlots[slotIdx].aNoteEvents[i].velocity?"P":"R", 0, 4);
    DisplayWriteInt(aSlots[slotIdx].aNoteEvents[i].note, 0, 7);
    DisplayWriteInt(aSlots[slotIdx].aNoteEvents[i].time, 0, 10);
    delay(1000);
  }
  delay(1000);
  RefreshDisplay();
}
#endif

