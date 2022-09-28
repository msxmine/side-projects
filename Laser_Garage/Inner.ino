#include <ESP8266WiFi.h>
//#include <ESP8266mDNS.h>
//#include <WiFiUdp.h>
//#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "vl53l1_api.h"

#define PREFIX "home-assistant/garaz"
#define NUMBER "1"
#define NAME   "brama-garaz-1"

/* Certyfikat */
const char caCert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
DEADBEEF
-----END CERTIFICATE-----
)EOF";

const String ssid = "Deadbeef";
const char* password = "Deadbeef";
const char* mqtt_server = "mqtt-server.lan";
const char* mqtt_login = "Deadbeef";
const char* mqtt_password = "Deadbeef";

unsigned long previous_wifi_scan = 0;
unsigned long previous_wifi_process = 0;
unsigned long previous_distance_scan = 0;
unsigned long previous_handling = 0;
unsigned long previous_remote = 0;
unsigned long previous_output = 0;
unsigned long previous_wlanconn = 0;
unsigned long previous_mqttconn = 0;
const unsigned long wifi_scan_interval = 86400000;
const unsigned long wifi_process_interval = 120000;
const unsigned long distance_scan_interval = 50;
const unsigned long handling_interval = 50;
const unsigned long remote_interval = 10;
const unsigned long output_interval = 10;
const unsigned long wlanconn_interval = 300000;
const unsigned long mqttconn_interval = 120000;

const int position_open = 535; //530
const int position_closed = 2650;
const float movement_coef_up = -30.0;
const float movement_coef_down = 30.0;
const int endstop_marigin = 50;
const int endstop_marigin_small = 20;

unsigned long now = 0;
unsigned long last_button_press = 0;

unsigned long last_distance_scan = 0;
unsigned long last_distance_data_change = 0;

int previous_returnCode = -1;
int previous_wynikPomiaru = -1;
float previous_signalRate = -1.0;
float previous_noiseRate = -1.0;

int current_gate_state = 0; //-1 zamykanie, 0 stop, 1 otwieranie
int current_task = -99; //-99 brak, -1 zamykanie, 0 stop, 1 otwieranie
int iteration = 1;
int positions[8] = {0};
bool positions_initialized = false;
const int lookback_period = 4;
const int sample_storage = 8;
float procent_old = 0;

bool output_in_progress = true;

unsigned long handle_wait_since = 0;
unsigned long handle_wait_for = 0;
bool handle_wait_enable = false;

int gate_suspected_state = 1; //1 --readyToOpen , 2 --Opening , 3 --readyToClose, 4 --Closing

int previousPinState = 1;
int pinStateCounter = 0;
int pinStateExecuted = 1;
int pilotIn = 1;

unsigned long next_pin_up_time = 0;
unsigned long next_pin_down_time = 0;
int pin_up_requests = 0;
int pin_down_requests = 0;
const unsigned long pin_up_duration = 200; //400
const unsigned long pin_down_duration = 200;

X509List caCertX509(caCert);
WiFiClientSecure espClient;
PubSubClient client(espClient);

VL53L1_Dev_t device;
VL53L1_DEV Czujnik = &device;
VL53L1_UserRoi_t roiSetting;

void reconnect(){
  if (client.connect(NAME , mqtt_login , mqtt_password)) {
    client.subscribe(PREFIX"/brama/"NUMBER"/set");
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

void addToSuspectState(){
  if (gate_suspected_state == 4){
    gate_suspected_state = 1;
  }
  else{
    gate_suspected_state++;
  }
}

void toggleButton(int times = 1, unsigned long additional_delay = 0){
    now = millis();

    //Serial.print("Naciskam ");
    //Serial.print(times);
    //Serial.print("\n");

    unsigned long ondelay = 0;

    if ( (unsigned long)(now - last_button_press) < (unsigned long)(pin_up_duration)){
      ondelay = (unsigned long)( pin_up_duration - (unsigned long)(now - last_button_press) );
    }

    pin_down_requests = 1;
    pin_up_requests = times;
    next_pin_down_time = (unsigned long)(now + (unsigned long)(ondelay) );
    next_pin_up_time = (unsigned long)(now + (unsigned long)(ondelay) + (unsigned long)(pin_down_duration) + additional_delay);

    output_in_progress = true;
}

void stateUpdater(){
  now = millis();
  
  int returnCode = -1;
  int wynikPomiaru = -1;
  float signalRate = -1.0;
  float noiseRate = -1.0;
  
  int status_czujnika;
  static byte dataReady;
  dataReady = 0;
  status_czujnika = VL53L1_GetMeasurementDataReady(Czujnik, &dataReady);
  
  if (!status_czujnika){
    if (dataReady){
    static VL53L1_RangingMeasurementData_t RangingData;
  
    status_czujnika = VL53L1_GetRangingMeasurementData(Czujnik, &RangingData);
    if (!status_czujnika ){
      returnCode = RangingData.RangeStatus;
      wynikPomiaru = RangingData.RangeMilliMeter;
      signalRate = RangingData.SignalRateRtnMegaCps/65536.0;
      noiseRate = RangingData.AmbientRateRtnMegaCps/65536.0;
    }
    else {
      Serial.print(F("Nie mozna odczytac wyniku pomiaru"));
    }

    
    status_czujnika = VL53L1_ClearInterruptAndStartMeasurement(Czujnik);
    if ( status_czujnika ){
      Serial.print(F("Nie mozna zakonczyc odczytu pomiaru"));
    }
  
    if (wynikPomiaru == -1){
      Serial.print(F("Blad odczytu pomiaru"));
      return;
    }

    last_distance_scan = now;

    if (previous_returnCode != returnCode){
      previous_returnCode = returnCode;
    }
    if (previous_wynikPomiaru != wynikPomiaru){
      previous_wynikPomiaru = wynikPomiaru;
      last_distance_data_change = now;
    }
    if (previous_signalRate != signalRate){
      previous_signalRate = signalRate;
      last_distance_data_change = now;
    }
    if (previous_noiseRate != noiseRate){
      previous_noiseRate = noiseRate;
      last_distance_data_change = now;
    }
    
  
    if(!positions_initialized){
      for (int i = 0; i < (sample_storage); i++){
        positions[i] = wynikPomiaru;
      }
      positions_initialized = true;
    }
  
    for (int i = 0; i < (sample_storage - 1); i++){
      positions[i] = positions[i+1];
    }
    positions[(sample_storage - 1)] = wynikPomiaru;
  
    float srednia = 0;
    for (int i = (sample_storage - 2); i < (sample_storage); i++){
      srednia += positions[i];
    }
    srednia = srednia/(2);
    float procent;
    procent = srednia - position_open;
    procent = ((procent / (position_closed - position_open))*100);
    if(procent < 0){procent = 0;}
    if(procent > 100){procent = 100;}
    procent = 100 - procent;
  
    float parametrCzasu[lookback_period];
    float parametrWartosci[lookback_period];
  
    for (int i = 0; i < lookback_period; i++){
      parametrCzasu[i] = (i+1)*(1.0);
    }
  
    int poz = 0;
    for (int i = (sample_storage - lookback_period); i < (sample_storage); i++){
      parametrWartosci[poz] = (positions[i])*(1.0);
      poz++;
    }
  
    float wynikKorelacji[2];
    wynikKorelacji[0] = 0.0;
    wynikKorelacji[1] = 0.0;
  
    simpLinReg(parametrCzasu, parametrWartosci, wynikKorelacji, lookback_period);
  
    now = millis();
    if ((unsigned long)(now - last_button_press) > (unsigned long)(3000)){
      
    
    
    if (wynikPomiaru < position_open){
      gate_suspected_state = 3;
    }
    if (wynikPomiaru > position_closed){
      gate_suspected_state = 1;
    }
  
    }
    
  
    if(wynikKorelacji[0] < movement_coef_up){
      current_gate_state = 1;
      //client.publish(PREFIX"/brama/"NUMBER"/get/Movement", "OPENING", true);
    }
    if(wynikKorelacji[0] > movement_coef_down){
      current_gate_state = -1;
      //client.publish(PREFIX"/brama/"NUMBER"/get/Movement", "CLOSING", true);
    }
    if(wynikKorelacji[0] >= movement_coef_up && wynikKorelacji[0] <= movement_coef_down){
      current_gate_state = 0;
      //client.publish(PREFIX"/brama/"NUMBER"/get/Movement", "STOPPED", true);
    }

    int procent_integer;
    String pozycja_procent;
    procent_integer = (int)procent;

    if (procent_integer > 100){
      procent_integer = 100;
    }
    if (procent_integer < 0){
      procent_integer = 0;
    }
    pozycja_procent = procent_integer;
    if (procent != procent_old){
      client.publish(PREFIX"/brama/"NUMBER"/get/Position", pozycja_procent.c_str(), true);
      procent_old = procent;
    }

    /*
    String wynik;
    if (returnCode){
      wynik += "BLAD: ";
      wynik += returnCode;
      wynik += " ,";
    }
    
    wynik += "Wynik: ";
    wynik += (wynikPomiaru / 10.0);
    wynik += " cm";
  
    wynik += " ,Sila sygnalu: ";
    wynik += signalRate;
    
    wynik += " ,Sila szumu: ";
    wynik += noiseRate;
    
    client.publish(PREFIX"/brama/"NUMBER"/get/Reading", wynik.c_str(), true);
    
    
    wynik = "";
    wynik += " ,Srednia: ";
    wynik += srednia / 10.0;
    wynik += " cm";
  
    wynik += " , Wsplcz: ";
    wynik += wynikKorelacji[0];
  
    client.publish(PREFIX"/brama/"NUMBER"/get/Reading", wynik.c_str(), true);
    */
    }
  }
  else{
    Serial.print(F("Nie wiadomo czy czujnik gotowy"));
  }

  if ( (unsigned long)(now - last_distance_data_change) > (unsigned long)(300000) ){
    Serial.print(F("Resetowanie czujnika"));
    sensorReset();
  }
  
}

void requestHandler(){
  now = millis();
  if (output_in_progress){
    handle_wait_since = now;
  }
  else {
    if ( ((unsigned long)(now - handle_wait_since) > handle_wait_for) || (handle_wait_enable == 0) ){
      
      handle_wait_enable = 0;
      
      //Zatrzymaj
      if (current_task == 0){
        if (iteration == 1){
          //Jesli ostatnio sie zamykala, lub otwierala, nacisnij
          if (gate_suspected_state == 2 || gate_suspected_state == 4){
            toggleButton();
          }
        }
  
        if (iteration == 2){
          //Jesli sie rusza
          if (current_gate_state != 0){
            //Jesli nie jest blisko koncow nacisnij
            if (!((positions[sample_storage-1] > (position_closed - endstop_marigin)) || (positions[sample_storage-1] < (position_open + endstop_marigin)))){
              toggleButton();
              if (current_gate_state == 1){
                gate_suspected_state = 3;
              }
              if (current_gate_state == -1){
                gate_suspected_state = 1;
              }
            }    
          }
        }
        
        if (iteration == 1){
          handle_wait_since = now;
          handle_wait_for = (unsigned long)(4500);
          handle_wait_enable = true;
          iteration = 2;
        }
        else{
          current_task = -99;
          iteration = 1;
        }
      }
      //Zamknij
      if (current_task == -1){
        //jesli nie jest blisko dolu
        if ( !(positions[sample_storage-1] > (position_closed - endstop_marigin_small) ) ){
  
          if (iteration == 1){
            if (gate_suspected_state == 1){
              toggleButton(3);
            }
            if (gate_suspected_state == 2){
              toggleButton(2);
            }
            if (gate_suspected_state == 3){
              toggleButton();
            }
          }
  
          if (iteration == 2){
            if (current_gate_state == 0){
              toggleButton();
              gate_suspected_state = 4;
            }
            if (current_gate_state == 1){
              if (positions[sample_storage-1] < (position_open + endstop_marigin)){
                toggleButton(1, 3000);
              }
              else{
                toggleButton(2);
              }
              gate_suspected_state = 4;
            }
            if (current_gate_state == -1){
              iteration = 4;
            }
          }
  
          if (iteration == 3){
            if (current_gate_state == 0){
              toggleButton();
              gate_suspected_state = 4;
            }
            if (current_gate_state == 1){
              if (positions[sample_storage-1] < (position_open + endstop_marigin)){
                toggleButton(1, 3000);
              }
              else{
                toggleButton(2);
              }
              gate_suspected_state = 4;
            }
          }
        }
        if (iteration == 1 || iteration == 2){
          handle_wait_since = now;
          handle_wait_for = (unsigned long)(4500);
          handle_wait_enable = true;
          iteration++;
        }
        else{
          current_task = -99;
          iteration = 1;
        }
      }
      //Otworz
      if (current_task == 1){
        if ( !(positions[sample_storage-1] < (position_open + endstop_marigin_small) ) ){
          if (iteration == 1){
            if (gate_suspected_state == 3){
              toggleButton(3);
            }
            if (gate_suspected_state == 4){
              toggleButton(2);
            }
            if (gate_suspected_state == 1){
              toggleButton();
            }
          }
  
          if (iteration == 2){
            if (current_gate_state == 0){
              toggleButton();
              gate_suspected_state = 2;
            }
            if (current_gate_state == -1){
              if (positions[sample_storage-1] > (position_closed - endstop_marigin)){
                toggleButton(1, 3000);
              }
              else{
                toggleButton(2);
              }
              gate_suspected_state = 2;
            }
            if (current_gate_state == 1){
              iteration = 4;
            }
          }
  
          if (iteration == 3){
            if (current_gate_state == 0){
              toggleButton();
              gate_suspected_state = 2;
            }
            if (current_gate_state == -1){
              if (positions[sample_storage-1] > (position_closed - endstop_marigin)){
                toggleButton(1, 3000);
              }
              else{
                toggleButton(2);
              }
              gate_suspected_state = 2;
            }
          }
        }
        if (iteration == 1 || iteration == 2){
          handle_wait_since = now;
          handle_wait_for = (unsigned long)(4500);
          handle_wait_enable = true;
          iteration++;
        }
        else{
          current_task = -99;
          iteration = 1;
        }
      }
    }
  }
}


void callback(char* topic, byte* payload, unsigned int length) {
  byte payloadcpy[length+1];
  for (int i = 0; i < length; i++){
    payloadcpy[i] = payload[i];
  }
  payloadcpy[length] = '\0';
  String strTopic = String(topic);
  String strPayload = String((char*)payloadcpy);

  if (strTopic == PREFIX"/brama/"NUMBER"/set"){
    if (strPayload == "toggle"){
      toggleButton();
      handle_wait_enable = false;
      iteration = 1;
      current_task = -99;
    }
    if (strPayload == "OPEN"){
      toggleButton(0);
      handle_wait_enable = false;
      iteration = 1;
      current_task = 1;
    }
    if (strPayload == "CLOSE"){
      toggleButton(0);
      handle_wait_enable = false;
      iteration = 1;
      current_task = -1;
    }
    if (strPayload == "STOP"){
      toggleButton(0);
      handle_wait_enable = false;
      iteration = 1;
      current_task = 0;
    }
  }
}

void handleRemote(){
  int pinState = 1;
  pinState = digitalRead(D1);

  if(pinState != previousPinState){
    pinStateCounter = 0;
    previousPinState = pinState;
  }

  if (pinStateCounter < 1000){
    pinStateCounter++;
  }

  if (pinStateCounter == 6){
    pilotIn = previousPinState;
    pinStateExecuted = 0;
  }

  if (pilotIn == 0){
    if (!pinStateExecuted){
      pinStateExecuted = 1;
      toggleButton();
      current_task = -99;
      iteration = 1;
      handle_wait_enable = false;
    }
  }
}

void updateOutputs(){
  now = millis();
  if ( (pin_up_requests > 0) && (pin_down_requests == 0) ){
    if ( (unsigned long)(next_pin_up_time - now) > (unsigned long)(999999) ){
      pin_up_requests -= 1;
      digitalWrite(D2, LOW);

      next_pin_up_time = (unsigned long)(now + pin_up_duration + pin_down_duration);
      next_pin_down_time = (unsigned long)(now + pin_up_duration);
      pin_down_requests = 1;
      
      last_button_press = now;
      addToSuspectState();
    }
  }
  if ( (pin_down_requests > 0) ){
    if ( (unsigned long)(next_pin_down_time - now) > (unsigned long)(999999) ){
      pin_down_requests = 0;
      digitalWrite(D2, HIGH);
      
      if ( ((unsigned long)(next_pin_up_time) - (unsigned long)(now + pin_down_duration)) > (unsigned long)(999999999) ){
        next_pin_up_time = (unsigned long)(now + pin_down_duration);
      }
    }
  }
  if ( (pin_up_requests > 0) || (pin_down_requests > 0) ){
    output_in_progress = true; 
  }
  else{
    output_in_progress = false;
  }
}


void sensorReset(){
  Wire.begin(D4, D3);
  Wire.setClock(400000);
  Wire.status();
  Wire.begin(D4, D3);

  int status_czujnika = 0;
  
  Czujnik->I2cDevAddr = 0x52;

  roiSetting.TopLeftX = 3; //sufit
  roiSetting.TopLeftY = 15; //sciana
  roiSetting.BotRightX = 6; //podloga
  roiSetting.BotRightY = 0; //garaz

  VL53L1_software_reset(Czujnik);

  status_czujnika += VL53L1_WaitDeviceBooted(Czujnik);
  status_czujnika += VL53L1_DataInit(Czujnik);
  status_czujnika += VL53L1_StaticInit(Czujnik);
  status_czujnika += VL53L1_SetDistanceMode(Czujnik, VL53L1_DISTANCEMODE_MEDIUM);
  status_czujnika += VL53L1_SetMeasurementTimingBudgetMicroSeconds(Czujnik, 1000000);
  status_czujnika += VL53L1_SetUserROI(Czujnik, &roiSetting);
  status_czujnika += VL53L1_SetLimitCheckValue(Czujnik, VL53L1_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, (FixPoint1616_t)(15 * 65536 / 100));
  status_czujnika += VL53L1_SetLimitCheckValue(Czujnik, VL53L1_CHECKENABLE_SIGMA_FINAL_RANGE, (FixPoint1616_t)(80 * 65536));
  status_czujnika += VL53L1_SetInterMeasurementPeriodMilliSeconds(Czujnik, 1100);
  status_czujnika += VL53L1_StartMeasurement(Czujnik);

  if(status_czujnika){
    Serial.println(F("Czujnik nie dziala"));
  }
}


void setup() {
  digitalWrite(D2, HIGH);
  pinMode(D2, OUTPUT);
  digitalWrite(D2, HIGH);
  pinMode(D1, INPUT_PULLUP);
  
  Serial.begin(115200);
  Serial.println("Booting");
  
  WiFi.mode(WIFI_STA);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.hostname(NAME);

  WiFi.begin(ssid.c_str(), password);

  espClient.setTimeout(1300);
  espClient.setTrustAnchors(&caCertX509);
  //espClient.allowSelfSignedCerts();
  espClient.setX509Time(1564991524);

  client.setServer(mqtt_server, 8883);
  client.setCallback(callback);
  client.setSocketTimeout(1);

  sensorReset();

  /*
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("garaz");
  ArduinoOTA.setPassword("msx");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  */
  Serial.println("Ready");
}

void loop() {
  //ArduinoOTA.handle();
  client.loop();
  
  now = millis();

  if ((unsigned long)(now - previous_wifi_scan) > wifi_scan_interval){
    previous_wifi_scan = now;
    if (WiFi.scanComplete() != -1){
      scan_wifi_init();
    }
  }

  if ((unsigned long)(now - previous_wifi_process) > wifi_process_interval){
    previous_wifi_process = now;
    if (WiFi.scanComplete() >= 0){
      scan_wifi_process();
    }
  }

  if ((unsigned long)(now - previous_wlanconn) > wlanconn_interval){
    previous_wlanconn = now;
    if ( !WiFi.isConnected() || (WiFi.status() != WL_CONNECTED)){
      if (WiFi.scanComplete() != -1){
        scan_wifi_init(); 
      }
    }
  }

  if ((unsigned long)(now - previous_mqttconn) > mqttconn_interval){
    previous_mqttconn = now;
    if( !client.connected() ){
      if ( WiFi.isConnected() && (WiFi.status() == WL_CONNECTED)){
        reconnect();
      }
    }
  }

  now = millis();

  if ((unsigned long)(now - previous_distance_scan) > distance_scan_interval){
    previous_distance_scan = now;
    stateUpdater();
  }

  if ((unsigned long)(now - previous_handling) > handling_interval){
    previous_handling = now;
    //Serial.print("STAN TO ");
    //Serial.print(gate_suspected_state);
    //Serial.print("\n");
    requestHandler();
  }

  if ((unsigned long)(now - previous_remote) > remote_interval){
    previous_remote = now;
    handleRemote();
  }

  if ((unsigned long)(now - previous_output) > output_interval){
    previous_output = now;
    updateOutputs();
  }

  delay(10);
}


void simpLinReg(float* x, float* y, float* lrCoef, int n){
  // pass x and y arrays (pointers), lrCoef pointer, and n.  The lrCoef array is comprised of the slope=lrCoef[0] and intercept=lrCoef[1].  n is length of the x and y arrays.
  // http://en.wikipedia.org/wiki/Simple_linear_regression

  // initialize variables
  float xbar=0;
  float ybar=0;
  float xybar=0;
  float xsqbar=0;
  
  // calculations required for linear regression
  for (int i=0; i<n; i++){
    xbar=xbar+x[i];
    ybar=ybar+y[i];
    xybar=xybar+x[i]*y[i];
    xsqbar=xsqbar+x[i]*x[i];
  }
  xbar=xbar/n;
  ybar=ybar/n;
  xybar=xybar/n;
  xsqbar=xsqbar/n;
  
  // simple linear regression algorithm
  lrCoef[0]=(xybar-xbar*ybar)/(xsqbar-xbar*xbar);
  lrCoef[1]=ybar-lrCoef[0]*xbar;
}
