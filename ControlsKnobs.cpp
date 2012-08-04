
/*

-- Knob :

On stocke la derniere valeur et la date de validation
A chaque loop, on regarde si on a regardé il y a longtemps et si sup a un seuil, on update
On calcule un delta pour dire si CW ou CCW
TODO : calibrer les boutons voir établir un max (0-1024 ?)

*/

#include "Arduino.h"
#include "Controls.h"


/***********************************
 *     Knobs configuration
 ***********************************/
#define KNOB_DELAY 100                 //delay between knobs value check
#define KNOB_MAX_CB 2                  //max number of registered cb for knobs
#define KNOB_COUNT 2                   //Number of knobs in config
byte aKnobPins[KNOB_COUNT] = {0, 1};   //pins used for knobs in config


typedef struct
{
  tKnobCb aCallbacks[KNOB_MAX_CB];
  byte pin;
  int curValue;  //actuel knob value
  int prevValue; //previous knob value
  int lastChange; //previous time of value change
  tKnobRotate orientation; //previous orientation
} tKnob;

//global vars
tKnob aKnobs[KNOB_COUNT];
int lastKnobsChecked = 0;

void ControlsSetupKnobs(int time)
{
  int i;

  for (i = 0; i < KNOB_COUNT; i++)
  {
    memset(&aKnobs[i], 0x00, sizeof(tKnob));
    aKnobs[i].pin = aKnobPins[i];
    aKnobs[i].curValue = analogRead(aKnobs[i].pin);
    aKnobs[i].prevValue = aKnobs[i].curValue;
    aKnobs[i].lastChange = time;
    memset(&aKnobs[i].aCallbacks, 0x00, (KNOB_MAX_CB+1)*sizeof(tKnobCb));
  }
  lastKnobsChecked = time;
}

void ControlsUpdateKnobs(int time)
{
  int i;

  if (time - lastKnobsChecked < KNOB_DELAY) //Too early for a new check
    return;
  lastKnobsChecked = time;
  
  for (i = 0; i < KNOB_COUNT; i++)
  {
    aKnobs[i].prevValue = aKnobs[i].curValue;
    aKnobs[i].curValue = analogRead(aKnobs[i].pin);

    //Change
    if (aKnobs[i].prevValue != aKnobs[i].curValue)
    {
      int j = 0;
      
      //Orientation
      if (aKnobs[i].prevValue < aKnobs[i].curValue)
        aKnobs[i].orientation = eKnobOrientationCW; 
      else
        aKnobs[i].orientation = eKnobOrientationCCW;
      
      aKnobs[i].lastChange = time;
      while ((j < KNOB_MAX_CB) && (aKnobs[i].aCallbacks[j] != NULL))
      {
        aKnobs[i].aCallbacks[j](i, aKnobs[i].curValue, aKnobs[i].orientation);
        j ++;
      }
    }
  }
}

//Registers callback for Knobs value change (returns -1 on error)
int ControlsRegisterKnobCallback(byte knob, tKnobCb callback)
{
  int i;
  if ((knob < 0) || (knob >= KNOB_COUNT))
    return -1;
  
  
  for (i = 0; i < KNOB_MAX_CB; i++)
  {
    if (!aKnobs[knob].aCallbacks[i])
    {
      aKnobs[knob].aCallbacks[i] = callback;
      return 0;
    }
  }
  return -1; //Too many cb for this knob (increase KNOB_MAX_CB)
}

void ControlsNotifyKnob(byte knob)
{
  int j = 0;
  if ((knob < 0) || (knob >= KNOB_COUNT))
    return ;
    
  while ((j < KNOB_MAX_CB) && (aKnobs[knob].aCallbacks[j] != NULL))
  {
    aKnobs[knob].aCallbacks[j](knob, aKnobs[knob].curValue, eKnobOrientationNone);
    j ++;
  }
}
