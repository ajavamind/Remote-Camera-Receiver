#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

// WIFI Remote UDP Broadcast Message Receiver 
// Used to trigger two cameras with focus and shutter remote wiring
//

// Clone WeMos D1 R1 board
// Pinout Descriptions
//https://github.com/esp8266/Arduino/blob/master/variants/d1_mini/pins_arduino.h
//https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/

// WIFI local router ssid and passwork
const char * ssid = "PhotoBooth";
const char * pass = "321click";
// WIFI Access Point ssid and passwork
const char * apssid = "Photo Net";
const char * appass = "triggerme";
unsigned int port = 8000;      // local port to broadcast on
WiFiUDP udp;
IPAddress broadcast(255, 255, 255, 255);
boolean connected = false;  // WiFi connected

// maximum serial data expected before a newline
const unsigned int MAX_INPUT = 50;
const unsigned int FOCUS_DELAY = 5; // ms
const unsigned int SHUTTER_DELAY = 10; // ms
const unsigned int SHOT_DELAY = 500; // ms

int FOCUS_INPUT = D5;  // not used for input
int SHUTTER_INPUT = D6; // not used for input
int PROGRAM_INPUT = D7;
int FLASH1 = 5; // D1
int FLASH2 = 4; // D2
int CAMERA1_FOCUS = 4;
int CAMERA1_SHUTTER = 5;
int CAMERA2_FOCUS = D6;
int CAMERA2_SHUTTER = D5;

boolean DEBUG = true;
boolean ALT_FLASH = false;  // alternate between FLASH1 and FLASH2, true trigger both flashes
int flash = FLASH1;

// switch state
boolean programMode = false;   // Program mode
boolean focus = false;
boolean shutter = false;

// Modes
// 0 Program and Debug mode, sets mode, WiFi SSID, password, serial port commands
//   Power up reset, Program switch OFF enters mode stored in EEPROM
//   Power up reset, Program switch ON enters Program and Debug mode.
// 1 Normal FLASH 1 and FLASH 2 output with Shutter switch trigger
// 2 Alternate FLASH 1 and FLASH 2 output with Shutter switch trigger
// 3 Receive focus/shutter broadcast command over WiFi and trigger Camera
// 4 Generate Focus and Shutter broadcast commands over WiFi from focus/shutter switches
// 5
// 6

byte mode = 0; // initial mode
int imgNum = 1;
int flashDelay = 0; // ms

// buffers for receiving and sending data
char packetBuffer[UDP_TX_PACKET_MAX_SIZE + 1]; //buffer to hold incoming packet,
char  replyBuffer[] = "acknowledged\r\n";       // a string to send back

// EEPROM locations
int EEPROM_SIGNATURE = 0; // length 4
int EEPROM_SSID = 4; // length 16 max zero terminated
int EEPROM_PW = 20; // length 16 max zero terminated
int EEPROM_MODE = 36; // length 1
int EEPROM_IMGNUM = 37; // length 2

// Signature
static const char * SIGNATURE = "OCWR";
static char input_line [MAX_INPUT];
static unsigned int input_pos = 0;

boolean validSignature() {
  boolean ok = false;
  input_pos = 0;
  int addr = EEPROM_SIGNATURE;
  while (addr < EEPROM_SIGNATURE + 4) {
    byte value = EEPROM.read(addr);
    if (input_pos < (MAX_INPUT - 1))
      input_line [input_pos++] = char(value);
    addr = addr + 1;
  }
  input_line [input_pos] = 0;  // terminating null byte
  if (strcmp (input_line, SIGNATURE) == 0) {
    ok = true;
    Serial.println(input_line);
  } else {
    Serial.println("Invalid signature");
  }
  return ok;
}

void setSignature() {
  // clear EEPROM
  int i = 0;
  while (i < 512) {
    EEPROM.write(i++, 0);
  }

  i = 0;
  int addr = EEPROM_SIGNATURE;
  while (addr < EEPROM_SIGNATURE + 4) {
    EEPROM.write(addr, SIGNATURE[i++]);
    addr = addr + 1;
  }
}

void loadParameters() {
  // read EEPROM variables
  mode = EEPROM.read(EEPROM_MODE);
}

void storeByte(int addr, byte value) {
  EEPROM.write(addr, value);
  if (EEPROM.commit()) {
    Serial.println("EEPROM successfully committed");
  } else {
    errorMsg("ERROR! EEPROM commit failed", true);
  }
}

int loadInt(int addr) {
  int value;
  value = EEPROM.read(addr);
  value = value << 8;
  value |= EEPROM.read(addr + 1) & 0xFF;
  return value;
}

void storeInt(int addr, int value) {
  EEPROM.write(addr, (value >> 8) & 0xFF);
  EEPROM.write(addr + 1, value & 0xFF);
  if (EEPROM.commit()) {
    Serial.println("EEPROM successfully committed");
  } else {
    errorMsg("ERROR! EEPROM commit failed",true);
  }
}

void storeString(int addr, char* str) {
  byte value = str[0];
  EEPROM.write(addr, value);
  if (value == 0) {
    if (EEPROM.commit()) {
      Serial.println("EEPROM successfully committed");
    } else {
      errorMsg("ERROR! EEPROM commit failed", true);
    }
  }
}

void updateMode() {
  EEPROM.write(EEPROM_MODE, mode);
  if (EEPROM.commit()) {
    Serial.println("Mode successfully committed");
  } else {
    errorMsg("ERROR! EEPROM Mode commit failed", true);
  }
}

void errorMsg(const char* str, boolean loop) {
  while (loop) {
    Serial.printf(str);
    Serial.printf("\n");
    digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);   // Turn the LED off
    delay(250);
  }
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level

  pinMode(PROGRAM_INPUT, INPUT_PULLUP);
  //pinMode(FOCUS_INPUT, INPUT_PULLUP);
  //pinMode(SHUTTER_INPUT, INPUT_PULLUP);
  pinMode(CAMERA1_FOCUS, OUTPUT);
  digitalWrite(CAMERA1_FOCUS, LOW);
  pinMode(CAMERA1_SHUTTER, OUTPUT);
  digitalWrite(CAMERA1_SHUTTER, LOW);
  pinMode(CAMERA2_FOCUS, OUTPUT);
  digitalWrite(CAMERA2_FOCUS, LOW);
  pinMode(CAMERA2_SHUTTER, OUTPUT);
  digitalWrite(CAMERA2_SHUTTER, LOW);

  Serial.begin(115200);
  EEPROM.begin(512);

  while (!validSignature()) {
    setSignature();
  }
  loadParameters();

  programMode = digitalRead(PROGRAM_INPUT) == LOW;
  Serial.print("PROGRAM_INPUT=");
  Serial.println(programMode);
  if (programMode) {
    mode = 0;
  }
  //mode = 3;  // force mode for debugging
  //mode = 5;  // force mode for debugging
  //mode = 2; // force mode alternate flash
  mode =6;
  input_pos = 0;
  flash = FLASH1;
  Serial.print("Mode=");
  Serial.println(mode);
  delay(250);
  switch (mode) {
    case 0:
      connected = initializeWiFi();
      break;
    case 1:
      break;
    case 2:
      break;
    case 3:
      break;
    case 4:
      connected = initializeAccessPoint();
      break;
    case 5:
      connected = initializeAccessPoint();
      break;
    case 6:
      connected = initializeWiFi();
      break;
    default:
      break;
  }
}

void loop()
{
  switch (mode) {
    case 0:
      getSerial();
      break;
    case 1:
      normalFlash(0);
      break;
    case 2:
      selectFlash(0);
      break;
    case 3:
      alternateFlash(0);
      break;
    case 4:
      getUdp();
      break;
    case 5:
      cameraInput();
      getUdp();
      break;
    case 6:
      cameraProgramInput();
      getUdpBroadcast();
      break;
    default:
      break;
  }
}

boolean initializeWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    errorMsg("WiFi Failed ", false);
    return false;
  }
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  Serial.printf("UDP broadcast receive on port %d\n", port);
  udp.begin(port);

  digitalWrite(LED_BUILTIN, HIGH);   // Turn the LED off (Note that HIGH is the voltage level
  return true;
}

boolean initializeAccessPoint() {
  //WiFi.mode(WIFI_STA);
  //WiFi.begin(ssid, pass);
  WiFi.softAP(apssid, appass);
  IPAddress ip = WiFi.softAPIP();
  //if (WiFi.waitForConnectResult() != WL_CONNECTED) {
  //  errorMsg("WiFi Failed ", true);
  //  return false;
  //}
  Serial.print("Access Point IP address: ");
  Serial.println(ip);
  Serial.printf("UDP broadcast on port %d\n", port);
  udp.begin(port);

  digitalWrite(LED_BUILTIN, HIGH);   // Turn the LED off (Note that HIGH is the voltage level
  return true;
}

String imgFilename(int x) {
  String builtString = String("");
  if (x < 10) {
    builtString += "000";
  } else if (x < 100) {
    builtString += "00";
  } else if (x < 1000) {
    builtString += "0";
  }
  builtString += String(x);
  return builtString;
}

// Process incoming serial data after a line terminator received
void process_data (char * data)
{
  if (strcmp (data, "R") == 0 || strcmp(data, "r") == 0)
  {
    if (connected) {
      udp.beginPacket(broadcast, port); // subnet Broadcast IP and port
      udp.write("R");
      udp.endPacket();
    } else {
      printf("No WiFi connection %s\n", ssid);
    }
  }
  else if (strcmp (data, "F") == 0 || strcmp(data, "f") == 0) // focus only
  {
    if (connected) {
      udp.beginPacket(broadcast, port); // subnet Broadcast IP and port
      udp.write("F");
      udp.endPacket();
    } else {
      printf("No WiFi connection %s\n", ssid);
    }
  }
  else if (strcmp (data, "C") == 0 || strcmp(data, "c") == 0)  // focus and shutter capture
  {
    if (connected) {
      udp.beginPacket(broadcast, port); // subnet Broadcast IP and port
      udp.write("C");
      udp.write(imgFilename(imgNum).c_str());
      imgNum++;
      udp.endPacket();
    } else {
      printf("No WiFi connection %s\n", ssid);
    }
  }
  else if (strcmp (data, "L") == 0 || strcmp(data, "l") == 0)  // toggle LED
  {
    if (digitalRead(LED_BUILTIN) == LOW)
      digitalWrite(LED_BUILTIN, HIGH);
    else
      digitalWrite(LED_BUILTIN, LOW);
  }
  else if (strcmp (data, "D") == 0 || strcmp(data, "d") == 0)  // Output Debug information
  {
    programMode = digitalRead(PROGRAM_INPUT) == LOW;
    Serial.print("PROGRAM_INPUT=");
    Serial.println(programMode);
    focus = digitalRead(FOCUS_INPUT) == LOW;
    Serial.print("FOCUS_INPUT=");
    Serial.println(focus);
    shutter = digitalRead(SHUTTER_INPUT) == LOW;
    Serial.print("SHUTTER_INPUT=");
    Serial.println(shutter);
    if (digitalRead(LED_BUILTIN) == LOW)
      Serial.println("LED on");
    else
      Serial.println("LED off");
    Serial.printf("EEPROM Mode=%d\n", EEPROM.read(EEPROM_MODE));

    Serial.printf("Image number=%d\n", loadInt(EEPROM_IMGNUM));

    //
    if (connected) {
      Serial.print("Connected! IP address: ");
      Serial.println(WiFi.localIP());
      Serial.printf("UDP broadcast on port %d\n", port);
    } else {
      Serial.printf("WiFi not connected to %s\n", ssid);
    }
    delay(100);
    Serial.printf("LED GPIO=%d\n", LED_BUILTIN);
    Serial.printf("D0 GPIO=%d\n", D0);
    Serial.printf("D1 GPIO=%d\n", D1);
    Serial.printf("D2 GPIO=%d\n", D2);
    Serial.printf("D3 GPIO=%d\n", D3);
    Serial.printf("D4 GPIO=%d\n", D4);
    Serial.printf("D5 GPIO=%d\n", D5);
    Serial.printf("D6 GPIO=%d\n", D6);
    Serial.printf("D7 GPIO=%d\n", D7);
    Serial.printf("D8 GPIO=%d\n", D8);
  }

  else if (strcmp (data, "0") == 0 )  // clear FLASH1 and FLASH2
  {
    digitalWrite(FLASH1, LOW);
    digitalWrite(FLASH2, LOW);
  }
  else if (strcmp (data, "1") == 0 )  // trigger FLASH1
  {
    digitalWrite(FLASH1, HIGH);
    delay(1);
    digitalWrite(FLASH1, LOW);
  }
  else if (strcmp (data, "2") == 0 )  // trigger FLASH2
  {
    digitalWrite(FLASH2, HIGH);
    delay(1);
    digitalWrite(FLASH2, LOW);
  }
  else if (data[0] == 'M' || data[0] == 'm') // set mode
  {
    Serial.printf("Set Mode %c\n", data[1]);
    if (data[1] >= '0' && data[1] <= '9') {
      byte value = data[1] - '0';
      storeByte(EEPROM_MODE, value);
      Serial.printf("mode %d set\n", value);
    }
  }
  else if (strcmp (data, "I") == 0 || strcmp(data, "i") == 0)  // set SSID
  {
  }
  else if (strcmp (data, "P") == 0 || strcmp(data, "p") == 0)  // set Password
  {
  }
  Serial.println(data);

  delay(10);
}  // end of process_data

void getUdp() {
  // if there's data available, read a packet
  int packetSize = udp.parsePacket();
  if (packetSize) {
    if (DEBUG) {
      Serial.printf("Received packet of size %d from %s:%d\n    (to %s:%d, free heap = %d B)\n",
                    packetSize,
                    udp.remoteIP().toString().c_str(), udp.remotePort(),
                    udp.destinationIP().toString().c_str(), udp.localPort(),
                    ESP.getFreeHeap());
    }
    // read the packet into packetBufffer
    int n = udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
    packetBuffer[n] = 0;
    if (DEBUG) {
      Serial.println("Contents:");
      Serial.println(packetBuffer);
    }
    if (packetBuffer[0] == 'C' || packetBuffer[0] == 'S') {
    }
    if (DEBUG) {
      // send a reply, to the IP address and port that sent us the packet we received
      //udp.beginPacket(udp.remoteIP(), udp.remotePort());
      //udp.write(replyBuffer);
      //udp.endPacket();
    }
  }
}

void getUdpBroadcast() {
  // if there's data available, read a packet
  int packetSize = udp.parsePacket();
  if (packetSize) {
    if (DEBUG) {
      Serial.printf("Received packet of size %d from %s:%d\n    (to %s:%d, free heap = %d B)\n",
                    packetSize,
                    udp.remoteIP().toString().c_str(), udp.remotePort(),
                    udp.destinationIP().toString().c_str(), udp.localPort(),
                    ESP.getFreeHeap());
    }
    // read the packet into packetBufffer
    int n = udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
    packetBuffer[n] = 0;
    if (DEBUG) {
      Serial.println("Contents:");
      Serial.println(packetBuffer);
    }
    if (packetBuffer[0] == 'R' || packetBuffer[0] == 'r') {
      digitalWrite(CAMERA1_SHUTTER, LOW);
      digitalWrite(CAMERA2_SHUTTER, LOW);
      digitalWrite(CAMERA1_FOCUS, LOW);
      digitalWrite(CAMERA2_FOCUS, LOW);
    } else if (packetBuffer[0] == 'F' || packetBuffer[0] == 'f') {
      digitalWrite(CAMERA1_FOCUS, HIGH);
      digitalWrite(CAMERA2_FOCUS, HIGH);
    } else if (packetBuffer[0] == 'C' || packetBuffer[0] == 'c') {
      digitalWrite(CAMERA1_FOCUS, HIGH);
      digitalWrite(CAMERA2_FOCUS, HIGH);
      delay(FOCUS_DELAY);
      digitalWrite(CAMERA1_SHUTTER, HIGH);
      digitalWrite(CAMERA2_SHUTTER, HIGH);
      delay(SHUTTER_DELAY);
      digitalWrite(CAMERA1_SHUTTER, LOW);
      digitalWrite(CAMERA2_SHUTTER, LOW);
      digitalWrite(CAMERA1_FOCUS, LOW);
      digitalWrite(CAMERA2_FOCUS, LOW);
    } else if (packetBuffer[0] == 'S' || packetBuffer[0] == 's') {
      digitalWrite(CAMERA1_FOCUS, HIGH);
      digitalWrite(CAMERA2_FOCUS, HIGH);
      delay(FOCUS_DELAY);
      digitalWrite(CAMERA1_SHUTTER, HIGH);
      digitalWrite(CAMERA2_SHUTTER, HIGH);
      delay(SHUTTER_DELAY);
      digitalWrite(CAMERA1_SHUTTER, LOW);
      digitalWrite(CAMERA2_SHUTTER, LOW);
    }
    if (DEBUG) {
      // send a reply, to the IP address and port that sent us the packet we received
      //udp.beginPacket(udp.remoteIP(), udp.remotePort());
      //udp.write(replyBuffer);
      //udp.endPacket();
    }
  }
}

void getSerial()
{
  // check for incoming serial data:
  while (Serial.available() > 0) {
    char inByte = Serial.read ();

    switch (inByte)
    {

      case '\n':   // end of text
        input_line [input_pos] = 0;  // terminating null byte

        // terminator reached! process input_line here ...
        process_data (input_line);

        // reset buffer for next time
        input_pos = 0;
        break;

      case '\r':   // discard carriage return
        break;

      default:
        // keep adding if not full ... allow for terminating null byte
        if (input_pos < (MAX_INPUT - 1))
          input_line [input_pos++] = inByte;
        break;

    }  // end of switch

  }  // end of incoming data
}

void alternateFlash(int ms) {
  if (ms != 0) {
    delay(ms);
  }
  while (digitalRead(SHUTTER_INPUT) == LOW) {
    digitalWrite(flash, HIGH);
    delay(1);
    digitalWrite(flash, LOW);
    if (flash == FLASH1) {
      flash = FLASH2;
    } else {
      flash = FLASH1;
    }
    while (digitalRead(SHUTTER_INPUT) == LOW) {
      delay(1);
    }
  }
}

void normalFlash(int ms) {
  if (ms != 0) {
    delay(ms);
  }
  while (digitalRead(SHUTTER_INPUT) == LOW) {
    digitalWrite(FLASH1, HIGH);
    digitalWrite(FLASH2, HIGH);
    delay(1);
    digitalWrite(FLASH1, LOW);
    digitalWrite(FLASH2, LOW);
    while (digitalRead(SHUTTER_INPUT) == LOW) {
      delay(1);
    }
  }
}

void selectFlash(int ms) {
  boolean option = digitalRead(PROGRAM_INPUT) == LOW;
  if(option) {
    normalFlash(ms);
  } else {
    alternateFlash(ms);
  }
}

void altFlash(int ms) {
  if (ms != 0) {
    delay(ms);
  }
  digitalWrite(flash, HIGH);
  delay(1);
  digitalWrite(flash, LOW);
  if (flash == FLASH1) {
    flash = FLASH2;
  } else {
    flash = FLASH1;
  }
}

void normFlash(int ms) {
  if (ms != 0) {
    delay(ms);
  }
  digitalWrite(FLASH1, HIGH);
  digitalWrite(FLASH2, HIGH);
  delay(1);
  digitalWrite(FLASH1, LOW);
  digitalWrite(FLASH2, LOW);
}

void cameraProgramInput() {
  if (digitalRead(PROGRAM_INPUT) == LOW) {
    digitalWrite(CAMERA1_FOCUS, HIGH);
    digitalWrite(CAMERA2_FOCUS, HIGH);
    if (DEBUG) digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that HIGH is the voltage level
    if (DEBUG) Serial.println("FOCUS on");
    delay(FOCUS_DELAY);
    digitalWrite(CAMERA1_SHUTTER, HIGH);
    digitalWrite(CAMERA2_SHUTTER, HIGH);
    if (DEBUG) Serial.println("SHUTTER on");
    delay(1);
    while (digitalRead(PROGRAM_INPUT) == LOW) {
      delay(1);
    }
    digitalWrite(CAMERA1_SHUTTER, LOW);
    digitalWrite(CAMERA2_SHUTTER, LOW);
    if (DEBUG) Serial.println("SHUTTER off");
    digitalWrite(CAMERA1_FOCUS, LOW);
    digitalWrite(CAMERA2_FOCUS, LOW);
    if (DEBUG) digitalWrite(LED_BUILTIN, HIGH);   // Turn the LED off (Note that HIGH is the voltage level
    if (DEBUG) Serial.println("FOCUS off");
    delay(SHOT_DELAY);
  }
}

void cameraInput() {
  int ctrf = 0;
  int ctrs = 0;
  while (digitalRead(FOCUS_INPUT) == LOW) {
    digitalWrite(CAMERA1_FOCUS, HIGH);
    digitalWrite(CAMERA2_FOCUS, HIGH);
    if (DEBUG) digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that HIGH is the voltage level
    if (DEBUG) Serial.println("FOCUS on");
    delay(1);
    ctrf++;
    if (digitalRead(SHUTTER_INPUT) == LOW) {
      digitalWrite(CAMERA1_SHUTTER, HIGH);
      digitalWrite(CAMERA2_SHUTTER, HIGH);
      if (DEBUG) Serial.println("SHUTTER on");
      delay(1);
      ctrf++;
      ctrs++;
      while (digitalRead(SHUTTER_INPUT) == LOW) {
        delay(1);
        ctrf++;
        ctrs++;
      }
      digitalWrite(CAMERA1_SHUTTER, LOW);
      digitalWrite(CAMERA2_SHUTTER, LOW);
      if (DEBUG) Serial.println("SHUTTER off");
    }
    digitalWrite(CAMERA1_FOCUS, LOW);
    digitalWrite(CAMERA2_FOCUS, LOW);
    if (DEBUG) digitalWrite(LED_BUILTIN, HIGH);   // Turn the LED off (Note that HIGH is the voltage level
    if (DEBUG) Serial.println("FOCUS off");
  }
  if (DEBUG) {
    if (ctrf != 0 || ctrs != 0) {
      Serial.printf("focus=%d shutter=%d\n", ctrf, ctrs);
      delay(1000); // debug delay
    }
  }
}
