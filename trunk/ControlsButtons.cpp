
#include "Arduino.h"
#include "Controls.h"
#include "Display.h"



/*
-- Buttons :
On emet un signal différent pour press et release
A l'enregistrement de cb, on peut préciser press/release et préciser un temps de release-press
*/

/***********************************
 *     Buttons configuration
 ***********************************/
#define BUTTON_DELAY  200                    //delay between buttons value check
#define BUTTON_MAX_CB 4                      //max number of registered cb for buttons
#define BUTTON_COUNT  3                      //Number of buttons in config
byte aButtonPins[BUTTON_COUNT] = {2, 3, 4};  //pins used for buttons in config    //TODO : parameter for ButtonsSetup ?



typedef struct
{
  tButtonStatus event;     //Press or Release
  int           duration; //For how long ?
  tButtonCb     callback;  //Cb
} tButtonCallback;

typedef struct
{
  byte              pin;
  tButtonStatus     btStatus;
  int               timePressed;
  tButtonCallback   aCallbacks[BUTTON_MAX_CB + 1];
} tButton;


tButton aButtons[BUTTON_COUNT];
int lastButtonChecked = 0;


void ControlsSetupButtons(int time)
{
  int i;

  //Init buttons
  for (i = 0; i < BUTTON_COUNT; i++)
  {
    memset(&aButtons[i], 0x00, sizeof(tButton));
    aButtons[i].pin = aButtonPins[i];
    pinMode((int) aButtons[i].pin, INPUT_PULLUP);
    memset(&aButtons[i].aCallbacks, 0x00, (BUTTON_MAX_CB+1)*sizeof(tButtonCallback));
  }
  lastButtonChecked = time;
}

void ControlsUpdateButtons(int time)
{
  int i;
  //Update Buttons
  
  if (time - lastButtonChecked < BUTTON_DELAY)  //Too early for a new check
    return;
  lastButtonChecked = time;

  
  for (i = 0; i < BUTTON_COUNT; i++)
  {
    tButtonStatus btVal = (digitalRead(aButtons[i].pin) == HIGH)?eButtonStatus_Released:eButtonStatus_Pressed;
   
    if (aButtons[i].btStatus != btVal) //button has changed
    {
      int j;
    DisplayBlinkGreen();

      tButtonStatus st = eButtonStatus_None;
      int timePressed = 0;
        
      if ( (aButtons[i].btStatus == eButtonStatus_Pressed) && //Release
           (btVal == eButtonStatus_Released) )
      {
        timePressed = time - aButtons[i].timePressed;
        aButtons[i].btStatus = eButtonStatus_Released;
        st = eButtonStatus_Released;
      }
      else //Press
      {
        aButtons[i].timePressed = time;
        aButtons[i].btStatus = eButtonStatus_Pressed;
        st = eButtonStatus_Pressed;
      }
        
      //Process callbacks for this button
      j = 0;
      while ((j < BUTTON_MAX_CB) && aButtons[i].aCallbacks[j].callback)
      {
        if (aButtons[i].aCallbacks[j].event == st)
        {
          if (st == eButtonStatus_Pressed)
          {
            aButtons[i].aCallbacks[j].callback(i, aButtons[i].aCallbacks[j].event, aButtons[i].aCallbacks[j].duration);
          }
          else
          {
            if (!aButtons[i].aCallbacks[j].duration) //no duration specified
              aButtons[i].aCallbacks[j].callback(i, aButtons[i].aCallbacks[j].event, aButtons[i].aCallbacks[j].duration);
            else if (aButtons[i].aCallbacks[j].duration < timePressed)
              aButtons[i].aCallbacks[j].callback(i, aButtons[i].aCallbacks[j].event, aButtons[i].aCallbacks[j].duration);
          }              
        }
        j++;
      }
    }
  }
}



//Registers callback for Button press/release (returns -1 on error)
int ControlsRegisterButtonCallback(int button, tButtonStatus event, int duration, tButtonCb callback)
{
  int i;
  if ((button < 0) || (button >= BUTTON_COUNT))
    return -1;
  
  if ((event == eButtonStatus_Pressed) && (duration))
    return -1; // invalid settings
    
  for (i = 0; i < BUTTON_MAX_CB; i++)
  {
    if (!aButtons[button].aCallbacks[i].callback)
    {
      aButtons[button].aCallbacks[i].callback = callback;
      aButtons[button].aCallbacks[i].event = event;
      if (event == eButtonStatus_Released)
        aButtons[button].aCallbacks[i].duration = duration;

      return 0;
    }
  }
  
  return -1; //Too many cb for this knob (increase KNOB_MAX_CB)
}


