#include <RKPolyMux.h>
#include <RK002.h>

RKPolyMux polymux;
RKPolyMux psgmux;

RK002_DECLARE_INFO("GenMDM preset manager/poly tool", "jonne.kokkonen@gmail.com", "1.0", "c89a53fe-841f-436f-83c7-b0db1023bf9c");

RK002_DECLARE_PARAM(FMCHANNEL, 1, 0, 16, 3)
RK002_DECLARE_PARAM(PSGCHANNEL, 1, 0, 16, 4)
RK002_DECLARE_PARAM(ENABLEPOLYFM, 1, 0, 1, 1)
RK002_DECLARE_PARAM(ENABLEPOLYPSG, 1, 0, 1, 1)

//Enable debug messages
//#define DEBUG true

// Define a specific value to detect if tables are already present in RK002 memory
#define FLASH_SIGNATURE 0xFAFAFAFA

// Define the amount of presets to store / range of the CC #116 (Select preset slot)
// Probably RAM and EEPROM are the things that put most limitations to this
#define NUM_PRESETS 6

// Define MIDI CC numbers
#define CC_SELECT_PRESET_SLOT 116
#define CC_LOAD_PRESET 117
#define CC_STORE_IN_FLASH 118
#define CC_SEND_ALL_TO_GENMDM 119

// Define memory table structure
struct {
  uint32_t signature;
  byte preset[NUM_PRESETS][128];
} flashData;

byte activeSettings[128];

// Global variables
byte fmChannel = 0;
byte psgChannel = 0;
bool enablePolyFM;
bool enablePolyPSG;

bool psgModLfoActive = false;
byte psgModLfoSpeed = 10;
byte psgModLfoDepth = 0;
unsigned long tmr_psglfo = 0;

uint8_t selectedPreset = 0;

// Read / initialize memory with lookup tables
bool recallPresetsFromFlash()
{
  #ifdef DEBUG
    RK002_printf("Read mem");
  #endif
  uint8_t res = RK002_readFlash(0, sizeof(flashData), (byte*)&flashData);

  // init flash if readback failed:
  if (res != 0 || flashData.signature != FLASH_SIGNATURE) {
    #ifdef DEBUG
      RK002_printf("Read mem fail");
    #endif

    //set all values as 255 = do not transmit
    memset(flashData.preset, 255, sizeof(flashData.preset));

    flashData.signature = FLASH_SIGNATURE;
    return false;
  }
  return true;
}

// Write preset table data to RK002 memory
void storeAllPresetsInFlash()
{
  // Put current settings to selected preset slot
  storeCurrentSettingsToPreset();
  #ifdef DEBUG
    RK002_printf("Write mem");
  #endif
  int res = RK002_writeFlash(0, sizeof(flashData), (byte*)&flashData);
  #ifdef DEBUG
    if (res != 0) RK002_printf("Write fail");
  #endif

}

void storeCurrentSettingsToPreset()
{
  #ifdef DEBUG
    RK002_printf("Store to pres %d",selectedPreset);
  #endif
  for (int i=0;i<127;i++)
  {
    flashData.preset[selectedPreset][i] = activeSettings[i];
    #ifdef DEBUG
      RK002_printf("PRE: %d CC: %d VAL: %d",selectedPreset,i,activeSettings[i]);
    #endif
  }
}

// Send current preset to GenMDM, store in GenMDM RAM for faster access
void sendPresetToDevice(byte presetNo, byte ch)
{
  if (presetNo > NUM_PRESETS) return;
  #ifdef DEBUG
    RK002_printf("Send preset %02d", selectedPreset);
  #endif

  for (uint8_t j = 0; j < 128; j++)
  {
    if (flashData.preset[presetNo][j] != 255)
    {
      #ifdef DEBUG
        RK002_printf("PRE: %d CC: %d VAL: %d",selectedPreset,j,flashData.preset[selectedPreset][j]);
      #endif
      activeSettings[j] = flashData.preset[presetNo][j];
      if (enablePolyFM) {
        //Send preset to all 5 FM channels if Polymux is enabled
        for (uint8_t i = 0; i < 5; i++)
          RK002_sendControlChange(i, j, flashData.preset[presetNo][j]);
      } else {
        RK002_sendControlChange(ch, j, flashData.preset[presetNo][j]);
      }
    }
  }

  // Store preset in GenMDM's RAM for faster access.
  uint8_t genMdmPresetSlot = (8 * presetNo);
  if (genMdmPresetSlot == 0) genMdmPresetSlot = 1;
  RK002_sendControlChange(0, 6, genMdmPresetSlot);

}

// Send all presets to device
void sendAllPresetsToDevice()
{
  // Iterate through all presets and send the CCs to all of our five FM channels
  for (uint8_t k = 0; k < NUM_PRESETS; k++) {
    #ifdef DEBUG
      RK002_printf("SENDALL: Send preset %d", k);
    #endif
    sendPresetToDevice(k,0);
    delay(100);
  }
}

// POLYMUX RELATED:
// handles polymux engine midi output
void onFmPolyMuxOutput(void *userarg, byte polymuxidx, byte sts, byte d1, byte d2)
{
  byte chn = polymuxidx; // 'polymuxidx' is auto incremented by polymux

  if (chn <= 15) {
    sts |= chn; // Bitwise OR: channel will be added to message type 'sts'
    RK002_sendChannelMessage(sts, d1, d2);
  }
}

void onPsgPolyMuxOutput(void *userarg, byte polymuxidx, byte sts, byte d1, byte d2)
{
  byte chn = 6 + polymuxidx; // 'polymuxidx' is auto incremented by polymux

  if (chn <= 15) {
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
  if ((enablePolyFM && (sts & 0x0f) == fmChannel) || (!enablePolyFM && (sts & 0x0f) < 6)) {
    bool doPoly = true;

    // Check midi message type
    switch (sts & 0xf0) {

      // Handle control change messages
      case 0xB0:

        // Do not send the message to Polymux processor
        doPoly = false;

        //RK002_printf("%d %d",(sts & 0x0f),d1);

        switch (d1) {
          
          // Select preset slot
          case CC_SELECT_PRESET_SLOT:
            if (d2 < NUM_PRESETS) selectedPreset = d2;
            #if DEBUG
            RK002_printf("Slot %d",selectedPreset);
            #endif
            return false;
            break;
            
          // Send currently selected preset to device
          case CC_LOAD_PRESET:  
            sendPresetToDevice(selectedPreset, (sts & 0x0f));
            return false;
            break;
          
          // Store the preset array's current state in RK002 flash.
          case CC_STORE_IN_FLASH:
            storeAllPresetsInFlash();
            return false;
            break;
          
          // Send all presets to GenMDM. Not sure if this actually needs to be a CC...
          case CC_SEND_ALL_TO_GENMDM:
            sendAllPresetsToDevice();
            return false;
            break;
          
          // GenMDM RAM preset store/recall. We do not want to store these settings, just let them pass through.  
          case 6:
          case 9:
            break;
          default:
            activeSettings[d1] = d2;
            #ifdef DEBUG
              RK002_printf("aS CC %d value %d",d1,d2);
            #endif          
            break;        
        }

        if (enablePolyFM) {
          // Send the CC message to all FM channels
          for (byte i = 0; i < 5; i++)
            RK002_sendControlChange(i, d1, d2);
        }
        
        break;

      // Note on-message, we want a simple velocity booster of sorts on this for our FM channel since the GenMDM can get really quiet with low velocities
      case 0x90:
        d2 = (d2 / 10) + 115;
        break;
        
      // Program change, convert the message to GenMDM's internal RAM preset recall CC message
      case 0xC0:
        if (d1 >= NUM_PRESETS) return false;
        doPoly = false;
        uint8_t genMdmPresetSlot = (8 * d1);
        if (genMdmPresetSlot == 0) genMdmPresetSlot = 1;
        if (enablePolyFM) {
          for (byte i = 0; i < 5; i++)
            RK002_sendControlChange(i, 9, genMdmPresetSlot);
        } else
          RK002_sendControlChange((sts & 0x0f), 9, genMdmPresetSlot);
        return false;
        break;
    }

    if (!enablePolyFM) {
      doPoly = false;
      thru = true;
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
      // Note on-message, we want a simple velocity booster of sorts on this for our PSG channel since the GenMDM can get really quiet with low velocities
      case 0x90:
        d2 = (d2 / 10) + 115;
        break;
      
      // Handle control change messages
      case 0xB0:
        // CC1 = Modwheel
        if (d1 == 0x01) {
          doPoly = false;
          if (d2 > 0) {
            psgModLfoActive = true;
            psgModLfoDepth = d2;
            tmr_psglfo = millis();
          } else {
            psgModLfoDepth = 0;
            psgModLfoActive = false;
            tmr_psglfo = 0;
            for (int i=6; i<9; i++)
              RK002_sendPitchBend(i, 0);
          }
        }
        break;
      // Handle pitch bend messages
      case 0xE0:
        if (d2 == 0x40 && d1 == 0x00) {
          if (psgModLfoDepth != 0)
            psgModLfoActive = true;
        } else {
          psgModLfoActive = false;
        }
        break;
    }

    if (!enablePolyPSG) {
      doPoly = false;
      thru = true;
    }

    if (doPoly)
      psgmux.inputChannelMessage(sts, d1, d2);
  }

  return thru;

}

// PSG Pitch LFO
// The PSG channel on the GenMDM does not support modwheel :( We'll work around this by generating some pitch bend messages once every few cycles
void updatePsgModLfo()
{  
  static int psgModLfoPbValue = 0;
  static bool psgModLfoDirectionUp = false;
  // Send the message if 10ms has passed since the last one
  unsigned long t_now = millis();
  if ((t_now - tmr_psglfo) >= 10) {
    if (psgModLfoDirectionUp) {
      psgModLfoPbValue += (psgModLfoSpeed+(psgModLfoDepth*3));
      for (int i=6; i<9; i++)
        RK002_sendPitchBend(i, psgModLfoPbValue);  
      if (psgModLfoPbValue > psgModLfoDepth*10)
        psgModLfoDirectionUp = false;
    } else {
      psgModLfoPbValue -= (psgModLfoSpeed+(psgModLfoDepth*3));
      for (int i=6; i<9; i++)
        RK002_sendPitchBend(i, psgModLfoPbValue);
      if (-psgModLfoPbValue > psgModLfoDepth*10)
        psgModLfoDirectionUp = true;
    }
    tmr_psglfo = millis();
  }
}

void setup()
{
  enablePolyFM = RK002_paramGet(ENABLEPOLYFM);
  enablePolyPSG = RK002_paramGet(ENABLEPOLYPSG);
  #ifdef DEBUG
    if (enablePolyFM) RK002_printf("FM Poly");
    if (enablePolyPSG) RK002_printf("PSG Poly");
  #endif
  
  //initialize the active settings array
  memset(activeSettings,255,sizeof(activeSettings));
  #ifdef DEBUG
    RK002_printf("Flashdata size %d",sizeof(flashData));
  #endif
  fmChannel = (RK002_paramGet(FMCHANNEL) == 0) ? 16 : RK002_paramGet(FMCHANNEL) - 1;
  psgChannel = (RK002_paramGet(PSGCHANNEL) == 0) ? 16 : RK002_paramGet(PSGCHANNEL) - 1;
  if (enablePolyFM) {
    polymux.setOutputHandler(onFmPolyMuxOutput, 0);
    polymux.setPolyphony(5); // 5 channels polymux
  }
  psgmux.setOutputHandler(onPsgPolyMuxOutput, 0);
  psgmux.setPolyphony(3); // 3 chans for psg
  if (recallPresetsFromFlash()) {
    sendAllPresetsToDevice();
    // Load preset 1 from RAM (GenMDM CC #9)
    RK002_sendControlChange(0, 9, 1);
    //If polyFM isn't enabled, we need to send the preset to all channels
    if (!enablePolyFM) {
      for (int i=1; i<6; i++)
        RK002_sendControlChange(i, 9, 1);
    }
  } else {
    storeAllPresetsInFlash();
  }
}

void loop()
{
  if (psgModLfoActive) updatePsgModLfo();
}
