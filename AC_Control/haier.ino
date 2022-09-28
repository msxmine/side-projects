#include <ESP8266WiFi.h>
#include <PubSubClient.h>

const String ssid = "deadbeef"; //ssid
const char* password = "deadbeef"; //haslo
const char* mqtt_server = "mqtt-server.lan"; //Server MQTT
const char* mqtt_login = "deadbeef";
const char* mqtt_password = "deadbeef";

#define PREFIX "home-assistant/salon"
#define NUMBER "1"
#define ID_CONNECT "klimatyzator-salon-1" //nazwa klienta mqtt

/* Certyfikat */
const char caCert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
DEADBEEF
-----END CERTIFICATE-----
)EOF";

X509List caCertX509(caCert);
WiFiClientSecure espClient;
PubSubClient client(espClient);

#define B_CUR_TMP   13  //Aktualna temperatura
#define B_CMD       17  //00-komenda 7F-odpowiedz
#define B_MODE      23  //04 - DRY, 01 - cool, 02 - heat, 00 - smart 03 - wentulacja
#define B_FAN_SPD   25  //wentylator 02 - min, 01 - mid, 00 - max, 03 - auto
#define B_SWING     27  //01 - gora dol. 00 - brak. 02 - lewo prawo. 03 - oba naraz
#define B_LOCK_REM  28  //80 blokada pilota 00 -  brak
#define B_POWER     29  //on/off 01 - on, 00 - off (10, 11)-kondensator?? 09 - jonizator
#define B_FRESH     31  //fresh 00 - off, 01 - on 
#define B_SET_TMP   35  //Ustaw temperature

int fresh;
int power;
int oldpower;
int swing;
int lock_rem;
int cur_tmp;
int set_tmp;
int old_tmp;
int fan_spd;
int Mode;
int noise;
int light;
int health;
int health_airflow;
int compressor;

int tempoffsetmode = -1;
int fixmode = -1;


bool firsttransfer = true;
bool transferinprog = false;
int unclean = 0;

unsigned long previous_poll = 0;
unsigned long previous_wifi_scan = 0;
unsigned long previous_wifi_process = 0;
unsigned long previous_mqtt_conn = 0;
unsigned long previous_wifi_conn = 0;
const unsigned long poll_interval = 1000;
const unsigned long wifi_scan_interval = 600000;
const unsigned long wifi_process_interval = 10000;
const unsigned long mqtt_conn_interval = 10000;
const unsigned long wifi_conn_interval = 60000;

byte data[37] = {};
byte olddata[37] = {};
byte packdata[37] = {};
byte qstn[] = {255,255,10,0,0,0,0,0,1,1,77,1,90}; // komenda pollingu
byte on[]   = {255,255,10,0,0,0,0,0,1,1,77,2,91}; // wlacz klimatyzator
byte off[]  = {255,255,10,0,0,0,0,0,1,1,77,3,92}; // wylacz klimatyzator
byte lock[] = {255,255,10,0,0,0,0,0,1,3,0,0,14};  // zablokuj pilota

void reconnect() {
    if ( client.connect(ID_CONNECT , mqtt_login, mqtt_password) ) {
      client.subscribe(PREFIX"/klimatyzator/"NUMBER"/set/Set_Temp");
      client.subscribe(PREFIX"/klimatyzator/"NUMBER"/set/Mode");
      client.subscribe(PREFIX"/klimatyzator/"NUMBER"/set/rawMode");
      client.subscribe(PREFIX"/klimatyzator/"NUMBER"/set/Fan_Speed");
      client.subscribe(PREFIX"/klimatyzator/"NUMBER"/set/Swing");
      client.subscribe(PREFIX"/klimatyzator/"NUMBER"/set/Lock_Remote");
      client.subscribe(PREFIX"/klimatyzator/"NUMBER"/set/Power");
      client.subscribe(PREFIX"/klimatyzator/"NUMBER"/set/Ionizer");
      client.subscribe(PREFIX"/klimatyzator/"NUMBER"/set/Fresh");
      client.subscribe(PREFIX"/klimatyzator/"NUMBER"/set/Light");
      client.subscribe(PREFIX"/klimatyzator/"NUMBER"/set/Airflow");
      client.subscribe(PREFIX"/klimatyzator/"NUMBER"/set/Turbo");
      client.subscribe(PREFIX"/klimatyzator/"NUMBER"/set/Quiet");
      
    }
}

void scan_wifi_init(){
  WiFi.scanNetworks(true, false);
}

void scan_wifi_process(bool force = false) {
  bool ustanowione = false;
  int32_t obecny_rssi = -400;
  uint8_t obecny_bssid[6] = {0, 0, 0, 0, 0, 0};
  
  if (WiFi.isConnected() && WiFi.status() == WL_CONNECTED){
    obecny_rssi = WiFi.RSSI();
    for (int i = 0; i < 6; i++){
      obecny_bssid[i] = WiFi.BSSID()[i];
    }
    ustanowione = true;
  }
  
  int liczba_sieci = WiFi.scanComplete();
  
  if (liczba_sieci > 0){
    int32_t max_rssi_value = -400;
    int max_rssi_index = -1;
    int32_t max_rssi_channel = -1;
    uint8_t max_rssi_bssid[6] = {0, 0, 0, 0, 0, 0};
    
    for (int i = 0; i < liczba_sieci; i++){
      if (WiFi.SSID(i) == ssid){
        if (WiFi.RSSI(i) > max_rssi_value){
          max_rssi_value = WiFi.RSSI(i);
          max_rssi_index = i;
          max_rssi_channel = WiFi.channel(i);
          for (int j = 0; j < 6; j++){
            max_rssi_bssid[j] = WiFi.BSSID(i)[j];
          }
        }
      }
    }
    if (max_rssi_index != -1){
      bool tensamap = false;
      if (ustanowione){
        tensamap = true;
        for (int i = 0; i < 6; i++){
          if (max_rssi_bssid[i] != obecny_bssid[i]){
            tensamap = false;
          }
        }
      }
      if ( !tensamap ){
        if (((max_rssi_value > (obecny_rssi + 6)) || !ustanowione ) || force){
          client.disconnect();
          WiFi.setAutoReconnect(false);
          WiFi.disconnect(false);
          WiFi.setAutoReconnect(true);
          WiFi.begin(ssid.c_str(), password, max_rssi_channel, max_rssi_bssid, true);
        }
      }
    }
    else{
      if (!ustanowione){
        client.disconnect();
        WiFi.setAutoReconnect(false);
        WiFi.disconnect(false);
        WiFi.setAutoReconnect(true);
        WiFi.begin(ssid.c_str(), password);
      }
    }
    
  }
  else{
    if (!ustanowione){
      client.disconnect();
      WiFi.setAutoReconnect(false);
      WiFi.disconnect(false);
      WiFi.setAutoReconnect(true);
      WiFi.begin(ssid.c_str(), password);
    }
  }
  
  WiFi.scanDelete();  
}


void InsertData(byte fundata[], size_t size){
  byte mqdata[37] = {};
  for(int bajt = 0; bajt < size; bajt++){
    mqdata[bajt] = fundata[bajt];
  }
    set_tmp = mqdata[B_SET_TMP]+16;
    cur_tmp = mqdata[B_CUR_TMP];
    old_tmp = olddata[B_CUR_TMP];
    Mode = mqdata[B_MODE];
    fan_spd = mqdata[B_FAN_SPD];
    swing = mqdata[B_SWING];
    power = (int)bitRead( mqdata[B_POWER], 0);
    oldpower = (int)bitRead( olddata[B_POWER], 0);
    lock_rem = mqdata[B_LOCK_REM];
    fresh = (int)bitRead( mqdata[B_FRESH], 0);
    light = (int)bitRead( mqdata[B_FRESH], 5);
    noise = (int)bitRead( mqdata[B_FRESH], 1) + 2*(int)bitRead( mqdata[B_FRESH], 2);
    health = (int)bitRead( mqdata[B_POWER], 3);
    health_airflow = (int)bitRead( mqdata[B_FRESH], 3) + 2*(int)bitRead( mqdata[B_FRESH], 4);
    compressor = (int)bitRead( mqdata[B_POWER], 4);
    

  if (tempoffsetmode == -1){
    if (Mode == 0x02){
      tempoffsetmode = 1;
    }
    else{
      tempoffsetmode = 2;
    }
  }

  /////////////////////////////////
  if (fresh == 0x00){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Fresh", "off", true);
  }
  if (fresh == 0x01){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Fresh", "on", true);
  }
  /////////////////////////////////
  if (light == 0x00){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Light", "on", true);
  }
  if (light == 0x01){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Light", "off", true);
  }
  /////////////////////////////////
  if (noise == 0x00){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Quiet", "off", true);
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Turbo", "off", true);
  }
  if (noise == 0x01){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Quiet", "off", true);
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Turbo", "on", true);
  }
  if (noise == 0x02){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Quiet", "on", true);
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Turbo", "off", true);
  }
  if (noise == 0x03){ //WTF??
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Quiet", "on", true);
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Turbo", "on", true);
  }  
  /////////////////////////////////
  if (health == 0x00){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Ionizer", "off", true);
  }
  if (health == 0x01){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Ionizer", "on", true);
  }
  /////////////////////////////////
  if (health_airflow == 0x00){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Airflow", "off", true);
  }
  if (health_airflow == 0x01){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Airflow", "up", true);
  }
  if (health_airflow == 0x02){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Airflow", "down", true);
  }
  if (health_airflow == 0x03){ //WTF??
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Airflow", "unknown", true);
  }
  /////////////////////////////////
  if (lock_rem == 0x80){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Lock_Remote", "true", true);
  }
  if (lock_rem == 0x00){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Lock_Remote", "false", true);
  }
  /////////////////////////////////
  if (compressor == 0x00){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Compressor", "off", true);
  }
  if (compressor == 0x01){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Compressor", "on", true);
  }
  /////////////////////////////////
  if (power == 0x01){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Power", "on", true);
  }
  if (power == 0x00){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Power", "off", true);
  }
  /////////////////////////////////
  if (swing == 0x00){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Swing", "brak", true);
  }
  if (swing == 0x01){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Swing", "pionowo", true);
  }
  if (swing == 0x02){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Swing", "poziomo", true);
  }
  if (swing == 0x03){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Swing", "oba", true);
  }
  /////////////////////////////////  
  if (fan_spd == 0x00){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Fan_Speed", "maksymalny", true);
  }
  if (fan_spd == 0x01){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Fan_Speed", "normalny", true);
  }
  if (fan_spd == 0x02){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Fan_Speed", "minimalny", true);
  }
  if (fan_spd == 0x03){
      client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Fan_Speed", "auto", true);
  }
  /////////////////////////////////
  char b[5];
  char ba[5];
  char baa[5];
  String char_set_tmp = String(set_tmp);
  char_set_tmp.toCharArray(b,5);
  client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Set_Temp", b, true);
  ////////////////////////////////////
  if((cur_tmp - old_tmp) > 2){
    tempoffsetmode = 2;
  }
  if((cur_tmp - old_tmp) < -2){
    tempoffsetmode = 1;
  }
  
  int cur_tmp_adjusted = cur_tmp;
  int cur_tmp_adjusted_always = cur_tmp;
  if(tempoffsetmode == 1){
    if(power == 0x00){
      cur_tmp_adjusted += 2;
    }
    cur_tmp_adjusted_always += 2;
  }
  if(tempoffsetmode == 2){
    if(power == 0x00){
      cur_tmp_adjusted -= 2;
    }
    cur_tmp_adjusted_always -= 2;
  }
  String char_cur_tmp = String(cur_tmp);
  char_cur_tmp.toCharArray(b,5);
  String char_cur_tmp_adjusted = String(cur_tmp_adjusted);
  char_cur_tmp_adjusted.toCharArray(ba,5);
  String char_cur_tmp_adjusted_always = String(cur_tmp_adjusted_always);
  char_cur_tmp_adjusted_always.toCharArray(baa,5);
  client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Current_Temp", b, true);
  client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Current_Temp_Real", ba, true);
  client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Current_Temp_Adjusted", baa, true);

  if (oldpower == 1 && power == 0){
    if (tempoffsetmode == 2 && Mode == 0x02){
      fixmode = 1;
      //client.publish(PREFIX"/klimatyzator/"NUMBER"/set/rawMode", "cool");
    }
    if(tempoffsetmode == 1 && Mode == 0x01){
      fixmode = 2;
      //client.publish(PREFIX"/klimatyzator/"NUMBER"/set/rawMode", "heat");
    }
  }
  ////////////////////////////////////
  if (power == 0x00){
    client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Mode", "off", true);
  }
  else{
    if (Mode == 0x00){
        client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Mode", "auto", true);
    }
    if (Mode == 0x01){
        client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Mode", "cool", true);
    }
    if (Mode == 0x02){
        client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Mode", "heat", true);
    }
    if (Mode == 0x03){
        client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Mode", "fan_only", true);
    }
    if (Mode == 0x04){
        client.publish(PREFIX"/klimatyzator/"NUMBER"/get/Mode", "dry", true);
    }
  }
  
  String raw_str;
  char raw[75];
  for (int i=0; i < 37; i++){
     if (mqdata[i] < 0x10){
       raw_str += "0";
       raw_str += String(mqdata[i], HEX);
     } else {
      raw_str += String(mqdata[i], HEX);
     }    
  }
  raw_str.toUpperCase();
  raw_str.toCharArray(raw,75);
  client.publish(PREFIX"/klimatyzator/"NUMBER"/get/RAW", raw, true);

  
///////////////////////////////////
}

byte getCRC(byte req[], size_t size){
  byte crc = 0;
  for (int i=2; i < size; i++){
      crc += req[i];
  }
  return crc;
}

void SendData(byte req[], size_t size){
  //Serial.write(start, 2);
  Serial.write(req, size - 1);
  Serial.write(getCRC(req, size-1));
}

inline unsigned char toHex( char ch ){
   return ( ( ch >= 'A' ) ? ( ch - 'A' + 0xA ) : ( ch - '0' ) ) & 0x0F;
}

void callback(char* topic, byte* payload, unsigned int length) {
  bool kontynnuj = false;
  if (firsttransfer){
    return;
  }
  if(unclean == 0){
  for (int iter = 0; iter < 6; iter++){
    for (int bajt = 0; bajt < 37; bajt++){
      packdata[bajt] = olddata[bajt];
    }
    if(getCRC(packdata, (sizeof(packdata)/sizeof(byte))-1) == packdata[36]){
      kontynnuj = true;
    }
    if(kontynnuj){
      break;
    }
  }
  }
  else{
    kontynnuj = true;
  }
  if(kontynnuj){
  byte payloadcpy[length+1];
  for (int petl = 0; petl < length; petl++){
    payloadcpy[petl] = payload[petl];
  }
  payloadcpy[length] = '\0';
  String strTopic = String(topic);
  String strPayload = String((char*)payloadcpy);
  ///////////
  if (strTopic == PREFIX"/klimatyzator/"NUMBER"/set/Set_Temp"){
    if ( (strPayload.toInt() >= 16) && (strPayload.toInt() <= 30) ){
      packdata[B_SET_TMP] = strPayload.toInt()-16;      
    }
  }
  //////////
  if (strTopic == PREFIX"/klimatyzator/"NUMBER"/set/Mode"){
    if (strPayload == "off"){
      SendData(off, sizeof(off)/sizeof(byte));
      return;
    }
    if (strPayload == "auto"){
      bitSet( packdata[B_POWER], 0);
        packdata[B_MODE] = 0; 
    }
    if (strPayload == "cool"){
      bitSet( packdata[B_POWER], 0);
        packdata[B_MODE] = 1;
    }
    if (strPayload == "heat"){
      bitSet( packdata[B_POWER], 0);
        packdata[B_MODE] = 2; 
    }
    if (strPayload == "fan_only"){
      bitSet( packdata[B_POWER], 0);
        packdata[B_MODE] = 3;
    }
    if (strPayload == "dry"){
      bitSet( packdata[B_POWER], 0);
        packdata[B_MODE] = 4;
    }
  }
  //////////
  if (strTopic == PREFIX"/klimatyzator/"NUMBER"/set/rawMode"){
    if (strPayload == "auto"){
        packdata[B_MODE] = 0; 
    }
    if (strPayload == "cool"){
        packdata[B_MODE] = 1;
    }
    if (strPayload == "heat"){
        packdata[B_MODE] = 2; 
    }
    if (strPayload == "fan_only"){
        packdata[B_MODE] = 3;
    }
    if (strPayload == "dry"){
        packdata[B_MODE] = 4;
    }
  }
  //////////
  if (strTopic == PREFIX"/klimatyzator/"NUMBER"/set/Fan_Speed"){
    if (strPayload == "maksymalny"){
        packdata[B_FAN_SPD] = 0; 
        bitClear( packdata[B_FRESH], 1);
        bitClear( packdata[B_FRESH], 2);
    }
    if (strPayload == "normalny"){
        packdata[B_FAN_SPD] = 1;
        bitClear( packdata[B_FRESH], 1);
        bitClear( packdata[B_FRESH], 2);
    }
    if (strPayload == "minimalny"){
        packdata[B_FAN_SPD] = 2; 
        bitClear( packdata[B_FRESH], 1);
        bitClear( packdata[B_FRESH], 2);
    }
    if (strPayload == "auto"){
        packdata[B_FAN_SPD] = 3; 
    }
  }
  ////////
  if (strTopic == PREFIX"/klimatyzator/"NUMBER"/set/Swing"){
    if (strPayload == "brak"){
        packdata[B_SWING] = 0; 
    }
    if (strPayload == "pionowo"){
        packdata[B_SWING] = 1;
        bitClear( packdata[B_FRESH], 4);
        bitClear( packdata[B_FRESH], 3);
    }
    if (strPayload == "poziomo"){
        packdata[B_SWING] = 2; 
    }
    if (strPayload == "oba"){
        packdata[B_SWING] = 3; 
        bitClear( packdata[B_FRESH], 4);
        bitClear( packdata[B_FRESH], 3);
    }
  }
  ////////
  if (strTopic == PREFIX"/klimatyzator/"NUMBER"/set/Lock_Remote"){
    if (strPayload == "true"){
        packdata[B_LOCK_REM] = 80;
    }
    if (strPayload == "false"){
        packdata[B_LOCK_REM] = 0;
    }
  }
  ////////
  if (strTopic == PREFIX"/klimatyzator/"NUMBER"/set/Power"){
    if (strPayload == "off" || strPayload == "false" || strPayload == "0"){
      SendData(off, sizeof(off)/sizeof(byte));
      return;
    }
    if (strPayload == "on" || strPayload == "true" || strPayload == "1"){
      SendData(on, sizeof(on)/sizeof(byte));
      return;
    }
  }
  ////////
  if (strTopic == PREFIX"/klimatyzator/"NUMBER"/set/Ionizer"){
    if (strPayload == "on"){
        bitSet( packdata[B_POWER], 3);
    }
    if (strPayload == "off"){
        bitClear( packdata[B_POWER], 3);
    }
  }
  ////////
  if (strTopic == PREFIX"/klimatyzator/"NUMBER"/set/Fresh"){
    if (strPayload == "on"){
        bitSet( packdata[B_FRESH], 0);
    }
    if (strPayload == "off"){
        bitClear( packdata[B_FRESH], 0);
    }
  }
  ////////
  if (strTopic == PREFIX"/klimatyzator/"NUMBER"/set/Light"){
    if (strPayload == "off"){
      bitSet( packdata[B_FRESH], 5);
    }
    if (strPayload == "on"){
      bitClear( packdata[B_FRESH], 5);
    }
  }
  ////////
  if (strTopic == PREFIX"/klimatyzator/"NUMBER"/set/Airflow"){
    if (strPayload == "off"){
        bitClear( packdata[B_FRESH], 4);
        bitClear( packdata[B_FRESH], 3);
    }
    if (strPayload == "up"){
        bitClear( packdata[B_FRESH], 4);
        bitSet( packdata[B_FRESH], 3);
        if(packdata[B_SWING] == 1){
          packdata[B_SWING] = 0;
        }
        if(packdata[B_SWING] == 3){
          packdata[B_SWING] = 2;
        }
    }
    if (strPayload == "down"){
        bitSet( packdata[B_FRESH], 4);
        bitClear( packdata[B_FRESH], 3);
        if(packdata[B_SWING] == 1){
          packdata[B_SWING] = 0;
        }
        if(packdata[B_SWING] == 3){
          packdata[B_SWING] = 2;
        }
    }
  }
  ////////
  if (strTopic == PREFIX"/klimatyzator/"NUMBER"/set/Turbo"){
    if (strPayload == "on"){
        bitSet( packdata[B_FRESH], 1);
        bitClear( packdata[B_FRESH], 2);
        packdata[B_FAN_SPD] = 3;
    }
    if (strPayload == "off"){
      
        bitClear( packdata[B_FRESH], 1);
    }
  }
  ////////
  if (strTopic == PREFIX"/klimatyzator/"NUMBER"/set/Quiet"){
    if (strPayload == "on"){
        bitSet( packdata[B_FRESH], 2);
        bitClear( packdata[B_FRESH], 1);
        packdata[B_FAN_SPD] = 3;
    }
    if (strPayload == "off"){
      
        bitClear( packdata[B_FRESH], 2);
    }
  }
  ////////
  bool wyslac = false;
  for (int bajt=0; bajt < 37; bajt++){
    if (packdata[bajt] != olddata[bajt]){
      wyslac = true;
    }
  }
  packdata[B_CMD] = 0;
  packdata[9] = 1;
  packdata[10] = 77;
  packdata[11] = 95;
  
  if(wyslac){
    SendData(packdata, sizeof(packdata)/sizeof(byte));
    if (transferinprog){
      unclean = 2;
    }
    else{
      unclean = 1;
    }
  }
  }
}

void setup() {
  Serial.begin(9600);
  
  WiFi.mode(WIFI_STA);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.hostname(ID_CONNECT);

  espClient.setTrustAnchors(&caCertX509);
  espClient.setX509Time(1564991524);
  
  client.setServer(mqtt_server, 8883);
  client.setCallback(callback);
}

void loop() {
  if(Serial.available() > 0){
    transferinprog = true;
    Serial.readBytes(data, 37);
    while(Serial.available()){
      delay(2);
      Serial.read();
    }
    transferinprog = false;
    if(getCRC(data, (sizeof(data)/sizeof(byte))-1) == data[36]){
      bool nowy = false;
      for(int bajt = 0; bajt < 37; bajt++){
        if(data[bajt] != olddata[bajt]){
          nowy = true;
        }
      }
      if(nowy){
        if (firsttransfer){
          for(int bajt = 0; bajt < 37; bajt++){
            olddata[bajt] = data[bajt];
          }
        }
        InsertData(data, 37);
        for(int bajt = 0; bajt < 37; bajt++){
          olddata[bajt] = data[bajt];
        }
      }
      firsttransfer = false;
      if (unclean == 2){
        unclean = 1;
      }
      if (unclean == 1){
        unclean = 0;
      }
    }
  }
  
  if (fixmode == 1){
    fixmode = -1;
    char* tpcfx = PREFIX"/klimatyzator/"NUMBER"/set/rawMode";
    char* msgfx = "cool";
    callback(tpcfx, (byte*)msgfx, 4);
  }
  if (fixmode == 2){
    fixmode = -1;
    char* tpcfx = PREFIX"/klimatyzator/"NUMBER"/set/rawMode";
    char* msgfx = "heat";
    callback(tpcfx, (byte*)msgfx, 4);
  }
  client.loop();

  unsigned long now = millis();
  
  if ((unsigned long)(now - previous_wifi_scan) > wifi_scan_interval){
    previous_wifi_scan = now;
    if (WiFi.scanComplete() != -1){
      scan_wifi_init();
    }
  }

  if ((unsigned long)(now - previous_wifi_conn) > wifi_conn_interval){
    previous_wifi_conn = now;
    if ( !WiFi.isConnected() || (WiFi.status() != WL_CONNECTED) ){
      if (WiFi.scanComplete() != -1){
        scan_wifi_init();
      }
    }
  }

  if ((unsigned long)(now - previous_wifi_process) > wifi_process_interval){
    previous_wifi_process = now;
    if (WiFi.scanComplete() >= 0){
      scan_wifi_process();
    }
  }

  if ((unsigned long)(now - previous_mqtt_conn) > mqtt_conn_interval){
    previous_mqtt_conn = now;
    if ( !client.connected() ){
      if ( WiFi.isConnected() && (WiFi.status() == WL_CONNECTED) ){
        reconnect();
      }
    }
  }
  
  if ((unsigned long)(now - previous_poll) > poll_interval) {
    previous_poll = now;
    SendData(qstn, sizeof(qstn)/sizeof(byte)); //polling
  }
  delay(500);
}
