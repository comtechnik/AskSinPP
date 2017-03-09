//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

/*
 * Setup defines to configure the library.
 * Note: If you are using the Eclipse Arduino IDE you will need to set the
 * defines in the project properties.
 */
#ifndef __IN_ECLIPSE__
  #define USE_AES
  #define HM_DEF_KEY 0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10
  #define HM_DEF_KEY_INDEX 0
#endif

#include <AskSinPP.h>
#include <PinChangeInt.h>
#include <TimerOne.h>
#include <LowPower.h>

#include <MultiChannelDevice.h>
#include <SwitchChannel.h>

// number of relays - possible values 1,2,4
// will map to HM-LC-SW1-SM, HM-LC-SW2-SM, HM-LC-SW4-SM
#define RELAY_COUNT 4

// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER

#ifdef USE_OTA_BOOTLOADER
  #define OTA_MODEL_START  0x7ff0 // start address of 2 byte model id in bootloader
  #define OTA_SERIAL_START 0x7ff2 // start address of 10 byte serial number in bootloader
  #define OTA_HMID_START   0x7ffc // start address of 3 byte device id in bootloader
#else
  // define model is matching the number of relays
  #if RELAY_COUNT == 2
    #define SW_MODEL 0x0a
  #elif RELAY_COUNT == 4
    #define SW_MODEL 0x03
  #else
    #define SW_MODEL 0x02
  #endif
  // device ID
  #define DEVICE_ID HMID(0x12,0x34,0x56)
  // serial number
  #define DEVICE_SERIAL "papa000000"
#endif

// we use a Pro Mini
// Arduino pin for the LED
// D4 == PIN 4 on Pro Mini
#define LED_PIN 4
// Arduino pin for the config button
// B0 == PIN 8 on Pro Mini
#define CONFIG_BUTTON_PIN 8


// relay output pins compatible to the HM_Relay project
#define RELAY1_PIN 5
#define RELAY2_PIN 6
#define RELAY3_PIN 7
#define RELAY4_PIN 3

// number of available peers per channel
#define PEERS_PER_CHANNEL 4


// all library classes are placed in the namespace 'as'
using namespace as;

// map number of channel to pin
// this will be called by the SwitchChannel class
uint8_t SwitchPin (uint8_t number) {
  switch( number ) {
    case 2: return RELAY2_PIN;
    case 3: return RELAY3_PIN;
    case 4: return RELAY4_PIN;
  }
  return RELAY1_PIN;
}

// setup the device with channel type and number of channels
MultiChannelDevice<SwitchChannel<PEERS_PER_CHANNEL>,RELAY_COUNT> sdev(0x20);

class CfgButton : public Button {
public:
  CfgButton () {
    setLongPressTime(seconds2ticks(3));
  }
  virtual void state (uint8_t s) {
    uint8_t old = Button::state();
    Button::state(s);
    if( s == Button::released ) {
      sdev.channel(1).toggleState();
    }
    else if( s == longreleased ) {
      sdev.startPairing();
    }
    else if( s == longpressed ) {
      if( old == longpressed ) {
        sdev.reset(); // long pressed again - reset
      }
      else {
        sled.set(StatusLed::key_long);
      }
    }
  }
};

CfgButton cfgBtn;
void cfgBtnISR () { cfgBtn.check(); }

// if A0 and A1 connected
// we use LOW for ON and HIGH for OFF
bool checkLowActive () {
  pinMode(14,OUTPUT); // A0
  pinMode(15,INPUT);  // A1
  digitalWrite(15,HIGH);
  digitalWrite(14,LOW);
  return digitalRead(15) == LOW;
}

void setup () {
#ifndef NDEBUG
  Serial.begin(57600);
  DPRINTLN(ASKSIN_PLUS_PLUS_IDENTIFIER);
#endif
  // first initialize EEProm if needed
  if( eeprom.setup(sdev.checksum()) == true ) {
    sdev.firstinit();
  }

  bool low = checkLowActive();
  for( uint8_t i=1; i<=sdev.channels(); ++i ) {
    sdev.channel(i).lowactive(low);
  }

  sled.init(LED_PIN);

  cfgBtn.init(CONFIG_BUTTON_PIN);
  attachPinChangeInterrupt(CONFIG_BUTTON_PIN,cfgBtnISR,CHANGE);
  radio.init();

#ifdef USE_OTA_BOOTLOADER
  sdev.init(radio,OTA_HMID_START,OTA_SERIAL_START);
  sdev.setModel(OTA_MODEL_START);
  if( sdev.getModel()[1] == 0x02 ) {
    sdev.channels(1);
    DPRINTLN(F("HM-LC-SW1-SM"));
  }
  else if( sdev.getModel()[1] == 0x0a ) {
    sdev.channels(2);
    DPRINTLN(F("HM-LC-SW2-SM"));
  }
  else {
    DPRINTLN(F("HM-LC-SW4-SM"));
  }
#else
  sdev.init(radio,DEVICE_ID,DEVICE_SERIAL);
  sdev.setModel(0x00,SW_MODEL);
#endif
  sdev.setFirmwareVersion(0x16);
  sdev.setSubType(Device::Switch);
  sdev.setInfo(0x41,0x01,0x00);

  radio.enableGDO0Int();

  aclock.init();

  sled.set(StatusLed::welcome);

  // TODO - random delay
}

void loop() {
  bool worked = aclock.runready();
  bool poll = sdev.pollRadio();
  if( worked == false && poll == false ) {
    activity.savePower<Idle>();
  }
}
