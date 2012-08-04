#include "Arduino.h"

typedef enum
{
  eKnobOrientationNone = 0,
  eKnobOrientationCW,
  eKnobOrientationCCW
} tKnobRotate;

 typedef enum
 {
   eButtonStatus_None = 0,
   eButtonStatus_Pressed,
   eButtonStatus_Released  
 } tButtonStatus;
 
void ControlsSetup();
void ControlsUpdate();


typedef void (* tButtonCb) (byte button, tButtonStatus event, int duration) ;
typedef void (* tKnobCb) (byte knob, int value, tKnobRotate rot) ;

void ControlsSetupKnobs(int time);
void ControlsUpdateKnobs(int time);
void ControlsSetupButtons(int time);
void ControlsUpdateButtons(int time);

int ControlsRegisterButtonCallback(byte button, tButtonStatus event, int duration, tButtonCb callback);
int ControlsRegisterKnobCallback(byte knob, tKnobCb callback);

void ControlsNotifyKnob(byte knob); //Force callback for knob 

