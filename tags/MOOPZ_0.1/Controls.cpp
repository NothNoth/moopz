#include "Arduino.h"
#include "Controls.h"


void ControlsSetup()
{
  int initTime = millis();

  ControlsSetupKnobs(initTime);
  ControlsSetupButtons(initTime);
}

void ControlsUpdate()
{
  int updateTime = millis();
  
  ControlsUpdateKnobs(updateTime);
  ControlsUpdateButtons(updateTime);
}

