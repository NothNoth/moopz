#include "MIDIProcessor.h"
#include "Controls.h"
#include "Display.h"


#define _DEBUG
#define IS_NOTE_OFF(_n)    (((((_n).velocity&0x7F) == 0x00)?true  : false))


byte CharPlay[8] = {
  0b00000,
  0b10000,
  0b11000,
  0b11110,
  0b11110,
  0b11000,
  0b10000,
  0b00000,
};

byte CharStop[8] = {
  0b00000,
  0b10010,
  0b10010,
  0b10010,
  0b10010,
  0b10010,
  0b10010,
  0b00000
};


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
  unsigned int time;
  byte note;
  byte velocity;
  unsigned int duration;
} tNoteEvent;


#define MAX_SAMPLE 32 //Max events in sample (32 notes)
#define MAX_SLOTS 4
byte slotIdx = 0;

typedef struct
{
  tNoteEvent aNoteEvents[MAX_SAMPLE];
  
  byte noteIdx;  //Current note record index
  byte sampleSize;  //Complete size of sample 
  unsigned int repeatDelay; //delay between last not and first note
  
  byte replayIdx;              //Current note being played on the loop
  unsigned long previousLoopTimestamp;  //Ts of last played NoteOff (Note Off are not ordered, so we have no index) 
  unsigned long  firstNoteTimestamp;  //Current timestamp for note 0 when playing or recording "when did we play first note ?"
  byte bChannel;               //MIDI channel for this slot
  tLooperStatus slotStatus;    //Slot status
} tLooperSlot;

tLooperMode   looperMode;
tLooperStatus looperStatus;
tLooperSlot   aSlots[MAX_SLOTS];
unsigned long displayTimeout;

//Callbacks for buttons/Knobs
void changeLooperModeCb(byte button, tButtonStatus event, int duration); //Auto/Manual
void slotPlayMuteCb(byte button, tButtonStatus event, int duration); //Start/stop play
void slotRecordCb(byte button, tButtonStatus event, int duration); //Start Recording
void generalPlayStopCb(byte button, tButtonStatus event, int duration); //RePlay previous loop on current slot
void dumpLoopCb(byte button, tButtonStatus event, int duration); //Dump loop contents
void slotSelectCb (byte knob, int value, tKnobRotate rot);

//MIDI event callback
byte NoteCb(byte channel, byte note, byte velocity, unsigned long timestamp);

//Looper mode changes
void SetGlobalMode(tLooperMode mode);
void SetStatus(tLooperStatus lstatus);
void ResetLoop(byte slot);
void ResetPlay(byte playIdx = 0);

void RefreshDisplay(const char * msg = NULL);



void LooperSetup()
{
  int i;
  MIDIRegisterNoteCb(NoteCb);

  DisplayCreateChar(CharPlay, 0);
  DisplayCreateChar(CharStop, 1);

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
  byte s, i;
  
  if (looperStatus != eLooperPlaying) //Nothing to do
    return;
  unsigned long timestamp = millis();
  
  // Play recorded loops
  for (s = 0; s < MAX_SLOTS; s++)
  {
    tLooperSlot * slot = &aSlots[s];
    
    if (slot->sampleSize && ((slot->slotStatus == eLooperPlaying)||(slot->slotStatus == eLooperIdle)))
    {
      //A (0ms)  B (10ms) C (5ms) D (1ms) E (100ms) A (0ms) ...
      //Increase timers ont muted slots to keep sync
      
      //Play note ON events
      while ((timestamp > slot->firstNoteTimestamp) && //firstNoteTimestampcan be set to a future date (end loop delay)
             ((int)(timestamp - slot->firstNoteTimestamp) >= slot->aNoteEvents[slot->replayIdx].time))
      {
         //On first loop note, make sure there's no pending Note Off to be processed from previous loop
        if (!slot->replayIdx) 
        {
          //Flush remaning note off
          for (i = 0; i < slot->sampleSize; i++)
          {
            unsigned int noteOffTs = slot->aNoteEvents[i].time + slot->aNoteEvents[i].duration;
            if (slot->previousLoopTimestamp < timestamp + noteOffTs)
            {
              Serial.write(0x80 | slot->bChannel);
              Serial.write(slot->aNoteEvents[i].note);
              Serial.write(slot->aNoteEvents[i].velocity);
            }
          }
          slot->previousLoopTimestamp = slot->firstNoteTimestamp;
        }
        
        //Play note
        if (slot->slotStatus == eLooperPlaying)
        {
          Serial.write(0x90 | slot->bChannel);
          Serial.write(slot->aNoteEvents[slot->replayIdx].note);
          Serial.write(slot->aNoteEvents[slot->replayIdx].velocity);
        }
        slot->replayIdx ++;
          
        //End of loop, start again
        if (slot->replayIdx == slot->sampleSize) 
        {
          //Start at idx 0
          slot->replayIdx = 0;
          
          //Wait for last note to end using repeatDelay (we don't want to play last note and first one at the same time !)
          slot->firstNoteTimestamp = millis() + slot->repeatDelay; //Set sequence ts start
        }
      }
      
      //Play note off events
      for (i = 0; i < slot->sampleSize; i++)
      {
        unsigned int noteOffTs = slot->aNoteEvents[i].time + slot->aNoteEvents[i].duration;
        if ((slot->previousLoopTimestamp < slot->firstNoteTimestamp + noteOffTs) && //not already played
            (timestamp >= slot->firstNoteTimestamp + noteOffTs))                     //time to play it
        {
          Serial.write(0x80 | slot->bChannel);
          Serial.write(slot->aNoteEvents[i].note);
          Serial.write(slot->aNoteEvents[i].velocity);
        }
      }
      slot->previousLoopTimestamp = timestamp;
    }
  }
  
  //Auto vanish messages after 2s
  if (displayTimeout && ((int)(timestamp - displayTimeout) > 2000))
    RefreshDisplay();
}

//Updates "sampleSIze" to the longest loop found on given slot
//Returns current position in loop in that case or 0xFF if no loop is detected
byte LoopDetect(tLooperSlot * slot)
{
  if (slot->noteIdx < 4) //Need at least 4 notes "AB AB" to detect "AB".
    return 0xFF;
  
  if ((slot->aNoteEvents[0].note == slot->aNoteEvents[slot->noteIdx - 2].note) &&
      (slot->aNoteEvents[1].note == slot->aNoteEvents[slot->noteIdx - 1].note))
  {
    //Set sample length
    slot->sampleSize = slot->noteIdx - 2;
    //Compute delay between last note of the sample and first one
    //If we don't do last, first note will be play immediately after last one
    slot->repeatDelay = slot->aNoteEvents[slot->noteIdx-2].time - slot->aNoteEvents[slot->noteIdx-3].time;
    return 1;//Consider we've been playing note 1 (0, 1 .. and then next is 2)
  }
  return 0xFF; //Nothing found
}

bool AddNoteOff(tLooperSlot * slot, byte note, unsigned long timestamp)
{
  //Note off : try to find correponding Note On and update duration
  for (int i = slot->noteIdx - 1; i >= 0; i--)
  {
    if ((slot->aNoteEvents[i].note == note) && (slot->aNoteEvents[i].duration == 0)) //Corresponding Note On found !
    {
      //Update note duration
      slot->aNoteEvents[i].duration = (int)(timestamp - slot->firstNoteTimestamp) - slot->aNoteEvents[i].time;
      return true;
    }
  }
  return false; 
}

bool AddNoteOn(tLooperSlot * slot, byte note, byte velocity, unsigned long timestamp)
{
  slot->aNoteEvents[slot->noteIdx].note     = note;
  slot->aNoteEvents[slot->noteIdx].time     = (int)(timestamp - slot->firstNoteTimestamp);
  slot->aNoteEvents[slot->noteIdx].velocity = velocity;
  slot->aNoteEvents[slot->noteIdx].duration = 0;  //Note Off event will set duration
  slot->noteIdx ++;  
  return true;
}

//Adds a note on the current slot
//Detects and optimize chords
//returns false on slot full
bool AddNote(tLooperSlot * slot, byte note, byte velocity, byte channel, unsigned long timestamp)
{
  if (slot->noteIdx == MAX_SAMPLE) //Slot full
    return false;
   
  if ((slot->noteIdx == 0) && (velocity == 0))//Ignore "NoteOff" as first sample event (not an error)
    return true;
    
  if (slot->noteIdx == 0) //First note of a loop decides the slot's whole channel (reject channel change during loop record)
  {
    slot->bChannel = channel;
    slot->firstNoteTimestamp = timestamp; //Set beginning of loop timestamp
    RefreshDisplay();
  }
  
  if (!velocity)  
    return AddNoteOff(slot, note, timestamp);
  else
    return AddNoteOn(slot, note, velocity, timestamp);
  
  return true;
}


//Called when Note (on/off) received
//Return : Silent ?
byte NoteCb(byte channel, byte note, byte velocity, unsigned long timestamp)
{
  byte loopFound;
  tLooperSlot * slot = &aSlots[slotIdx];
  DisplayBlinkRed();

  if (slot->slotStatus == eLooperIdle) //Simply replay received note
    return false;

  if (slot->slotStatus == eLooperPlaying) //Simply replay received note FIXME : stop passthrough when just starting replay ?
    return false;
    
  //Recording
  if (slot->noteIdx == MAX_SAMPLE) //Pattern too long
  {
    slot->slotStatus = eLooperIdle;
    RefreshDisplay((char*)"Too long !");
    ResetLoop(slotIdx);
    return false;
  }
  
  if (!velocity && !slot->noteIdx) //Loop may not start with a NoteOff event ...
    return false;
   
  //DisplayBlinkGreen();
  if (!AddNote(slot, note, velocity, channel, timestamp))
  {
    //Cannot add another note on slot
    //-> sample must be too long
    slot->slotStatus = eLooperIdle;
    RefreshDisplay((char*)"Too long !");
    ResetLoop(slotIdx);
    return false;
  }
  
  if ((velocity & 0x7F) == 0x00) //NoteOff is not used to setup a loop
    return false;
  
  loopFound = LoopDetect(slot);
 
  if (loopFound == 0xFF)//No loop, continue
    return false;
   
  if (looperMode   == eLooperAuto)
  {
    slot->slotStatus = eLooperPlaying;
    looperStatus = eLooperPlaying;
    RefreshDisplay("Loop Ok !");
    ResetPlay(loopFound);
    return false;
  }
  else //Message and wait for manual ack
  {
    RefreshDisplay("Loop Ready!");
    return false;
  }

  return false;
}



// ######## SLOT SPECIFIC FUNCTIONS #########
//Reset loop contents on current slot
void ResetLoop(byte slot)
{
  aSlots[slot].sampleSize         = 0;
  aSlots[slot].noteIdx            = 0;
  aSlots[slot].bChannel           = 0;

  memset(aSlots[slot].aNoteEvents,    0x00, MAX_SAMPLE*sizeof(tNoteEvent));
}
//Reset play indexes
void ResetPlay(byte playIdx)
{
  aSlots[slotIdx].replayIdx = playIdx;
  aSlots[slotIdx].firstNoteTimestamp = millis() - aSlots[slotIdx].aNoteEvents[playIdx].time; //Compute a fake 1st note timestamp (roll back in time)
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

/*
   X|Auto|Ch07|Sl 4
   ............Rec.

*/

  //1st line
  switch (looperStatus)
  {
    case eLooperIdle:
      DisplayWriteChar(1, 0,0);
    break;
    case eLooperPlaying:
      DisplayWriteChar(0, 0,0);
    break;
  }
  DisplayWriteStr((looperMode==eLooperManual)?"|Man |":"|Auto|", 0, 1);

  if (aSlots[slotIdx].sampleSize)
  {
    DisplayWriteStr("Ch00|", 0, 7);
    DisplayWriteInt(aSlots[slotIdx].bChannel+1, 0, (aSlots[slotIdx].bChannel>9)?9:10); //Ch01 - Ch16
  }
  else
  {
    DisplayWriteStr("ChXX|", 0, 7);
  }
  DisplayWriteStr("Sl ", 0, 12);
  DisplayWriteInt(slotIdx+1, 0, 15);
  
  //2nd line
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
    DisplayWriteStr(msg, 1, 0);
    displayTimeout = millis();
  }
  else
  {
    displayTimeout = 0;
  }
}

//Sends a "All note off" message to stop all pending notes.
void ChannelAllOff(byte channel)
{
  //Write 0xB<channel> 0x7B 0x00
  Serial.write(0xB0 | channel);
  Serial.write(0x7B);
  Serial.write((byte)0x00);
}


// ######## BUTTON CALLBACKS #########
//## Button 1 : Change global mode (Play/Stop)
void generalPlayStopCb(byte button, tButtonStatus event, int duration)
{
  if (looperStatus == eLooperIdle)
  {
    SetStatus(eLooperPlaying);
  }
  else
  {
    int i;
    for (i = 0; i < MAX_SLOTS; i++)
    {
      if (aSlots[i].slotStatus == eLooperPlaying)
        ChannelAllOff(aSlots[i].bChannel);
    }
    SetStatus(eLooperIdle);
  }
}



//## Button 2 : Change slot mode (Play/Stop) or acknowledge loop on manual mode
void slotPlayMuteCb(byte button, tButtonStatus event, int duration) //Change status (start recording)
{
  if ((aSlots[slotIdx].slotStatus == eLooperIdle) && aSlots[slotIdx].sampleSize) //Start playing
  {
    aSlots[slotIdx].slotStatus = eLooperPlaying;
    looperStatus = eLooperPlaying;
  }
  else if (aSlots[slotIdx].slotStatus == eLooperPlaying) //Loop (if loop found)
  {
    ChannelAllOff(aSlots[slotIdx].bChannel);
    aSlots[slotIdx].slotStatus = eLooperIdle;
  }
  else if ((aSlots[slotIdx].slotStatus == eLooperRecording) && (looperMode == eLooperManual)) //Manual ack
  {
    if (aSlots[slotIdx].sampleSize) //Manual enable
    {
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
void slotRecordCb(byte button, tButtonStatus event, int duration) //Start Recording
{
  ResetLoop(slotIdx);
  aSlots[slotIdx].slotStatus = eLooperRecording;
  RefreshDisplay();
}

//## Button 3 : Change looper mode (Auto/manual ack)
void changeLooperModeCb(byte button, tButtonStatus event, int duration) //Change mode
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
void slotSelectCb (byte knob, int value, tKnobRotate rot)
{
  byte v = MAX_SLOTS - (value * MAX_SLOTS/1024) - 1;
  
  if (v == slotIdx)
    return;
  
  slotIdx = v;
  RefreshDisplay();
}


#ifdef _DEBUG
void dumpLoopCb(byte button, tButtonStatus event, int duration)
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


  if (!aSlots[slotIdx].sampleSize) 
  {
    DisplayWriteStr("No Sample", 0, 0);
    delay(1000);
    return;
  }
  delay(1000);
  DisplayWriteStr("[ ]   n       ms", 0, 0);
  DisplayWriteStr("Dur. :        ms",       1, 0);
  for (i = 0; i < aSlots[slotIdx].sampleSize; i++)
  {
    //[4]   n67 1234ms
    //Dur. : 1200   ms
    DisplayWriteInt(i, 0, 1);
    DisplayWriteInt(aSlots[slotIdx].aNoteEvents[i].note, 0, 7);
    DisplayWriteInt(aSlots[slotIdx].aNoteEvents[i].time, 0, 10);
    DisplayWriteInt(aSlots[slotIdx].aNoteEvents[i].duration, 1, 7);
    delay(1500);
  }

  RefreshDisplay();
}
#endif

