#include "Arduino.h"
void DisplaySetup(const  char * helloStr);
void DisplayUpdate();


void DisplayBlinkRed();
void DisplayBlinkGreen();

void DisplayWriteStr(const char * str, byte line = -1, byte col = -1);
void DisplayWriteInt(int val, byte line = -1, byte col = -1);
void DisplayClear();
