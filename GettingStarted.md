# Building your Moopz device #

## Materials ##
To build your Moopz device, you will need :
  * An Arduino (tested on Arduino Uno, should work well with others)
  * A MIDI shield ( http://www.sparkfun.com/products/9595 )
  * An 16x2 LCD screen based on HD44780 (ex : http://www.sparkfun.com/products/255 )
  * Optionally a Protoshield to solder the LCD ( http://www.sparkfun.com/products/7914 )
  * 
Total price : 30$ + 20$ + 14$ + 15$ = 79$

## Pins ##
The MIDI shield will use the following pins :
  * D0 Rx : MIDI IN
  * D1 Tx : MIDI OUT
  * D2 : Button 3
  * D3 : Button 2
  * D4 : Button 1
  * D6 : Green LED
  * D7 : Red LED
  * A0 : Knob 1
  * A1 : Knob 2

Solder the LCD screen on the Protoshield using the following PINs :
  * D5  : RS
  * D8  : Enable
  * D9  : DB0
  * D10 : DB1
  * D11 : DB2
  * D12 : DB3
  * GND : VSS
  * GND : DB4 to DB7
  * +5V : VDD
Solder a 10k potentiometer between V0, ground and +5V to adjust contrast.
If you have a backlight on you LCD, do not forget to add a resistor or it will burn !

Unused pins :
  * D13
  * D14
  * A2
  * A3
  * A4
  * A5

## Program ##
Checkout last release from this project subversion.

On the MIDI shield, turn switch to "PROG" . The arduino flash uses Rx and Tx lines, also used by the MIDI Shield. To avoid conflicts, this switch must always be switch to "Prog" when programming, then to "Run" for using and testing the program.

Flash the arduino.
The Arduino should restart : Red and Green leds are turned on and the LCD shows :
```
      - Moop'z -
> Starting ...
```

Turn the MIDI switch to "RUN".