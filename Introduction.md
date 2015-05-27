Moopz is a MIDI Looper project.

# Introduction #

Moopz is based on Arduino platform and includes a MIDI Shield and LCD screen.
The looper analyzes MIDI protocol, detects loops and plays them automatically.

The project is still under early development stages and many features are still missing !

# Details #
Moopz a is a MIDI looper : you plug a MIDI keyboard and a sound module; played loops ont the keyboard will be repeated.
You can play up to 4 tracks and control (play/record/stop) them separately.

The killer feature of this project is loop detection. No need to press a button at the very precise end of your sample, the Arduino detects the right time for you.
It's easier to use, especially for live shows !

## Auto Mode ##
In automatic mode, the looper will automatically decide to replay the recorded loop when the second note of the played sample is found.

**Example :**
You loop is A B C D A B C D A B C D  ...
The looper, set in "Record mode" will start to play at : "A B C D A B", and then play "C"
You can now stop playing, use another instrument, add another part or go for a break ;) !


## Manual Mode ##
If you play a more "complex" melody, automatic mode will sometime fail.

**Example :**
You loop is A B C A B D A B C A B D A B C A B D ...
In automatic mode, the looper would start after the second "B", missing the "D" note.

In manual mode, the looper displays a message once a loop has been found and waits for a button press.
The looper will always remain the biggest loop found.


## Incoming features ##
  * Improve loops detection (support of chords detection)
  * Detect BPM