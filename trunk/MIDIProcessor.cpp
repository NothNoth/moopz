#include "MIDIProcessor.h"
#include "Display.h"
#include "Arduino.h"


typedef struct
{
  byte bStatus;        //Which command
  byte bChannel;       //bChannel (optional)
  byte aData[3];       //Max 3 aData bytes allowed
  byte bBytesPending;  //How many aData bytes to read ? 
  byte bBytesRead;     //Bytes already read
} tMIDICommand;
tMIDICommand stCurrent;  //Current MIDI command
tMIDICommand stRunning; //Previous MIDI command (for running status)
boolean bIgnoredCommand;

tMIDINoteCb  pfNoteCb;   //Callback for NoteOn/Off commands

void MIDIRead();
boolean ReadStatus(byte b);
boolean ReadData(byte b);


void MIDIProcessorSetup()
{
  Serial.begin(31250);
  pfNoteCb = NULL;
  memset(&stCurrent,  0x00, sizeof(tMIDICommand)); //Reset current command
  memset(&stRunning, 0x00, sizeof(tMIDICommand)); //Reset previous command
  bIgnoredCommand = false;
}


void MIDIProcessorUpdate()
{
  if (Serial.available())
    MIDIRead();
}



boolean ReadStatus(byte b)
{
  byte bStatus;
  byte bChannel;
  memset(&stCurrent, 0x00, sizeof(tMIDICommand)); //Reset stCurrentrent command
    
  if ((b & 0xF0) == 0xF0) //System messages (F0 - FF)
  {
    bStatus = b;
    bChannel = 0; //No bChannel
    bIgnoredCommand = true;

#ifdef _DEBUG
    DisplayWriteStr("### Sys:      ###", 0, 0);
    DisplayWriteInt(bChannel, 8, 0);
    DisplayWriteInt(st, 12, 0);
#endif
  }
  else
  {
    bStatus = b>>4; //Status is first 4 bits
    bChannel = b & 0x0F;  // bChannel is next 4 bits
    bIgnoredCommand = false;

#ifdef _DEBUG

    DisplayWriteStr("### Sts:      ###", 0, 1);
    DisplayWriteInt(bChannel, 8, 1);
    DisplayWriteInt(bStatus, 12, 1);
#endif
  }
      
  stCurrent.bStatus  = bStatus;
  stCurrent.bChannel = bChannel;

  switch (bStatus)
  {
    //Channel dependant messages
    case 0x08: //Note Off
      stCurrent.bBytesPending = 2; //Note, Velocity
    break;
    case 0x09: //Note On
      stCurrent.bBytesPending = 2; //Note, Velocity
    break;
    case 0x0A: //AfterTouch
      stCurrent.bBytesPending = 2; //Note, pressure
    break;
    case 0x0B: // Ctrl Change
       stCurrent.bBytesPending = 2; //Controller Number, value
    break;
    case 0x0C: //Program Patch
      stCurrent.bBytesPending = 1; //Prog number
    break;
    case 0x0D: //Channel Pressure
      stCurrent.bBytesPending = 1; //Pressure amount
    break;
    case 0x0E: //Pitch
      stCurrent.bBytesPending = 2; //14 bits val : pitch value (0X2000 centered)
    break;
        
    //System Common
    case 0xF0: //Begin SysEx msg
    case 0xF1:
    case 0xF2:
    case 0xF3:
    case 0xF4: //Unused
    case 0xF5: //Unused
    case 0xF6:
    case 0xF7: //End SysEx msg
      //DisplayWriteStr("Sys Co", 9, 1);
    break;
        
    //System Realtime
    case 0xF8:
    case 0xF9: //Unused
    case 0xFA:
    case 0xFB:
    case 0xFC:
    case 0xFD: //Unused
    case 0xFE:
    case 0xFF:
      //DisplayWriteStr("Sys Rtm", 9, 1);
    break;
    default: 
      DisplayWriteStr("### WTF:      ###", 0, 1);
      DisplayWriteInt(bChannel, 8, 1);
      DisplayWriteInt(bStatus, 12, 1);
  }  

  return bIgnoredCommand;
}


boolean ReadData(byte b)
{
  boolean silent = false;
  int i;
  
  if (stCurrent.bBytesPending == 0) //New aData, no status Bytes : "running status" mode
    memcpy(&stCurrent, &stRunning, sizeof(tMIDICommand)); //Current MIDI status is previous One (status byte skipped)

  if (!stCurrent.bBytesPending) //Not waiting for anything (RealTime or unsupported..)
    return true;
    
  
  stCurrent.bBytesPending --;
  stCurrent.aData[stCurrent.bBytesRead] = b;

  if (stCurrent.bBytesPending) //Wait for other data bytes
  {
    stCurrent.bBytesRead ++; 
    return false; //Bufferize
  }
  
  //### MIDI Command completed !
  if (bIgnoredCommand) // All data bytes already replayed
      return true;
  
  //Callbacks
  if (pfNoteCb && ((stCurrent.bStatus == 0x09) || (stCurrent.bStatus == 0x08))) //Callback for Note On/Off
  {
      //Note Off or not On + velocity = 0 ==> false, else true
      silent = pfNoteCb( stCurrent.bChannel, 
                              stCurrent.aData[0], 
                              stCurrent.aData[1], 
                              ((stCurrent.bStatus == 0x08)||((stCurrent.bStatus == 0x09) && (stCurrent.aData[1] == 0x00)))?false:true);
  }
  

  Serial.write(0x9F);
  Serial.write(0xAA);
  Serial.write(0xAA);
  
  
  if (!silent) //Echo bufferized MIDI Command
  {
    DisplayBlinkGreen();
    Serial.write(stCurrent.bStatus << 4 | stCurrent.bChannel);
    for (i = 0; i < stCurrent.bBytesRead; i++)
      Serial.write(stCurrent.aData[i]);
  }
  
  //Ready for a new msg with same status (Running Status)
  stRunning.bStatus = stCurrent.bStatus;
  stRunning.bChannel = stCurrent.bChannel; 
  stRunning.bBytesRead = 0;
  stRunning.bBytesPending = stCurrent.bBytesRead + 1;
  memset(&stCurrent, 0x00, sizeof(tMIDICommand)); //Reset stCurrentrent command
  return false;
}

void MIDIRead()
{     
  byte passThrough = true; //Only for Unknown/dropped MIDI bytes
  byte b = Serial.read();

  if (b & 0x80) //Status Byte
  {
    passThrough = ReadStatus(b);
  }
  else //aData byte
  {
    passThrough = ReadData(b);
  }
    
  if (passThrough)
    Serial.write(b); //Echo input
}



void MIDIRegisterNoteCb(tMIDINoteCb callback)
{
  pfNoteCb = callback;
}

