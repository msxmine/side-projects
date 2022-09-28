#include <ESP8266WiFi.h>
//#include <ESP8266mDNS.h>
//#include <WiFiUdp.h>
//#include <ArduinoOTA.h>
#include <PubSubClient.h>

/* Certyfikat */
const char caCert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
DEADBEEF
-----END CERTIFICATE-----
)EOF";


const char* ssid = "deadbeef";
const char* password = "deadbeef";
const char* mqtt_server = "mqtt-server.lan";
const char* mqtt_login = "deadbeef";
const char* mqtt_password = "deadbeef";

X509List caCertX509(caCert);
WiFiClientSecure espClient;
PubSubClient clientMQTT(espClient);

const byte rsAddr = 0x02;
const int PIN_BUS_BUSY = 14;
const int PIN_ESP_BOOTED = 5;
const int PIN_PICO_BOOTSEL = 12;
const int PIN_PICO_RUN = 4;

/*
WiFiUDP Udp;
unsigned int localUdpPort = 4210;
char incomingPacket[256];
char replyPacket[500] = "Hello!\n\0";
*/

byte serialBuf[1024];

unsigned long lastTaskStart = 0;
unsigned long lastWiFiScan = 0;
unsigned long lastBusGood = 0;

//msg constants

const int DEVICE_CODE_OFFSET = 0;
const byte DEVICE_CODE_PANEL = 0xc4;
const byte DEVICE_CODE_HVAC = 0x44;

const int PANEL_TOUCH_OFFSET = 1;
const int PANEL_SENSOR_OFFSET = 3;

const byte TOUCH_IDLE_VAL = 0x00;
const byte TOUCH_EDIT_VAL = 0x01;
const byte TOUCH_CLOCK_VAL = 0x01;
const byte TOUCH_NEXT_VAL = 0x06;
const byte TOUCH_MAX_VAL = 0x06;
const byte TOUCH_OK_VAL = 0x06;
const byte TOUCH_DOWN_VAL = 0x07;
const byte TOUCH_DOWN_LONG_VAL = 0x87;
const byte TOUCH_UP_VAL = 0x08;
const byte TOUCH_ESC_VAL = 0x09;
const byte TOUCH_VIEW_VAL = 0x09;
const byte TOUCH_MENU_VAL = 0x0a;

const int HVAC_BUZZER_OFFSET = 1;
const int HVAC_SPIN_OFFSET = 1;

const int HVAC_WEEKDAY_OFFSET = 3;

const int HVAC_GEARBARS_OFFSET = 6;
const int HVAC_STOP_OFFSET = 6;

const int HVAC_ONOFF_OFFSET = 8;

const int HVAC_HEATEX_OFFSET = 10;

const int HVAC_DIGIT_4_OFFSET = 12;
const int HVAC_DIGIT_3_OFFSET = 13;
const int HVAC_DIGIT_2_OFFSET = 14;
const int HVAC_DIGIT_1_OFFSET = 15;


const int HVAC_BLINK_OFFSET = 18;

const int SEVSEGM_A_BIT = 0;
const int SEVSEGM_B_BIT = 1;
const int SEVSEGM_C_BIT = 2;
const int SEVSEGM_D_BIT = 3;
const int SEVSEGM_E_BIT = 4;
const int SEVSEGM_F_BIT = 5;
const int SEVSEGM_G_BIT = 6;
const int SEVSEGM_DOTS_BIT = 7;


struct hvacState
{
  int view_id = -1;
  int gear = -1;
  int setting_state = -1;
  int heat_exchanger = -1;
};

const int VIEW_ID_MAINVIEW = 1;
const int VIEW_ID_MENU1_VENT = 2;
const int VIEW_ID_MENU1_OUTFAN = 3;
const int VIEW_ID_MENU1_INFAN = 4;
const int VIEW_ID_MENU1_HEATEX = 5;
const int VIEW_ID_MENU1_HEATEX_EDIT = 6;
const int VIEW_ID_HEATEX_VIEW = 7;


struct rsTask
{
  int id = -1;
  int targetGear = -1;
  int targetView = -1;
  int targetHeatEx = -1;
};

const int TASK_ID_SETGEAR = 1;
const int TASK_ID_SETHEATEX = 2;
const int TASK_ID_NAVIGATE = 100;

struct rsTask queue[100];

int queueFront = 0;
int queueBack = 0;
int queueFree = 100;

bool addTask(struct rsTask toadd){
  if ( (queueBack + 1) % 100 == queueFront){
    return false;
  }
  else{
    if (queueBack == queueFront){
      lastTaskStart = millis();
    }
    queue[queueBack] = toadd;
    queueBack = (queueBack+1) % 100;
    queueFree--;
  }
}

struct rsTask topTask(){
  if (queueFront == queueBack){
    struct rsTask empty;
    return empty;
  }
  else{
    return queue[queueFront];
  }
}

bool popTask(){
  if (queueFront == queueBack){
    return false;
  }
  else{
    queueFront = (queueFront+1) % 100;
    if (queueFront != queueBack){
      lastTaskStart = millis();
    }
    queueFree++;
  }
}


unsigned int mbCRC(byte* msg, int len){
  unsigned int crc = 0xffff;

  for (int pos = 0; pos < len; pos++){
    crc ^= (unsigned int)msg[pos];
    
    for (int i = 0; i < 8; i++){
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      }
      else{
        crc >>= 1;
      }
    }
  }

  return crc;
}


void rsSend(byte* data, int len){
  if (len > 256){
    return;
  }

  byte pkgBuf[270];
  pkgBuf[0] = 0xfe; //HVAC address
  pkgBuf[1] = rsAddr; //This controller's address
  memcpy(pkgBuf+2, data, len);
  unsigned int pkgCRC = mbCRC(pkgBuf, len+2);
  pkgBuf[len+2] = (byte)(pkgCRC & 0x00ff);
  pkgBuf[len+3] = (byte)( (pkgCRC >> 8) & 0x00ff);

  byte specialSymbols[] = {0x16, 0x68, 0x80};
  
  byte transmitBuf[550];

  int tBufidx = 0;
  
  transmitBuf[tBufidx++] = 0xff;
  transmitBuf[tBufidx++] = 0xff;
  transmitBuf[tBufidx++] = 0x68; //Preamble
  
  
  for (int i = 0; i < len+4; i++){
    int specialIdx = -1;
    for (int j = 0; j < sizeof(specialSymbols); j++){
      if (pkgBuf[i] == specialSymbols[j]){
        specialIdx = j;
        break;
      }
    }
    if (specialIdx >= 0){
      transmitBuf[tBufidx++] = 0x80;
      transmitBuf[tBufidx++] = specialSymbols[specialIdx] ^ 0xff;
    }
    else{
      transmitBuf[tBufidx++] = pkgBuf[i];
    }
  }

  transmitBuf[tBufidx++] = 0x16;

  Serial.write(transmitBuf, tBufidx);
  Serial.flush();
}

int rsRecv(byte* buf, int siz){
  if (siz > 256){
    return -1;
  }

  byte recvBuf[1024];
  int pos = 0;

  byte preamble[] = {0xff, 0xff, 0x68};
  int prematch = 0;

  int got = 0;
  byte data[3];

  Serial.setTimeout(1);

  bool specialByte = false;
  while (true){
    got = Serial.readBytes(data, 1);
    if (got == 0){
      return 0; //Timed out
    }
    else{
      if (prematch < sizeof(preamble)){
        if (data[0] == preamble[prematch]){
          prematch++;
        }
        else{
          prematch = 0;
        }
      }
      else{
        if (pos > 250){
          return -1;
        }
        else{
          if (data[0] == 0x16){
            break;
          }
          else{
            if (data[0] == 0x80){
              specialByte = true;
            }
            else{
              if (specialByte){
                recvBuf[pos] = data[0] ^ 0xff;
                specialByte = false;
              }
              else{
                recvBuf[pos] = data[0];
              }
              pos++;
            }
          }
        }
      }
    }
  }

  if (Serial.readBytes(data,1) != 0){
    return -4; //Another msg in fifo
  }

  if (pos < 5){
    return -2; //Corrupted
  }

  unsigned int packCRC = mbCRC(recvBuf, pos-2);

  if ( (byte)(packCRC & 0x00ff) != recvBuf[pos-2] || (byte)((packCRC >> 8) & 0x00ff) != recvBuf[pos-1]){
    return -2; //Corrupted
  }

  if (recvBuf[0] != rsAddr || recvBuf[1] != 0xfe){
    return -3; //Not for this node
  }

  if (pos - 4 > siz){
    return -1;
  }

  memcpy(buf, recvBuf+2, pos-4);
  return pos-4;
}

int sendResponse(byte touchstat){
  byte response[4];
  memset(response, 0, 4);
  
  response[DEVICE_CODE_OFFSET] = DEVICE_CODE_PANEL;
  response[PANEL_SENSOR_OFFSET] = 0x7c; //???

  int sent = 0;

  bool busState = digitalRead(PIN_BUS_BUSY);
  

  response[PANEL_TOUCH_OFFSET] = touchstat;


  if (!busState){
    rsSend(response, 4);
    sent = 1;
  }

  return sent;
  
}

void xorBuffers(const byte* b1, const byte* b2, byte* ret, const int len){
  for (int i = 0; i < len; i++){
    ret[i] = b1[i] ^ b2[i];
  }
}

void andBuffers(const byte* b1, const byte* b2, byte* ret, const int len){
  for (int i = 0; i < len; i++){
    ret[i] = b1[i] & b2[i];
  }
}

bool checkZeros(const byte* target, const int len){
  bool ret = true;
  for(int i = 0; i < len; i++){
    if (target[i] != 0){
      ret = false;
      break;
    }
  }
  return ret;
}

bool inArray(const byte obj, const byte* arr, const int len){
  bool found = false;
  for (int i = 0; i < len; i++){
    if (arr[i] == obj){
      found = true;
      break;
    }
  }
  return found;
}



struct hvacState decodeState(byte* data, int len){
  const byte view_template_main[] = {0x44,0x05,0x80,0x40,0x22,0x00,0x17,0x00,0x10,0x00,0xd4,0x08,0x6f,0x3f,0xe6,0x06,0x82,0x00};
  const byte view_mask_main[]     = {0xff,0xf0,0xff,0x00,0xff,0xff,0x00,0xff,0xff,0xff,0x7f,0xff,0x00,0x00,0x00,0x00,0xff,0xff};

  const byte view_template_menu1_vent[] = {0x44,0x07,0x06,0x00,0x12,0x00,0x0f,0x00,0x00,0x80,0x41,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x86};
  const byte view_mask_menu1_vent[] =     {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xbf,0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

  const byte view_template_menu1_outfan[] = {0x44,0x0f,0x06,0x00,0x12,0x00,0x00,0x00,0x80,0x00,0x41,0x08,0x00,0x00,0x00,0x00,0x00,0x01,0x4b};
  const byte view_mask_menu1_outfan[] =     {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

  const byte view_template_menu1_infan[] = {0x44,0x0f,0x06,0x00,0x12,0x00,0x00,0x00,0x80,0x00,0x45,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x42};
  const byte view_mask_menu1_infan[] =     {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

  const byte view_template_menu1_heatex[] = {0x44,0x0f,0x06,0x00,0x12,0x00,0x00,0x00,0x80,0x00,0xc1,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x47};
  const byte view_mask_menu1_heatex[]  =    {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x3f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

  const byte view_template_menu1_heatex_edit[] = {0x44,0x0f,0x06,0x00,0x10,0x00,0x10,0x00,0x90,0x00,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x37};
  const byte view_mask_menu1_heatex_edit[] =     {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x3f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe};

  const byte view_template_heatex_status[] = {0x44,0x0f,0x00,0x00,0x22,0x08,0x00,0x00,0x40,0x00,0x44,0x08,0x00,0x00,0x00,0x00,0x00,0x80,0x47};
  const byte view_mask_heatex_status[] =     {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x3f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

  const byte sevsegm_hex[16] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71};
  const byte weekdays[7] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40};

  if (len == 18 || len == 19){
    byte temp[30];
    xorBuffers(data, view_template_main, temp, 18);
    andBuffers(temp, view_mask_main, temp, 18);
    
    if (checkZeros(temp, 18)){
      if (inArray(data[HVAC_WEEKDAY_OFFSET], weekdays,7 )){
        if (inArray(data[HVAC_DIGIT_1_OFFSET], sevsegm_hex, 3) &&
            inArray(data[HVAC_DIGIT_2_OFFSET]^bit(SEVSEGM_DOTS_BIT), sevsegm_hex, 10) &&
            inArray(data[HVAC_DIGIT_3_OFFSET], sevsegm_hex, 6) &&
            inArray(data[HVAC_DIGIT_4_OFFSET], sevsegm_hex, 10)){
          int gear = -1;
          int heatex = -1;
          if (data[HVAC_SPIN_OFFSET] == 0x00 && data[HVAC_GEARBARS_OFFSET] == 0x50 && len == 18){gear = 0;}
          if (data[HVAC_SPIN_OFFSET] == 0x01 && data[HVAC_GEARBARS_OFFSET] == 0x11 && len == 18){gear = 1;}
          if (data[HVAC_SPIN_OFFSET] == 0x02 && data[HVAC_GEARBARS_OFFSET] == 0x13 && len == 19 && data[HVAC_BLINK_OFFSET] == 0x21){gear = 2;}
          if (data[HVAC_SPIN_OFFSET] == 0x03 && data[HVAC_GEARBARS_OFFSET] == 0x13 && len == 18){gear = 3;}
          if (data[HVAC_SPIN_OFFSET] == 0x04 && data[HVAC_GEARBARS_OFFSET] == 0x17 && len == 19 && data[HVAC_BLINK_OFFSET] == 0x22){gear = 4;}
          if (data[HVAC_SPIN_OFFSET] == 0x05 && data[HVAC_GEARBARS_OFFSET] == 0x17 && len == 18){gear = 5;}
          if (data[HVAC_SPIN_OFFSET] == 0x06 && data[HVAC_GEARBARS_OFFSET] == 0x1f && len == 19 && data[HVAC_BLINK_OFFSET] == 0x23){gear = 6;}
          if (data[HVAC_SPIN_OFFSET] == 0x07 && data[HVAC_GEARBARS_OFFSET] == 0x1f && len == 18){gear = 7;}

          if (data[HVAC_HEATEX_OFFSET] == 0xd4){heatex = 1;}
          if (data[HVAC_HEATEX_OFFSET] == 0x54){heatex = 0;}

          if (gear != -1 && heatex != -1){
            struct hvacState ret;
            ret.view_id = 1;
            ret.gear = gear;
            ret.heat_exchanger = heatex;
            return ret;
          }
        }
      }
    }
  }

  if (len == 19){
    byte temp[30];
    xorBuffers(data, view_template_menu1_vent, temp, 19);
    andBuffers(temp, view_mask_menu1_vent, temp, 19);

    if (checkZeros(temp, 19)){
      if ( (data[HVAC_ONOFF_OFFSET] == 0x00 && data[9] == 0x80) || (data[HVAC_ONOFF_OFFSET] == 0x40 && data[9] == 0x00) ){
        struct hvacState ret;
        ret.view_id = VIEW_ID_MENU1_VENT;
        return ret;
      }
    }
  }

  if (len == 19){
    byte temp[30];
    xorBuffers(data, view_template_menu1_outfan, temp, 19);
    andBuffers(temp, view_mask_menu1_outfan, temp, 19);

    if (checkZeros(temp, 19)){
      struct hvacState ret;
      ret.view_id = VIEW_ID_MENU1_OUTFAN;
      return ret;
    }
  }

  if (len == 19){
    byte temp[30];
    xorBuffers(data, view_template_menu1_infan, temp, 19);
    andBuffers(temp, view_mask_menu1_infan, temp, 19);

    if (checkZeros(temp, 19)){
      struct hvacState ret;
      ret.view_id = VIEW_ID_MENU1_INFAN;
      return ret;
    }
  }

  if (len == 19){
    byte temp[30];
    xorBuffers(data, view_template_menu1_heatex, temp, 19);
    andBuffers(temp, view_mask_menu1_heatex, temp, 19);

    if (checkZeros(temp, 19)){
      if (data[HVAC_ONOFF_OFFSET] == 0x80){
        struct hvacState ret;
        ret.view_id = VIEW_ID_MENU1_HEATEX;
        ret.setting_state = 1;
        return ret;
      }
      if (data[HVAC_ONOFF_OFFSET] == 0x40){
        struct hvacState ret;
        ret.view_id = VIEW_ID_MENU1_HEATEX;
        ret.setting_state = 0;
        return ret;
      }
    }
  }

  if (len == 19){
    byte temp[30];
    xorBuffers(data, view_template_menu1_heatex_edit, temp, 19);
    andBuffers(temp, view_mask_menu1_heatex_edit, temp, 19);

    if (checkZeros(temp, 19)){
      if (data[HVAC_ONOFF_OFFSET] == 0x90 && data[HVAC_BLINK_OFFSET] == 0x37){
        struct hvacState ret;
        ret.view_id = VIEW_ID_MENU1_HEATEX_EDIT;
        ret.setting_state = 1;
        return ret;
      }
      if (data[HVAC_ONOFF_OFFSET] == 0x50 && data[HVAC_BLINK_OFFSET] == 0x36){
        struct hvacState ret;
        ret.view_id = VIEW_ID_MENU1_HEATEX_EDIT;
        ret.setting_state = 0;
        return ret;
      }
    }
  }

  if (len == 19){
    byte temp[30];
    xorBuffers(data, view_template_heatex_status, temp, 19);
    andBuffers(temp, view_mask_heatex_status, temp, 19);

    if (checkZeros(temp, 19)){
      if (data[HVAC_ONOFF_OFFSET] == 0x40 || data[HVAC_ONOFF_OFFSET] == 0x80){
        struct hvacState ret;
        ret.view_id = VIEW_ID_HEATEX_VIEW;
        return ret;
      }
    }
  }

  struct hvacState ret;
  return ret;
  
}

void reconnectMQTT() {
  if (clientMQTT.connect("hvac-strych-1", mqtt_login, mqtt_password)){
    clientMQTT.subscribe("home-assistant/strych/hvac/1/set/Gear");
    clientMQTT.subscribe("home-assistant/strych/hvac/1/set/HeatEx");
    //clientMQTT.subscribe("home-assistant/strych/hvac/1/set/picoboot");
  }
}
/*
void picoBoot(bool usb){
  digitalWrite(PIN_PICO_RUN, LOW);
  pinMode(PIN_PICO_RUN, OUTPUT);
  digitalWrite(PIN_PICO_RUN, LOW);

  delay(5);

  if(usb){
    digitalWrite(PIN_PICO_BOOTSEL, LOW);
    pinMode(PIN_PICO_BOOTSEL, OUTPUT);
    digitalWrite(PIN_PICO_BOOTSEL, LOW);
  }

  delay(5);
  pinMode(PIN_PICO_RUN, INPUT);
  delay(5);
  pinMode(PIN_PICO_BOOTSEL, INPUT);  
}
*/
void callbackMQTT(const char* topic, byte* payload, unsigned int len){
  if (len > 250){
    return;
  }
  char text[256];
  memcpy(text, payload, len);
  text[len] = '\0';
  if (strcmp(topic, "home-assistant/strych/hvac/1/set/Gear") == 0){
    int targetgear = atoi(text);
    struct rsTask goToMain;
    goToMain.id = TASK_ID_NAVIGATE;
    goToMain.targetView = VIEW_ID_MAINVIEW;
    struct rsTask setGear;
    setGear.id = TASK_ID_SETGEAR;
    setGear.targetGear = targetgear;
    if (queueFree > 2){
      addTask(goToMain);
      addTask(setGear);
    }
  }

  if (strcmp(topic, "home-assistant/strych/hvac/1/set/HeatEx") == 0){
    int targetheatex = -1;
    if (strcmp(text, "on") == 0){
      targetheatex = 1;
    }
    if (strcmp(text, "off") == 0){
      targetheatex = 0;
    }
    if (targetheatex >= 0 ){
      struct rsTask goToMain;
      goToMain.id = TASK_ID_NAVIGATE;
      goToMain.targetView = VIEW_ID_MAINVIEW;
      struct rsTask setHeatEx;
      setHeatEx.id = TASK_ID_SETHEATEX;
      setHeatEx.targetHeatEx = targetheatex;
      struct rsTask returnToMain;
      returnToMain.id = TASK_ID_NAVIGATE;
      returnToMain.targetView = VIEW_ID_MAINVIEW;
      if (queueFree > 3){
        addTask(goToMain);
        addTask(setHeatEx);
        addTask(returnToMain);
      }
    }
  }
  /*
  if (strcmp(topic, "home-assistant/strych/hvac/1/set/picoboot") == 0){
    if (strcmp(text, "usb") == 0){
      picoBoot(true);
    }
    if (strcmp(text, "run") == 0){
      picoBoot(false);
    }
  }
  */
}

void setup() {
  pinMode(PIN_BUS_BUSY, INPUT_PULLUP);
  pinMode(PIN_ESP_BOOTED, OUTPUT);
  pinMode(PIN_PICO_BOOTSEL, INPUT);
  pinMode(PIN_PICO_RUN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  digitalWrite(PIN_ESP_BOOTED, LOW);
  digitalWrite(LED_BUILTIN, LOW);

  
  WiFi.mode(WIFI_STA);
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(false);
  WiFi.hostname("hvac-strych-1");
  WiFi.begin(ssid, password);
  WiFi.waitForConnectResult();

  espClient.setTrustAnchors(&caCertX509);
  espClient.setX509Time(1564991524);

  //ArduinoOTA.begin();
  //Udp.begin(localUdpPort);

  clientMQTT.setServer(mqtt_server, 8883);
  clientMQTT.setCallback(callbackMQTT);

  Serial.setTimeout(1);
  while(Serial.read() != -1){}

}

int reported_gear = -1;
int reported_heatex = -1;
int reported_alive = -1;
int current_alive = -1;

void loop() {
  
  int recvbytes = rsRecv(serialBuf, 200);

  if (recvbytes > 0){

    struct hvacState current = decodeState(serialBuf, recvbytes);
    struct rsTask task = topTask();

    bool matched = false;

    if (task.id == TASK_ID_SETGEAR){
      if (current.view_id == 1){
        if (current.gear == task.targetGear){
          popTask();
        }
        else{
          if (task.targetGear == 0){
            sendResponse(TOUCH_DOWN_LONG_VAL);
            matched = true;
          }
          else{
            if (current.gear < task.targetGear){
              sendResponse(TOUCH_UP_VAL);
              matched = true;
            }
            else{
              sendResponse(TOUCH_DOWN_VAL);
              matched = true;
            }
          }
        }
      }
    }

    if (task.id == TASK_ID_SETHEATEX){
      if (current.view_id == VIEW_ID_MAINVIEW){
        if (current.heat_exchanger == task.targetHeatEx){
          popTask();
        }
        else{
          sendResponse(TOUCH_MENU_VAL);
          matched = true;
        }
      }
      if (current.view_id == VIEW_ID_MENU1_VENT){
        sendResponse(TOUCH_NEXT_VAL);
        matched = true;
      }
      if (current.view_id == VIEW_ID_MENU1_OUTFAN){
        sendResponse(TOUCH_NEXT_VAL);
        matched = true;
      }
      if (current.view_id == VIEW_ID_MENU1_INFAN){
        sendResponse(TOUCH_NEXT_VAL);
        matched = true;
      }
      if (current.view_id == VIEW_ID_MENU1_HEATEX){
        if (current.setting_state == task.targetHeatEx){
          popTask();
        }
        else{
          sendResponse(TOUCH_EDIT_VAL);
          matched = true;
        }
      }
      if (current.view_id == VIEW_ID_MENU1_HEATEX_EDIT){
        if (current.setting_state == task.targetHeatEx){
          sendResponse(TOUCH_OK_VAL);
          matched = true;
        }
        if (current.setting_state == 0 && task.targetHeatEx == 1){
          sendResponse(TOUCH_UP_VAL);
          matched = true;
        }
        if (current.setting_state == 1 && task.targetHeatEx == 0){
          sendResponse(TOUCH_DOWN_VAL);
          matched = true;
        }
      }
    }

    if (task.id == TASK_ID_NAVIGATE){
      if (current.view_id == task.targetView){
        popTask();
      }
      else{
        if (task.targetView == VIEW_ID_MAINVIEW){
          if (current.view_id == VIEW_ID_MENU1_VENT || current.view_id == VIEW_ID_MENU1_OUTFAN || current.view_id == VIEW_ID_MENU1_INFAN || current.view_id == VIEW_ID_MENU1_HEATEX){
            sendResponse(TOUCH_ESC_VAL);
            matched = true;
          }
          if (current.view_id == VIEW_ID_HEATEX_VIEW){
            sendResponse(TOUCH_CLOCK_VAL);
            matched = true;
          }
        }
      }
    }



/*
    char tempHexRep[120] = "";
    replyPacket[0] = 0;
  
    for (int i = 0; i < recvbytes; i++){
      sprintf(tempHexRep, "%02x", serialBuf[i]);
      strcat(replyPacket, tempHexRep);
    }

    sprintf(tempHexRep, " VIEW: %d, GEAR: %d , HEATEX: %d ", current.view_id, current.gear, current.heat_exchanger);
    strcat(replyPacket, tempHexRep);
    
    strcat(replyPacket, "\n");
    

    int paklenpc = strlen(replyPacket);
    if (WiFi.isConnected()){
    Udp.beginPacket(msxpc, 7777);
    Udp.write(replyPacket, paklenpc);
    Udp.endPacket();
    }
*/


    struct rsTask ttask;
    ttask = topTask();
    unsigned long now = millis();
    if (ttask.id == -1 || (now - lastTaskStart) > 10000ul){
      if ( WiFi.isConnected() && clientMQTT.connected()){
        if(current.gear != -1 && current.gear != reported_gear){
          char mqttMsgBuf[20];
          reported_gear = current.gear;
          sprintf(mqttMsgBuf, "%d", current.gear);
          clientMQTT.publish("home-assistant/strych/hvac/1/get/Gear", mqttMsgBuf, true);
        }
    
        if(current.heat_exchanger != -1 && current.heat_exchanger != reported_heatex){
          reported_heatex = current.heat_exchanger;
          if (current.heat_exchanger == 0){
            clientMQTT.publish("home-assistant/strych/hvac/1/get/HeatEx", "off", true);
          }
          if (current.heat_exchanger == 1){
            clientMQTT.publish("home-assistant/strych/hvac/1/get/HeatEx", "on", true);
          }
        }
      }
    }

  }



  unsigned long now = millis();
  if (digitalRead(PIN_BUS_BUSY) == false){
    lastBusGood = now;
    current_alive = 1;
  }
  
  if ((now - lastBusGood) > 5000ul){
    lastBusGood = (now - 6000ul);
    current_alive = 0;
    while (queueFree < 100){
      popTask();
    }
  }

  
  if (queueFree == 100 || (now - lastTaskStart) > 10000ul){
    
    if ( (!WiFi.isConnected() && (now - lastWiFiScan) > 90000ul) || (now - lastWiFiScan) > 600000ul ){
      if (WiFi.scanComplete() == -2){
        lastWiFiScan = now;
        WiFi.scanNetworks(true, false);
      }
    }
  
    int foundnets = WiFi.scanComplete();
    if (foundnets >= 0){
      bool cur_conn = false;
      int32_t cur_rssi = -400;
      uint8_t cur_bssid[6] = {0,0,0,0,0,0};
  
      if (WiFi.isConnected()){
        cur_conn = true;
        cur_rssi = WiFi.RSSI();
        memcpy(cur_bssid, WiFi.BSSID(), 6);
      }
  
      int new_index = -1;
      int32_t new_rssi = -400;
      int32_t new_channel = -1;
      uint8_t new_bssid[6] = {0,0,0,0,0,0};
  
      for (int i = 0; i < foundnets; i++){
        if (WiFi.SSID(i) == String(ssid)){
          if (WiFi.RSSI(i) > new_rssi){
            new_index = i;
            new_rssi = WiFi.RSSI(i);
            new_channel = WiFi.channel(i);
            memcpy(new_bssid, WiFi.BSSID(i), 6);
          }
        }
      }
  
      bool switchnets = true;
  
      if (new_index == -1){
        switchnets = false;
      }
  
      if (memcmp(cur_bssid, new_bssid, 6) == 0){
        switchnets = false;
      }
  
      if ( cur_conn && new_rssi <= (cur_rssi+6) ){
        switchnets = false;
      }
  
      if (switchnets){
        clientMQTT.disconnect();
        WiFi.disconnect(false);
        WiFi.begin(ssid, password, new_channel, new_bssid, true);
        WiFi.waitForConnectResult();
      }
  
      WiFi.scanDelete();
      
    }


    if (WiFi.isConnected() && (now - lastWiFiScan) > 45000ul){
      IPAddress myIP = WiFi.localIP();
      if ((myIP == INADDR_NONE) || (myIP == INADDR_ANY) || (myIP == IPAddress(0)) || myIP.isLocal()){
        WiFi.disconnect(false);
      }
    }

    if (WiFi.isConnected()){
      if (!clientMQTT.connected()){
        reconnectMQTT();
      }
    }

    if ( WiFi.isConnected() && clientMQTT.connected()){
      if (reported_alive != current_alive){
        reported_alive = current_alive;
        if (current_alive == 0){
          clientMQTT.publish("home-assistant/strych/hvac/1/get/HvacResponding", "off", true);
        }
        if (current_alive == 1){
          clientMQTT.publish("home-assistant/strych/hvac/1/get/HvacResponding", "on", true);
        }
      }
    }
    
  }
  
  clientMQTT.loop();
  //ArduinoOTA.handle();
  delay(20);
}
