#include "Arduino.h"
void DisplaySetup();
void DisplayUpdate();


void DisplayBlinkRed();
void DisplayBlinkGreen();

void DisplayCreateChar(byte array[8], byte id);
void DisplayWriteChar(byte id, byte line, byte col);

void DisplayWriteStr(const char * str, byte line, byte col);
void DisplayWriteInt(int val,          byte line, byte col);
void DisplayClear();
