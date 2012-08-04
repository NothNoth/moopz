#include "Arduino.h"


typedef byte (* tMIDINoteCb) (byte channel, byte note, byte velocity, unsigned long timestamp) ;



void MIDIProcessorSetup();
void MIDIProcessorUpdate(unsigned long timestamp);
void MIDIRegisterNoteCb(tMIDINoteCb callback);
