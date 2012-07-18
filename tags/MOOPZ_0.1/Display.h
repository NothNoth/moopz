#include "Arduino.h"
void DisplaySetup(const  char * helloStr);
void DisplayUpdate();


void DisplayBlinkRed();
void DisplayBlinkGreen();

void DisplayWriteStr(const char * str, byte line, byte col);
void DisplayWriteInt(int val,          byte line, byte col);
void DisplayClear();
