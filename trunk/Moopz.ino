#include "Controls.h"

#include <LiquidCrystal.h>
#include "Display.h"
#include "MIDIProcessor.h"
#include "Looper.h"

/*
 
 
  MIDI shield :
    #Digital
    - Midi IN : D0 Rx
    - Midi OUT : D1 Tx
    - button : D2
    _ button : D3
    - button : D4
    
    - LED : D6
    - LED : D7
    
    #Analog
    _ knob : A0
    - knob : A1

    LCD (protoshield) :
    - RS : D5
    - Enable : D8
    - 0 : D9
    - 1 : D10
    - 2 : D11
    - 3 : D12
    
    Unused :
    - D13
    - D14
    
*/

void knobSample (int knob, int value, tKnobRotate rot)
{
  DisplayWriteStr("> Knob [ ]      ", 0, 1);  
  DisplayWriteInt(knob, 8, 1);  
  DisplayWriteInt(value, 12, 1);
}



void setup()
{
  DisplaySetup("- Moopz -");
  DisplayWriteStr("> Starting...", 0, 1);
  delay(2000);

  MIDIProcessorSetup();

  ControlsSetup();


//  ControlsRegisterKnobCallback(0, knobSample);
//  ControlsRegisterKnobCallback(1, knobSample);
  
  
  LooperSetup();
  DisplayWriteStr("   Ready !", 0, 1);
}

void loop()
{
  ControlsUpdate(); 
  MIDIProcessorUpdate();
  LooperUpdate();

  DisplayUpdate();
}
