# Moopz is a MIDI Looper project.

##Introduction
Moopz is based on Arduino platform and includes a MIDI Shield and LCD screen. The looper analyzes MIDI protocol, detects loops and plays them automatically.

The project is still under early development stages and many features are still missing !

##Details
Moopz a is a MIDI looper : you plug a MIDI keyboard and a sound module; played loops ont the keyboard will be repeated. You can play up to 4 tracks and control (play/record/stop) them separately.

The killer feature of this project is loop detection. No need to press a button at the very precise end of your sample, the Arduino detects the right time for you. It's easier to use, especially for live shows !

###Auto Mode
In automatic mode, the looper will automatically decide to replay the recorded loop when the second note of the played sample is found.

__Example :__ Your loop is A B C D A B C D A B C D ... The looper, set in "Record mode" will start to play at : "A B C D A B", and then play "C" You can now stop playing, use another instrument, add another part or go for a break ;) !

###Manual Mode
If you play a more "complex" melody, automatic mode will sometime fail.

Example : You loop is A B C A B D A B C A B D A B C A B D ... In automatic mode, the looper would start after the second "B", missing the "D" note.

In manual mode, the looper displays a message once a loop has been found and waits for a button press. The looper will always remain the biggest loop found.

##Incoming features

* Improve loops detection (support of chords detection)
* Detect BPM


# Getting started

## Materials
To build your Moopz device, you will need :

* An Arduino (tested on Arduino Uno, should work well with others)
* A MIDI shield ( http://www.sparkfun.com/products/9595 )
* An 16x2 LCD screen based on HD44780 (ex : http://www.sparkfun.com/products/255 )
* Optionally a Protoshield to solder the LCD ( http://www.sparkfun.com/products/7914 )

Total price : 30$ + 20$ + 14$ + 15$ = 79$

## Pins
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

* D5 : RS
* D8 : Enable
* D9 : DB0
* D10 : DB1
* D11 : DB2
* D12 : DB3
* GND : VSS
* GND : DB4 to DB7
* +5V : VDD

Solder a 10k potentiometer between V0, ground and +5V to adjust contrast. If you have a backlight on you LCD, do not forget to add a resistor or it will burn !
Unused pins :

* D13
* D14
* A2
* A3
* A4
* A5

## Program
Checkout last release from this project subversion.

On the MIDI shield, turn switch to "PROG" . The arduino flash uses Rx and Tx lines, also used by the MIDI Shield. To avoid conflicts, this switch must always be switch to "Prog" when programming, then to "Run" for using and testing the program.

Flash the arduino. The Arduino should restart : Red and Green leds are turned on and the LCD shows :

    - Moop'z -
    > Starting ...

Turn the MIDI switch to "RUN".



# Moopz user's guide

## LCD Screen
LCD display is separated into 5 parts :

    <General Status>        <LooperMode>
    <Slot Id>  <Message>   <Slot Status>

"General Status" tells you if the looper is actually playing something. It can be "Play" or "Idle". In Play mode, filled slots will be played (except muted and empty slot). You can use button 1 to switch between these status.

"Looper Mode" show the looper mode : Auto or Manual (Man). In Auto mode, detected loops will be automatically played if the first 2 notes of the sample are repeated. In manual mode, the looper stores le longest loop found and waits for a manual acknowledge. You can use button 3 to switch between these modes. "Slot Id" show the current slot selected (1-4). You can change slot by using Knob 1.

"Message" is a temporary message for the selected slot. It can tells is a loop is found, an error occurred, etc. The message will be shown for 2 seconds.

"Slot Status" tells you is the selected slot is Empty/Played/Recording/Muted. You can switch between the slot status by using button 2.

## Buttons

Moopz is using 3 buttons :

- Button 1 : Pressing this button will switch between Play and Idle mode. In play mode, slots in "play status" will be played. Empty, and Muted slots will be ignored. In idle mode, the looper remains silents (and the looper is a simple passtrough box).

- Button 2 : Pressing this button changes the status of the current slot. The effect of this button depends on the current status. With "Empty" status, this button has no effect. With "Muted" status, this button will switch slot to "Play". With "Play" status, this button will switch slot to "Muted". With "Recording" status, and in "Manual" mode, this button will start playing the last loop found.

- Button 2 (press for 1s) : By remaining pressed for 1s (or more) on this button, current slot will be switched to "Recording" status. The looper will listen to MIDI notes played and will try to detect loops. In "Auto" mode, detected loop will be played immediately (slot switches to "Play" status) and in manual mode, it will wait for a manual ack (short press on button 2).

- Button 3 : Switch between Auto and Manual mode. Current mode is display at the top right of the LCD screen.

## Knobs

Knob 1 can be used to switch between slots (1-4). Current slot is displayed at the bottom left of the LCD screen (ex : "Sl3").

### Example 1 : 3 notes loop in auto mode on slot 1
1 : Press button 3 to select "Auto" (selected by default) mode at the top right of the screen 2 : Turn knob 1 to select Slot 1 ("Sl1" at the bottom left of the screen)

The LCD now displays :

    Idle             Auto
    Sl1             Empt

3 : Press button 2 for 1s. LCD screen shows :

    Idle             Auto
    Sl1               Rec.

4 : Play a 3 notes sequences two times. At the second note of the second sample, looper will start to play :

    Play             Auto
    Sl1               Play

5 : Press button 2, the loop is muted :

    Play             Auto
    Sl1              Mute

6 : Press it again, the loops is played :

    Play             Auto
    Sl1               Play

7 : Press button 1, the looper remains silent :

    Idle             Auto
    Sl1               Play

2 : Press button 1 again, the loop in slot1 is played :

    Play             Auto
    Sl1               Play

### Example 2 : add a loop in manual mode on slot2

1 : Process with Example 1

2 : Use knob to select slot 2 :

    Play             Auto
    Sl2              Empt

3 : Press button 3 to switch to "Manual" mode :

    Play              Man
    Sl2              Empt

4 : Press button 2 for 1s :

    Play              Man
    Sl2                Rec.

5 : Play a loop on your keyboard. You can use this manual mode to play more complex melodies (where first two notes of the sample are repeated). If a loop is found, the screen shows ;

    Play              Man
    Sl2 LoopRdy  Rec.

6 : You can now press button 2 to start playing this loop.

    Play              Man
    Sl2                Play

7 : You now have two loops playing at the same time. You can switch between slots by using the knob 1, play/mute them separately by using button 2, etc.
