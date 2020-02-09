#include <RKPolyMux.h>
#include <RK002.h>

RKPolyMux polymux;

RK002_DECLARE_INFO("GenMDM poly mode","jonne.kokkonen@gmail.com","1.0","c89a53fe-841f-436f-83c7-b0db1023bf9c");

RK002_DECLARE_PARAM(PMUXCH,1,0,16,1)
RK002_DECLARE_PARAM(PMUXBASECH,1,1,16,1)
RK002_DECLARE_PARAM(PMUXPOLY,1,1,8,4)

byte pChannel=0;
 
// POLYMUX RELATED:
// handles polymux engine midi output
void onPolyMuxOutput(void *userarg, byte polymuxidx, byte sts, byte d1, byte d2)
{
  byte chn = RK002_paramGet(PMUXBASECH)- 1 + polymuxidx; // 'polymuxidx' is auto incremented by polymux

  if (chn <= 15)
  {
    sts |= chn; // Bitwise OR: channel will be added to message type 'sts'
    RK002_sendChannelMessage(sts,d1,d2);
  }
}

// Handler for ALL channel messages: used for adding messages to polymux engine
bool RK002_onChannelMessage(byte sts, byte d1, byte d2)
{
  bool thru = false;

  // is it on our midi channel?
  if ((sts & 0x0f) == pChannel)
  {
    // don't send through
    thru = false;
    bool doPoly=true;
    
    switch (sts & 0xf0) {
      case 0xB0: //handle control change messages
        doPoly=false;
        for (byte i=0; i <= RK002_paramGet(PMUXPOLY); i++){
          RK002_sendControlChange(i,d1,d2);
        }
        break;
      case 0x90: //note on message, we want a velocity compressor of sorts on this since genmdm can get really quiet with low velocities        
        d2=(d2/10)+115;
        break;
      case 0xC0: //program change
        break;
    }  
    
    if(doPoly) polymux.inputChannelMessage(sts,d1,d2);
    
  }
  return thru;
}
 
 
void setup()
{
  pChannel = (RK002_paramGet(PMUXCH) == 0) ? 16 : RK002_paramGet(PMUXCH)-1;
  polymux.setOutputHandler(onPolyMuxOutput,0);
  polymux.setPolyphony(5); // 5 channels polymux 
}

void loop()
{
}
