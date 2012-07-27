#include "Arduino.h"


typedef byte (* tMIDINoteCb) (byte channel, byte note, byte velocity) ;



void MIDIProcessorSetup();
void MIDIProcessorUpdate();
void MIDIRegisterNoteCb(tMIDINoteCb callback);
