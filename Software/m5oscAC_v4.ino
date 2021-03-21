#include <WiFiUdp.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager (Install from Library Manager)
#include <OSCMessage.h>   // https://github.com/CNMAT/OSC (Install from Library Manager)
#include <analogWrite.h>  // https://github.com/ERROPiX/ESP32_AnalogWrite (Install from Library Manager)
#include <M5Atom.h>       // https://github.com/m5stack/M5Atom (Install from Library Manager)

#include <RBDdimmer.h> // https://github.com/RobotDynOfficial/RBDDimmer (Download zip and go to Sketch > Include Library > Add .ZIP Library)

// OSC reciever for m5stack atom - AC light dimmer using triac!
// Since the TouchOSC app only sends floats, floats everything...

// Float value between 0 and 1 at DEVICE_ROUTE - sets output PWM
// Float 0 or 1 at DEVICE_ROUTE/full - fade to zero / fade to full individual
// Float 0 or 1 at GLOBAL_ROUTE - fade to zero / fade to full all lights
// Float value (more details below) at GLOBAL_ROUTE/fd - sets the fade delay


// CONFIGS!!
// ----------------------------------------------

// Set this device's name here!
// Set different names for different devices (ex: luz2, luz3, etc)
// Do NOT use "/" here, it is auto added to generate the device address later
# define DEVICE_NAME "luz7"

// OSC address to speak to all devices (set with "/")
// Not recommended to change!
# define GLOBAL_ROUTE "/global"

// Set signal out pin here!
# define triacpin G26
// Set pin to read zero crossing signal here!
# define zcpin G32

// Fade loop default delay (milliseconds). A full fade is this duration x 32.
// Full fade durations example: 5 = 160ms, 125 = 4s
// This value can be changed at runtime using the /global/fd address
int fadedelay = 20;

// ----------------------------------------------
// DO NOT CHANGE STUFF BELOW (unless you know what you're doing)

// used to hold the delay in use because everything uses fadeToValue() now
// this is set to fadedelay on fade to full / zero calls
// and set to 2-5 ms during regular calls
int actual_delay = fadedelay;
// normal delay for regular calls
#define normal_delay 4

dimmerLamp dimmer(triacpin, zcpin);

// UDP communication
WiFiUDP Udp;
const unsigned int inPort = 8888;

// stores the generated fade setting path
char fdroute[sizeof(GLOBAL_ROUTE)+3];
char devroute[sizeof(DEVICE_NAME)+1];

// current light level
int current_dimval = 0;
// requested light level
int new_dimval = 0;

void setup() {
  Serial.begin(115200);


  // ------ Setup WIFI --------------------
  WiFi.mode(WIFI_STA); 
  WiFiManager wm;
  //wm.resetSettings();
  
  char ssid[sizeof(DEVICE_NAME)+4];
  sprintf(ssid, "ESP-%s", DEVICE_NAME);
  wm.autoConnect(ssid);
  
  // --------------------------------------

  // Setup UDP
  Udp.begin(inPort);

  // Setup M5 object
  // Params are SerialEnable, I2CEnable, DisplayEnable
  M5.begin(true, false, false);
  
  // Generate route names from device name
  sprintf(fdroute, "%s/fd", GLOBAL_ROUTE);
  sprintf(devroute, "/%s", DEVICE_NAME);

  // dimmer stuff
  dimmer.begin(NORMAL_MODE, ON);
  dimmer.setPower(0);
}

void loop(){ 
  
  int packetSize = Udp.parsePacket();
  if(packetSize) {
  
    byte buf[packetSize];
    int bytesRead = Udp.read(buf, packetSize);
        
    OSCMessage msgIn;
    msgIn.fill(buf, packetSize);
    
    msgIn.route(devroute, deviceCallback);
    msgIn.dispatch(GLOBAL_ROUTE, globalCallback);
    msgIn.dispatch(fdroute, setFadeCallback);

    //printpacketdebuginfo(packetSize, bytesRead, buf);
    //printOSCdebuginfo(msgIn);
  }

  if (M5.Btn.wasPressed()) {
    new_dimval = 0;
    actual_delay = normal_delay;
  }
  fadeToValue(new_dimval, actual_delay);

  
  M5.update();
}

// gets value from app and sets it to new_dimval
// Values mapped to 0-95 because power above 95 seems to cause flicker
void deviceCallback(OSCMessage &msg, int off) {
  if (msg.fullMatch("/full", sizeof(DEVICE_NAME))) {
    if (msg.isFloat(0)) {
      int value = msg.getFloat(0);
    
      actual_delay = fadedelay;
      if (value == 0.) new_dimval=0;
      else if (value == 1.) new_dimval=95;
    }
  }
  else if (msg.isFloat(0)) {
    float value = msg.getFloat(0);
    if (value >= 0 && value <= 1) {
      actual_delay = normal_delay;
      new_dimval = map(value*100, 0, 100, 0, 95);
    }
  }
}

void globalCallback(OSCMessage &msg) {  
  if (msg.isFloat(0)) {
    int value = msg.getFloat(0);
    
    actual_delay = fadedelay;
    if (value == 0.) new_dimval=0;
    else if (value == 1.) new_dimval=95;
  }
}

void setFadeCallback(OSCMessage &msg) {  
  if (msg.isFloat(0)) {
    int value = trunc(msg.getFloat(0));
    if (value > 0) fadedelay = value;
    else fadedelay = 0;
  }
}

// fade from current level to value
void fadeToValue(int value, int loop_delay) {
  // if same, do nothing
  if (value == current_dimval) return;
  // if valor > dimval --> subir
  if (value > current_dimval) {
    // <= required to allow current_dimval to become equal to value
    for (int i = current_dimval;i<=value; i++) {
      current_dimval = i;
//      Serial.print(value);
//      Serial.print(" bigger ");
//      Serial.println(current_dimval);
      dimmer.setPower(current_dimval);
      delay(loop_delay);
    }
  }
  // valor < dimval --> descer
  else {
    for (int i = current_dimval;i>=value; i--) {
      current_dimval = i;
//      Serial.print(value);
//      Serial.print(" smalelr ");
//      Serial.println(current_dimval);
      dimmer.setPower(current_dimval);
      delay(loop_delay);
    }
  }
}
