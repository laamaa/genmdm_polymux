#include <RKPolyMux.h>
#include <RK002.h>

RKPolyMux polymux;
RKPolyMux psgmux;

RK002_DECLARE_INFO("GenMDM poly mode", "jonne.kokkonen@gmail.com", "1.0", "c89a53fe-841f-436f-83c7-b0db1023bf9c");

RK002_DECLARE_PARAM(FMCHANNEL, 1, 0, 16, 1)
RK002_DECLARE_PARAM(PSGCHANNEL, 1, 0, 16, 2)

//Enable debug messages
#define DEBUG true

// Define a specific value to detect if tables are already present in RK002 memory
#define FLASH_SIGNATURE 0xACDCABBA

// Define the amount of presets to store / range of the CC #116 (Select preset slot)
#define NUM_PRESETS 16

// Define memory table structure
struct
{
  uint32_t signature;
  byte preset[NUM_PRESETS][127];
} flashData;

// Global variables
byte fmChannel = 0;
byte psgChannel = 0;

bool psgModLfoActive = false;
bool psgModLfoDirectionUp = false;
byte psgModLfoDepth = 0;
byte psgModLfoSkipCycle = 0;

uint8_t currentPreset = 0;

// Read / initialize memory with lookup tables
void recallPresetsFromFlash()
{
  if (DEBUG) RK002_printf("recallPresetsFromFlash: Reading flash");
  int res = RK002_readFlash(0, sizeof(flashData), (byte*)&flashData);

  // init flash if readback failed:
  if ((res != 0) || (flashData.signature != FLASH_SIGNATURE))
  {
    if (DEBUG) RK002_printf("recallPresetsFromFlash: Reading flash failed");    
    uint8_t i, j;

    for (i = 0; i < NUM_PRESETS; i++)
    {
      for (j = 0; j < 128; j++)
      {
        flashData.preset[i][j] = 255;
      }
    }

    flashData.signature = FLASH_SIGNATURE;
  }
}

// Write table data to RK002 memory
void storePresetsInFlash()
{
  if (DEBUG) RK002_printf("storePresetsInFlash: Writing to flash");
  int res = RK002_writeFlash(0, sizeof(flashData), (byte*)&flashData);
  if (DEBUG && res != 0) RK002_printf("storePresetsInFlash: Write failed");

}

// Send current preset to GenMDM, store in GenMDM RAM for faster access
void sendPresetToDevice()
{
  if (DEBUG) RK002_printf("sendPresetToDevice: sending preset %d to device", currentPreset);
  uint8_t i, j;

  for (j = 0; j < 128; j++)
  {
    if (flashData.preset[currentPreset][j] != 255)
    {
      for (i = 0; i < 6; i++)
      {
        RK002_sendControlChange(i, j, flashData.preset[currentPreset][j]);
      }
    }
  }

  //Store preset in GenMDM's RAM for faster access.
  uint8_t genMdmPresetSlot = ((128 / 16) * currentPreset) - 1;
  RK002_sendControlChange(0, 6, genMdmPresetSlot);
}

// Send all presets to device
void sendAllPresetsToDevice()
{
  uint8_t i, j, k;

  // Iterate through all presets and send the CCs to all of our five FM channels
  for (k = 0; k < NUM_PRESETS; k++)
  {
    if (DEBUG) RK002_printf("sendAllPresetsToDevice: sending preset %d to device", k);
    for (j = 0; j < 128; j++)
    {
      if (flashData.preset[k][j] != 255)
      {
        for (i = 0; i < 6; i++)
        {
          RK002_sendControlChange(i, j, flashData.preset[k][j]);
        }
      }
    }
    //Store preset in GenMDM's RAM for faster access.
    uint8_t genMdmPresetSlot = ((128 / 16) * (k + 1)) - 1;
    RK002_sendControlChange(0, 6, genMdmPresetSlot);
  }
}

// POLYMUX RELATED:
// handles polymux engine midi output
void onFmPolyMuxOutput(void *userarg, byte polymuxidx, byte sts, byte d1, byte d2)
{
  byte chn = polymuxidx; // 'polymuxidx' is auto incremented by polymux

  if (chn <= 15)
  {
    sts |= chn; // Bitwise OR: channel will be added to message type 'sts'
    RK002_sendChannelMessage(sts, d1, d2);
  }
}

void onPsgPolyMuxOutput(void *userarg, byte polymuxidx, byte sts, byte d1, byte d2)
{
  byte chn = 6 + polymuxidx; // 'polymuxidx' is auto incremented by polymux

  if (chn <= 15)
  {
    sts |= chn; // Bitwise OR: channel will be added to message type 'sts'
    RK002_sendChannelMessage(sts, d1, d2);
  }
}

// Handler for ALL channel messages: used for adding messages to polymux engine
bool RK002_onChannelMessage(byte sts, byte d1, byte d2)
{

  // By default, do not send the message through
  bool thru = false;

  // FM channel handler
  // Check if we're receiving a message on our FM channel
  if ((sts & 0x0f) == fmChannel)
  {
    bool doPoly = true;

    // Check midi message type
    switch (sts & 0xf0) {

      // Handle control change messages
      case 0xB0:

        // Do not send the message to Polymux processor
        doPoly = false;

        // Store the message to our preset array (except if it's modwheel or preset related)
        if (d1 != 1 || d1 != 116 || d1 != 117 || d1 != 118)
        {
          flashData.preset[currentPreset][d1 - 1] = d2;
          return false;
        }

        // Select preset slot
        if (d1 == 116)
        {
          if (d2 < NUM_PRESETS) currentPreset = d2;
          return false;
        }

        // Send currently selected preset to device
        if (d1 == 117)
        {
          sendPresetToDevice();
          return false;
        }

        if (d1 == 118)
        {
          storePresetsInFlash();
        }

        // Send the CC message to all FM channels
        for (byte i = 0; i < 5; i++) {
          RK002_sendControlChange(i, d1, d2);
        }
        break;

      // Note on-message, we want a simple velocity booster of sorts on this for our FM channel since the GenMDM can get really quiet with low velocities
      case 0x90:
        d2 = (d2 / 10) + 115;
        break;
      case 0xC0: //program change
        break;
    }

    if (doPoly) polymux.inputChannelMessage(sts, d1, d2);

  }

  // PSG channel handler
  // Check if we're receiving a message on our PSG channel
  if ((sts & 0x0f) == psgChannel)
  {
    // By default, send message to Polymux handler
    bool doPoly = true;

    // Check MIDI message type
    switch (sts & 0xf0) {
      // Handle control change messages
      case 0xB0:
        // CC1 = Modwheel
        if (d1 == 1)
        {
          doPoly = false;
          if (d2 > 0)
          {
            psgModLfoDepth = d2;
          }
          else
          {
            psgModLfoDepth = 0;
            psgModLfoActive = false;
          }
        }
        break;
    }

    if (doPoly) psgmux.inputChannelMessage(sts, d1, d2);
  }

  return thru;

}

// PSG Pitch LFO
// The PSG channel on the GenMDM does not support modwheel :( We'll work around this by generating some pitch bend messages once every few cycles
void updatePsgModLfo()
{
  if (psgModLfoSkipCycle == 4)
  {
    // Do stuff and reset the counter to 0
    psgModLfoSkipCycle = 0;
  }
  else
  {
    psgModLfoSkipCycle++;
  }
}

void setup()
{
  fmChannel = (RK002_paramGet(FMCHANNEL) == 0) ? 16 : RK002_paramGet(FMCHANNEL) - 1;
  psgChannel = (RK002_paramGet(PSGCHANNEL) == 0) ? 16 : RK002_paramGet(PSGCHANNEL) - 1;
  polymux.setOutputHandler(onFmPolyMuxOutput, 0);
  polymux.setPolyphony(6); // 5 channels polymux
  psgmux.setOutputHandler(onPsgPolyMuxOutput, 0);
  psgmux.setPolyphony(3); // 3 chans for psg
  recallPresetsFromFlash();
  sendAllPresetsToDevice();
}

void loop()
{
  if (psgModLfoActive) updatePsgModLfo();
}
