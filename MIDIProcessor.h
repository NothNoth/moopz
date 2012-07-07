#include "Arduino.h"


typedef byte (* tMIDINoteCb) (byte channel, byte note, byte velocity, byte onOff) ;



void MIDIProcessorSetup();
void MIDIProcessorUpdate();
void MIDIRegisterNoteCb(tMIDINoteCb callback);
