#include "MIDIProcessor.h"
#include "Controls.h"
#include "Display.h"


#define _DEBUG
#define CHORD_DELAY 10 //x ms max between notes of a same chord

#define SET_CHORD_FLAG(_n) ((_n).velocity = (_n).velocity | 0x80)
#define IS_CHORD(_n)       (((_n).velocity&0x80 == 0x00)   ?false : true)
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
} tNoteEvent;


#define MAX_SAMPLE 32 //Max events in sample (32 notes)
#define MAX_SLOTS 4
byte slotIdx = 0;

typedef struct
{
  tNoteEvent aNoteEvents[MAX_SAMPLE];
  
  byte noteIdx;
  byte sampleSize;
  
  byte replayIdx;
  unsigned long  replayTimer;
  unsigned long  recordTimer;
  byte bChannel;
  tLooperStatus slotStatus;
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
  byte i;
  
  if (looperStatus != eLooperPlaying) //Nothing to do
    return;
  unsigned long timestamp = millis();
  
  // Play recorded loops
  for (i = 0; i < MAX_SLOTS; i++)
  {
    tLooperSlot * slot = &aSlots[i];
    if (slot->sampleSize && ((slot->slotStatus == eLooperPlaying)||(slot->slotStatus == eLooperIdle)))
    {
      //A (0ms)  B (10ms) C (5ms) D (1ms) E (100ms) A (0ms) ...
      //Increase timers ont muted slots to keep sync
      if ((int)(timestamp - slot->replayTimer) >= slot->aNoteEvents[slot->replayIdx].time)
      {
        if (slot->slotStatus == eLooperPlaying)
        {
          Serial.write((IS_NOTE_OFF(slot->aNoteEvents[slot->replayIdx])?0x80:0x90) | slot->bChannel);
          Serial.write(slot->aNoteEvents[slot->replayIdx].note);
          Serial.write(slot->aNoteEvents[slot->replayIdx].velocity << 1);
        }
        slot->replayTimer = timestamp;
        slot->replayIdx ++;
      }
      if (slot->replayIdx == slot->sampleSize) slot->replayIdx = 0;
    }
  }
  if (displayTimeout && ((int)(timestamp - displayTimeout) > 2000))
    RefreshDisplay();
}

//True : Match on a least 2 NoteOn or 2 chords at specified offsets
//False : No match
byte EventsCompare(tLooperSlot * slot, byte idx1, byte idx2)
{
  //Quick checks to reject trivial cases
  if ((IS_NOTE_OFF(slot->aNoteEvents[idx1])) || //No check on NoteOff
      ((IS_NOTE_OFF(slot->aNoteEvents[idx2]))) || //No check on NoteOff
      ((IS_CHORD(slot->aNoteEvents[idx1])) && (!IS_CHORD(slot->aNoteEvents[idx2]))) || //idx1 is a chord and not idx2
      ((IS_CHORD(slot->aNoteEvents[idx2])) && (!IS_CHORD(slot->aNoteEvents[idx1]))) ||
      (slot->aNoteEvents[idx1].note != slot->aNoteEvents[idx2].note)) //First note differs
    return false;
  
  byte nbCommon = 0;
  //A B CDE DE A B CDE DE

  while (idx2 < slot->noteIdx)
  { 
    //jump to next NoteOn event
    while ((idx1 < idx2) && (IS_NOTE_OFF(slot->aNoteEvents[idx1])))
      idx1++;
    if (idx1 == idx2) break;
    while ((idx2 < slot->noteIdx) && (IS_NOTE_OFF(slot->aNoteEvents[idx2])))
      idx2++;
    if (idx2 == slot->noteIdx) break;

    //Same note (on single note or same chord)
    if ((slot->aNoteEvents[idx1].note == slot->aNoteEvents[idx2].note) && (IS_CHORD(slot->aNoteEvents[idx1]) == IS_CHORD(slot->aNoteEvents[idx2])))
    { 
      if (!idx1 || (slot->aNoteEvents[idx1].time != slot->aNoteEvents[idx1-1].time)) //increase just once for chords
        nbCommon ++;
    }
    else
    {
      //No match, stop here
      break;
    }
    //Match : try next idx
    idx1 ++;
    idx2 ++;
  }

  //Checks common notes found.
  if (nbCommon >= 2)
    return true;
  else
    return false;
}

//Updates "sampleSIze" to the longest loop found on given slot
//Returns current position in loop in that case or 0xFF if no loop is detected
byte LoopDetect(tLooperSlot * slot)
{
  byte loopStartIdx;
  //A B CDE DE A B CDE DE
  //Not very optimal, rechecks all from the beginning at every note ..
  //Anyway, it gives us a simpler algorithm with less memory usage (less states to remember)

  loopStartIdx = slot->noteIdx - 3;
  while (loopStartIdx > 1) //Shorter sample will be too short (Size 0 or 1)
  {
    if (EventsCompare(slot, 0, loopStartIdx)) //Found loop
    {
      slot->sampleSize = loopStartIdx + 1;
      return slot->noteIdx - slot->sampleSize;
    }
    loopStartIdx --; //Try a shorter loop
  }
  return 0xFF; //Nothing found
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
    RefreshDisplay();
  }
  
  
  slot->aNoteEvents[slot->noteIdx].note     = note;
  slot->aNoteEvents[slot->noteIdx].time     = slot->noteIdx?(int)(timestamp - slot->recordTimer):0;
  slot->aNoteEvents[slot->noteIdx].velocity = velocity >> 1;
  slot->recordTimer = timestamp;

  //This event is very close from the previous one => this is a chord.
  if (slot->noteIdx && (slot->aNoteEvents[slot->noteIdx].time < CHORD_DELAY) && (slot->aNoteEvents[slot->noteIdx-1].note != slot->aNoteEvents[slot->noteIdx].note))
  {
    bool ordered = false;
    byte i;
    
    slot->aNoteEvents[slot->noteIdx].time = 0;
    slot->aNoteEvents[slot->noteIdx].time = slot->aNoteEvents[slot->noteIdx - 1].time; //Set time at the same value of the previous note for the same chord
    SET_CHORD_FLAG(slot->aNoteEvents[slot->noteIdx]);
    SET_CHORD_FLAG(slot->aNoteEvents[slot->noteIdx - 1]);

    
    //Reorder notes from a chord (for easier and faster loop detection)
    //Dummy bubble sort, trying to "optimize" anything here would use more RAM ..
    while (!ordered)
    {
      ordered = true;
      for (i = 0; i < slot->noteIdx - 1; i++)
      {
        if ((slot->aNoteEvents[i].time == slot->aNoteEvents[i+1].time) &&
            (slot->aNoteEvents[i].note > slot->aNoteEvents[i+1].note))
        {
          tNoteEvent evt;
          memcpy(&evt, &slot->aNoteEvents[i], sizeof(tNoteEvent));
          memcpy(&slot->aNoteEvents[i], &slot->aNoteEvents[i+1], sizeof(tNoteEvent));
          memcpy(&slot->aNoteEvents[i+1], &evt, sizeof(tNoteEvent)); 
          ordered = false;         
        }
      }
    }
  }
  
  slot->noteIdx ++;  
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
   
  DisplayBlinkGreen();
  if (!AddNote(slot, note, velocity, channel, timestamp))
  {
    //Cannot add another note on slot
    //-> sample must be too long
    slot->slotStatus = eLooperIdle;
    RefreshDisplay((char*)"Too long !");
    ResetLoop(slotIdx);
    return false;
  }
  
  if (!velocity) //NoteOff is not used to setup a loop
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
    DisplayWriteInt(aSlots[slotIdx].bChannel, 0, (aSlots[slotIdx].bChannel>9)?9:10);
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

  
  if (aSlots[slotIdx].sampleSize) DisplayWriteStr("[ ]   n       ms", 0, 0);
  for (i = 0; i < aSlots[slotIdx].sampleSize; i++)
  {
    //[4] R n67 1234ms
    DisplayWriteInt(i, 0, 1);
    DisplayWriteStr(IS_NOTE_OFF(aSlots[slotIdx].aNoteEvents[i])?"R":"P", 0, 4);
    DisplayWriteInt(aSlots[slotIdx].aNoteEvents[i].note, 0, 7);
    DisplayWriteInt(aSlots[slotIdx].aNoteEvents[i].time, 0, 10);
    delay(1000);
  }
  delay(1000);
  RefreshDisplay();
}
#endif

