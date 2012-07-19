#include <LiquidCrystal.h>
#include "Display.h"

#define GREEN_PIN 6
#define RED_PIN   7
LiquidCrystal lcd(5, 8, 9, 10, 11, 12); //RS, Enable, d0, d1, d2, d3
#include "Arduino.h"

#define BLINK_DELAY 100
int redTime   = 0;
int greenTime = 0;

void DisplaySetup()
{
  //Setup LCD
  lcd.begin(16, 2);
  lcd.home();
  
  //Setup LEDs
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);
  digitalWrite(GREEN_PIN, HIGH);
  digitalWrite(RED_PIN,   HIGH);
  DisplayBlinkRed();
  DisplayBlinkGreen();
}



void DisplayUpdate()
{
  int t = millis();
  if (redTime && (t - redTime > BLINK_DELAY))
  {
    redTime = 0;
    digitalWrite(RED_PIN, HIGH);  
  }
  if (greenTime && (t - greenTime > BLINK_DELAY))
  {
    greenTime = 0; 
    digitalWrite(GREEN_PIN, HIGH);  
  }
}

void DisplayBlinkRed()
{
  redTime = millis();
  digitalWrite(RED_PIN, LOW);  

}
void DisplayBlinkGreen()
{
  greenTime = millis();
  digitalWrite(GREEN_PIN, LOW);  
}

void DisplayClear()
{
  lcd.clear();
}

void DisplayWriteStr(const char * str, byte line, byte col)
{
  lcd.home();
  //lcd.setCursor(line, col); //Row / col
  lcd.setCursor(col, line); //Row / col
  lcd.print(str);
}

void DisplayWriteInt(int  val, byte line, byte col)
{
  lcd.home();
  //lcd.setCursor(line, col); //Row / col
  lcd.setCursor(col, line); //Row / col
  lcd.print(val);
}
