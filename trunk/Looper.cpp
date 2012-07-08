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
  byte note;
  byte velocity;  
} tNoteEvent;


#define MAX_SAMPLE 64
tNoteEvent aNoteEvents[MAX_SAMPLE];
tLooperMode   looperMode;
tLooperStatus looperStatus;

byte noteIdx;
bool note1Ok;
byte sampleSize;

byte replayIdx;
int replayTimer;

void changeLooperModeCb(int button, tButtonStatus event, int duration); //Auto/Manual
void changeLooperStatusCb(int button, tButtonStatus event, int duration); //Start Recording/Manual play loop

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

  
  memset(aNoteEvents, 0x00, MAX_SAMPLE*sizeof(tNoteEvent));
  ResetLoop();
  SetMode(eLooperAuto);
  SetStatus(eLooperIdle);
}

void LooperUpdate()
{
  if (looperStatus != eLooperPlaying) //Nothing to do
    return;
      DisplayBlinkGreen();

  //TODO : replay
  
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
  if (noteIdx > 2)
  {
    if (!note1Ok) //Check note 1 => A B C D + A
    {
      if (note == aNoteEvents[0].note) //Current note is first note of sample
        note1Ok = true;
    }
    else //First note already matched, second note ?  => A B C D A + B
    {
      //TODO : also check time elapsed between Note1/Note0 and millis() = aNoteEvents[noteIdx-1] + epsilon;
      if (note == aNoteEvents[1].note) //2nd note also matches --> Loooop !!
      {
        if (looperMode   == eLooperAuto)
        {
          sampleSize = noteIdx -1; //Remove previous note (1st note repeated) => A B C D
          looperStatus = eLooperPlaying;
          DisplayWriteStr("AutoLooping !", 0, 1);
          ResetPlay();
          return true; //Do not play this note
        }
        else
        {
          //TODO : Euuh ok, on fait comment le mode manuel .. ?
          //=> Stocker la dernière bonne taille de pattern connue continuer a remplir le aNoteEvents
          // comme si de rien n'était ...
        }
      }
      else
      {
        // A B C D A E
        note1Ok = false; //restart from scratch
      }
    }
  }

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
  note1Ok = false;
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
