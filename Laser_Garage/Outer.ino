#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>

extern "C" {
#include "lwip/dns.h"
}

#define PREFIX "home-assistant/ogrod"
#define NUMBER "1"
#define NAME   "brama-ogrod-1"

/* Certyfikat */
const char caCert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
DEADBEEF
-----END CERTIFICATE-----
)EOF";

const int PIN_LEWA_OPEN           = 4;
const int PIN_LEWA_CLOSE          = 16;
const int PIN_LEWA_STOP           = 17;
const int PIN_PRAWA_OPEN          = 18;
const int PIN_PRAWA_CLOSE         = 19;
const int PIN_PRAWA_STOP          = 21;

unsigned long PIN_LEWA_OPEN_UP_TIME   = 0ul;
unsigned long PIN_LEWA_CLOSE_UP_TIME  = 0ul;
unsigned long PIN_LEWA_STOP_UP_TIME   = 0ul;
unsigned long PIN_PRAWA_OPEN_UP_TIME  = 0ul;
unsigned long PIN_PRAWA_CLOSE_UP_TIME = 0ul;
unsigned long PIN_PRAWA_STOP_UP_TIME  = 0ul;

unsigned long PIN_LEWA_OPEN_DOWN_TIME   = 0ul;
unsigned long PIN_LEWA_CLOSE_DOWN_TIME  = 0ul;
unsigned long PIN_LEWA_STOP_DOWN_TIME   = 0ul;
unsigned long PIN_PRAWA_OPEN_DOWN_TIME  = 0ul;
unsigned long PIN_PRAWA_CLOSE_DOWN_TIME = 0ul;
unsigned long PIN_PRAWA_STOP_DOWN_TIME  = 0ul;

bool PIN_LEWA_OPEN_UP_REQ   = false;
bool PIN_LEWA_CLOSE_UP_REQ  = false;
bool PIN_LEWA_STOP_UP_REQ   = false;
bool PIN_PRAWA_OPEN_UP_REQ  = false;
bool PIN_PRAWA_CLOSE_UP_REQ = false;
bool PIN_PRAWA_STOP_UP_REQ  = false;

bool PIN_LEWA_OPEN_DOWN_REQ   = false;
bool PIN_LEWA_CLOSE_DOWN_REQ  = false;
bool PIN_LEWA_STOP_DOWN_REQ   = false;
bool PIN_PRAWA_OPEN_DOWN_REQ  = false;
bool PIN_PRAWA_CLOSE_DOWN_REQ = false;
bool PIN_PRAWA_STOP_DOWN_REQ  = false;

const int PIN_LEWA_STATE_OPEN     = 27;
const int PIN_LEWA_STATE_CLOSED   = 26;
const int PIN_PRAWA_STATE_OPEN    = 25;
const int PIN_PRAWA_STATE_CLOSED  = 33;
const int PIN_FOTOKOMORKA         = 32;
const int PIN_PILOT               = 23;

const unsigned long PIN_LEWA_STATE_OPEN_STABILISATION     = 50ul;
const unsigned long PIN_LEWA_STATE_CLOSED_STABILISATION   = 50ul;
const unsigned long PIN_PRAWA_STATE_OPEN_STABILISATION    = 50ul;
const unsigned long PIN_PRAWA_STATE_CLOSED_STABILISATION  = 50ul;
const unsigned long PIN_FOTOKOMORKA_STABILISATION         = 25ul;
const unsigned long PIN_PILOT_STABILISATION               = 50ul;

unsigned long PIN_LEWA_STATE_OPEN_LAST_CHANGE    = 0ul;
unsigned long PIN_LEWA_STATE_CLOSED_LAST_CHANGE  = 0ul;
unsigned long PIN_PRAWA_STATE_OPEN_LAST_CHANGE   = 0ul;
unsigned long PIN_PRAWA_STATE_CLOSED_LAST_CHANGE = 0ul;
unsigned long PIN_FOTOKOMORKA_LAST_CHANGE        = 0ul;
unsigned long PIN_PILOT_LAST_CHANGE              = 0ul;

bool PIN_LEWA_STATE_OPEN_LAST_STATE     = false;
bool PIN_LEWA_STATE_CLOSED_LAST_STATE   = false;
bool PIN_PRAWA_STATE_OPEN_LAST_STATE    = false;
bool PIN_PRAWA_STATE_CLOSED_LAST_STATE  = false;
bool PIN_FOTOKOMORKA_LAST_STATE         = false;
bool PIN_PILOT_LAST_STATE               = false;

bool INPUT_LEWA_STATE_OPEN    = false;
bool INPUT_LEWA_STATE_CLOSED  = false;
bool INPUT_PRAWA_STATE_OPEN   = false;
bool INPUT_PRAWA_STATE_CLOSED = false;
bool INPUT_FOTOKOMORKA        = false;
bool INPUT_PILOT              = false;

int STATUS_PRAWA = 0; // -2 : ZAMKNIETA, -1 : ZAMYKA, 0 : STOP, 1: OTWIERA, 2: OTWARTA
int STATUS_LEWA = 0;

int STATUS_LAST_MOVE_ACTION = 0; // 0: CLOSE, 1: OPEN

bool IGNORE_LEWA_OPEN     = false;
bool IGNORE_LEWA_CLOSED   = false;
bool IGNORE_PRAWA_OPEN    = false;
bool IGNORE_PRAWA_CLOSED  = false;

unsigned long IGNORE_LEWA_OPEN_SINCE    = 0ul;
unsigned long IGNORE_LEWA_CLOSED_SINCE  = 0ul;
unsigned long IGNORE_PRAWA_OPEN_SINCE   = 0ul;
unsigned long IGNORE_PRAWA_CLOSED_SINCE = 0ul;

bool PILOT_HANDLED     = true;

unsigned long LAST_ACTION_TIME = 0ul;
unsigned long AUTOCLOSE_DELAY = 45000ul;
unsigned long AUTOCLOSE_FOTO_DELAY = 15000ul;

const String ssid = "Deadbeef";
const char* password = "deadbeef";
const char* mqtt_server = "mqtt-server.lan";
IPAddress mqtt_ip(192, 168, 1, 200);
const char* mqtt_login = "deadbeef";
const char* mqtt_password = "deadbeef";

unsigned long now = 0ul;

const unsigned long wifi_scan_interval = 900000ul;
const unsigned long wifi_process_interval = 10000ul;
const unsigned long wifi_afterconn_delay = 15000ul;
const unsigned long wifi_conncheck_interval = 120000ul;
const unsigned long mqtt_conncheck_interval = 60000ul;
const unsigned long input_update_interval = 3ul;
const unsigned long output_update_interval = 3ul;
const unsigned long state_update_interval = 3ul;
const unsigned long autoclose_handle_interval = 3ul;
const unsigned long remote_handle_interval = 10ul;
const unsigned long send_position_interval = 500ul;

unsigned long previous_wifi_scan = 0ul;
unsigned long previous_wifi_process = 0ul;
unsigned long previous_wifi_establish = 0ul;
unsigned long previous_wifi_conncheck = 0ul;
unsigned long previous_mqtt_conncheck = 0ul;
unsigned long previous_input_update = 0ul;
unsigned long previous_output_update = 0ul;
unsigned long previous_state_update = 0ul;
unsigned long previous_autoclose_handle = 0ul;
unsigned long previous_remote_handle = 0ul;
unsigned long previous_send_position = 0ul;

int mqtt_connfail_times = 0;

unsigned long previous_update_position = 0ul;


long suspect_position_prawa = 0l;
long max_position_prawa = 196l;
long suspect_position_lewa = 0l;
long max_position_lewa = 131l;
int last_reported_procent = -1;
int last_reported_procent_prawa = -1;
int last_reported_procent_lewa = -1;

WiFiClientSecure espClient;
PubSubClient client(espClient);


void updateInputs(){
  now = millis();
  
  bool PIN_LEWA_STATE_OPEN_CURRENT     = digitalRead(PIN_LEWA_STATE_OPEN);
  bool PIN_LEWA_STATE_CLOSED_CURRENT   = digitalRead(PIN_LEWA_STATE_CLOSED);
  bool PIN_PRAWA_STATE_OPEN_CURRENT    = digitalRead(PIN_PRAWA_STATE_OPEN);
  bool PIN_PRAWA_STATE_CLOSED_CURRENT  = digitalRead(PIN_PRAWA_STATE_CLOSED);
  bool PIN_FOTOKOMORKA_CURRENT         = digitalRead(PIN_FOTOKOMORKA);
  bool PIN_PILOT_CURRENT               = digitalRead(PIN_PILOT);

  if (PIN_LEWA_STATE_OPEN_CURRENT != PIN_LEWA_STATE_OPEN_LAST_STATE){
    PIN_LEWA_STATE_OPEN_LAST_STATE = PIN_LEWA_STATE_OPEN_CURRENT;
    PIN_LEWA_STATE_OPEN_LAST_CHANGE = now;
  }

  if (PIN_LEWA_STATE_CLOSED_CURRENT != PIN_LEWA_STATE_CLOSED_LAST_STATE){
    PIN_LEWA_STATE_CLOSED_LAST_STATE = PIN_LEWA_STATE_CLOSED_CURRENT;
    PIN_LEWA_STATE_CLOSED_LAST_CHANGE = now;
  }

  if (PIN_PRAWA_STATE_OPEN_CURRENT != PIN_PRAWA_STATE_OPEN_LAST_STATE){
    PIN_PRAWA_STATE_OPEN_LAST_STATE = PIN_PRAWA_STATE_OPEN_CURRENT;
    PIN_PRAWA_STATE_OPEN_LAST_CHANGE = now;
  }

  if (PIN_PRAWA_STATE_CLOSED_CURRENT != PIN_PRAWA_STATE_CLOSED_LAST_STATE){
    PIN_PRAWA_STATE_CLOSED_LAST_STATE = PIN_PRAWA_STATE_CLOSED_CURRENT;
    PIN_PRAWA_STATE_CLOSED_LAST_CHANGE = now;
  }

  if (PIN_FOTOKOMORKA_CURRENT != PIN_FOTOKOMORKA_LAST_STATE){
    PIN_FOTOKOMORKA_LAST_STATE = PIN_FOTOKOMORKA_CURRENT;
    PIN_FOTOKOMORKA_LAST_CHANGE = now;
  }

  if (PIN_PILOT_CURRENT != PIN_PILOT_LAST_STATE){
    PIN_PILOT_LAST_STATE = PIN_PILOT_CURRENT;
    PIN_PILOT_LAST_CHANGE = now;
  }

  if((unsigned long)(now - PIN_LEWA_STATE_OPEN_LAST_CHANGE) > PIN_LEWA_STATE_OPEN_STABILISATION){
    INPUT_LEWA_STATE_OPEN = PIN_LEWA_STATE_OPEN_CURRENT;
    PIN_LEWA_STATE_OPEN_LAST_CHANGE = (unsigned long)(now - (PIN_LEWA_STATE_OPEN_STABILISATION + (unsigned long)(100ul)));
  }

  if((unsigned long)(now - PIN_LEWA_STATE_CLOSED_LAST_CHANGE) > PIN_LEWA_STATE_CLOSED_STABILISATION){
    INPUT_LEWA_STATE_CLOSED = PIN_LEWA_STATE_CLOSED_CURRENT;
    PIN_LEWA_STATE_CLOSED_LAST_CHANGE = (unsigned long)(now - (PIN_LEWA_STATE_CLOSED_STABILISATION + (unsigned long)(100ul)));
  }

  if((unsigned long)(now - PIN_PRAWA_STATE_OPEN_LAST_CHANGE) > PIN_PRAWA_STATE_OPEN_STABILISATION){
    INPUT_PRAWA_STATE_OPEN = PIN_PRAWA_STATE_OPEN_CURRENT;
    PIN_PRAWA_STATE_OPEN_LAST_CHANGE = (unsigned long)(now - (PIN_PRAWA_STATE_OPEN_STABILISATION + (unsigned long)(100ul)));
  }

  if((unsigned long)(now - PIN_PRAWA_STATE_CLOSED_LAST_CHANGE) > PIN_PRAWA_STATE_CLOSED_STABILISATION){
    INPUT_PRAWA_STATE_CLOSED = PIN_PRAWA_STATE_CLOSED_CURRENT;
    PIN_PRAWA_STATE_CLOSED_LAST_CHANGE = (unsigned long)(now - (PIN_PRAWA_STATE_CLOSED_STABILISATION + (unsigned long)(100ul)));
  }

  if((unsigned long)(now - PIN_FOTOKOMORKA_LAST_CHANGE) > PIN_FOTOKOMORKA_STABILISATION){
    INPUT_FOTOKOMORKA = PIN_FOTOKOMORKA_CURRENT;
    PIN_FOTOKOMORKA_LAST_CHANGE = (unsigned long)(now - (PIN_FOTOKOMORKA_STABILISATION + (unsigned long)(100ul)));
  }

  if((unsigned long)(now - PIN_PILOT_LAST_CHANGE) > PIN_PILOT_STABILISATION){
    INPUT_PILOT = PIN_PILOT_CURRENT;
    PIN_PILOT_LAST_CHANGE = (unsigned long)(now - (PIN_PILOT_STABILISATION + (unsigned long)(100ul)));
  }
}

void updateOutputs(){
  now = millis();

  if (PIN_LEWA_OPEN_UP_REQ){
    if((unsigned long)(PIN_LEWA_OPEN_UP_TIME - now) > (unsigned long)(999999ul)){
      PIN_LEWA_OPEN_UP_REQ = false;
      digitalWrite(PIN_LEWA_OPEN, HIGH);
      LAST_ACTION_TIME = now;
      STATUS_LAST_MOVE_ACTION = 1;
      STATUS_LEWA = 1;
      IGNORE_LEWA_CLOSED_SINCE = (unsigned long)(now);
      IGNORE_LEWA_CLOSED = true;
    }
  }

  if (PIN_LEWA_OPEN_DOWN_REQ){
    if((unsigned long)(PIN_LEWA_OPEN_DOWN_TIME - now) > (unsigned long)(999999ul)){
      PIN_LEWA_OPEN_DOWN_REQ = false;
      digitalWrite(PIN_LEWA_OPEN, LOW);
    }
  }

  if (PIN_LEWA_CLOSE_UP_REQ){
    if((unsigned long)(PIN_LEWA_CLOSE_UP_TIME - now) > (unsigned long)(999999ul)){
      PIN_LEWA_CLOSE_UP_REQ = false;
      digitalWrite(PIN_LEWA_CLOSE, HIGH);
      LAST_ACTION_TIME = now;
      STATUS_LAST_MOVE_ACTION = 0;
      STATUS_LEWA = -1;
      IGNORE_LEWA_OPEN_SINCE = (unsigned long)(now);
      IGNORE_LEWA_OPEN = true;
    }
  }

  if (PIN_LEWA_CLOSE_DOWN_REQ){
    if((unsigned long)(PIN_LEWA_CLOSE_DOWN_TIME - now) > (unsigned long)(999999ul)){
      PIN_LEWA_CLOSE_DOWN_REQ = false;
      digitalWrite(PIN_LEWA_CLOSE, LOW);
    }
  }

  if (PIN_LEWA_STOP_UP_REQ){
    if((unsigned long)(PIN_LEWA_STOP_UP_TIME - now) > (unsigned long)(999999ul)){
      PIN_LEWA_STOP_UP_REQ = false;
      digitalWrite(PIN_LEWA_STOP, LOW);
      LAST_ACTION_TIME = now;
      STATUS_LEWA = 0;
    }
  }

  if (PIN_LEWA_STOP_DOWN_REQ){
    if((unsigned long)(PIN_LEWA_STOP_DOWN_TIME - now) > (unsigned long)(999999ul)){
      PIN_LEWA_STOP_DOWN_REQ = false;
      digitalWrite(PIN_LEWA_STOP, HIGH);
    }
  }

  if (PIN_PRAWA_OPEN_UP_REQ){
    if((unsigned long)(PIN_PRAWA_OPEN_UP_TIME - now) > (unsigned long)(999999ul)){
      PIN_PRAWA_OPEN_UP_REQ = false;
      digitalWrite(PIN_PRAWA_OPEN, HIGH);
      LAST_ACTION_TIME = now;
      STATUS_LAST_MOVE_ACTION = 1;
      STATUS_PRAWA = 1;
      IGNORE_PRAWA_CLOSED_SINCE = (unsigned long)(now);
      IGNORE_PRAWA_CLOSED = true;
    }
  }

  if (PIN_PRAWA_OPEN_DOWN_REQ){
    if((unsigned long)(PIN_PRAWA_OPEN_DOWN_TIME - now) > (unsigned long)(999999ul)){
      PIN_PRAWA_OPEN_DOWN_REQ = false;
      digitalWrite(PIN_PRAWA_OPEN, LOW);
    }
  }

  if (PIN_PRAWA_CLOSE_UP_REQ){
    if((unsigned long)(PIN_PRAWA_CLOSE_UP_TIME - now) > (unsigned long)(999999ul)){
      PIN_PRAWA_CLOSE_UP_REQ = false;
      digitalWrite(PIN_PRAWA_CLOSE, HIGH);
      LAST_ACTION_TIME = now;
      STATUS_LAST_MOVE_ACTION = 0;
      STATUS_PRAWA = -1;
      IGNORE_PRAWA_OPEN_SINCE = (unsigned long)(now);
      IGNORE_PRAWA_OPEN = true;
    }
  }

  if (PIN_PRAWA_CLOSE_DOWN_REQ){
    if((unsigned long)(PIN_PRAWA_CLOSE_DOWN_TIME - now) > (unsigned long)(999999ul)){
      PIN_PRAWA_CLOSE_DOWN_REQ = false;
      digitalWrite(PIN_PRAWA_CLOSE, LOW);
    }
  }

  if (PIN_PRAWA_STOP_UP_REQ){
    if((unsigned long)(PIN_PRAWA_STOP_UP_TIME - now) > (unsigned long)(999999ul)){
      PIN_PRAWA_STOP_UP_REQ = false;
      digitalWrite(PIN_PRAWA_STOP, LOW);
      LAST_ACTION_TIME = now;
      STATUS_PRAWA = 0;
    }
  }

  if (PIN_PRAWA_STOP_DOWN_REQ){
    if((unsigned long)(PIN_PRAWA_STOP_DOWN_TIME - now) > (unsigned long)(999999ul)){
      PIN_PRAWA_STOP_DOWN_REQ = false;
      digitalWrite(PIN_PRAWA_STOP, HIGH);
    }
  }
  
}

void openPrawa(unsigned long extradelay = 0){
  now = millis();
  if (extradelay == (unsigned long)(0)){
  PIN_PRAWA_OPEN_UP_REQ = false;
  PIN_PRAWA_OPEN_DOWN_REQ = false;
  PIN_PRAWA_CLOSE_UP_REQ = false;
  PIN_PRAWA_CLOSE_DOWN_REQ = false;
  PIN_PRAWA_STOP_UP_REQ = false;
  PIN_PRAWA_STOP_DOWN_REQ = false;
  digitalWrite(PIN_PRAWA_OPEN, LOW);
  digitalWrite(PIN_PRAWA_CLOSE, LOW);
  digitalWrite(PIN_PRAWA_STOP, HIGH);
  }
  PIN_PRAWA_OPEN_UP_TIME = (unsigned long)(now + (unsigned long)(50) + extradelay);
  PIN_PRAWA_OPEN_DOWN_TIME = (unsigned long)(now + (unsigned long)(100) + extradelay);
  PIN_PRAWA_OPEN_UP_REQ = true;
  PIN_PRAWA_OPEN_DOWN_REQ = true;
}

void closePrawa(unsigned long extradelay = 0){
  now = millis();
  if (extradelay == (unsigned long)(0)){
  PIN_PRAWA_OPEN_UP_REQ = false;
  PIN_PRAWA_OPEN_DOWN_REQ = false;
  PIN_PRAWA_CLOSE_UP_REQ = false;
  PIN_PRAWA_CLOSE_DOWN_REQ = false;
  PIN_PRAWA_STOP_UP_REQ = false;
  PIN_PRAWA_STOP_DOWN_REQ = false;
  digitalWrite(PIN_PRAWA_OPEN, LOW);
  digitalWrite(PIN_PRAWA_CLOSE, LOW);
  digitalWrite(PIN_PRAWA_STOP, HIGH);
  }
  PIN_PRAWA_CLOSE_UP_TIME = (unsigned long)(now + (unsigned long)(50) + extradelay);
  PIN_PRAWA_CLOSE_DOWN_TIME = (unsigned long)(now + (unsigned long)(100) + extradelay);
  PIN_PRAWA_CLOSE_UP_REQ = true;
  PIN_PRAWA_CLOSE_DOWN_REQ = true;
}

void stopPrawa(unsigned long extradelay = 0){
  now = millis();
  if (extradelay == (unsigned long)(0)){
  PIN_PRAWA_OPEN_UP_REQ = false;
  PIN_PRAWA_OPEN_DOWN_REQ = false;
  PIN_PRAWA_CLOSE_UP_REQ = false;
  PIN_PRAWA_CLOSE_DOWN_REQ = false;
  PIN_PRAWA_STOP_UP_REQ = false;
  PIN_PRAWA_STOP_DOWN_REQ = false;
  digitalWrite(PIN_PRAWA_OPEN, LOW);
  digitalWrite(PIN_PRAWA_CLOSE, LOW);
  digitalWrite(PIN_PRAWA_STOP, HIGH);
  }
  PIN_PRAWA_STOP_UP_TIME = (unsigned long)(now + (unsigned long)(50) + extradelay);
  PIN_PRAWA_STOP_DOWN_TIME = (unsigned long)(now + (unsigned long)(100) + extradelay);
  PIN_PRAWA_STOP_UP_REQ = true;
  PIN_PRAWA_STOP_DOWN_REQ = true;
}

void openLewa(unsigned long extradelay = 0){
  now = millis();
  if (extradelay == (unsigned long)(0)){
  PIN_LEWA_OPEN_UP_REQ = false;
  PIN_LEWA_OPEN_DOWN_REQ = false;
  PIN_LEWA_CLOSE_UP_REQ = false;
  PIN_LEWA_CLOSE_DOWN_REQ = false;
  PIN_LEWA_STOP_UP_REQ = false;
  PIN_LEWA_STOP_DOWN_REQ = false;
  digitalWrite(PIN_LEWA_OPEN, LOW);
  digitalWrite(PIN_LEWA_CLOSE, LOW);
  digitalWrite(PIN_LEWA_STOP, HIGH);
  }
  PIN_LEWA_OPEN_UP_TIME = (unsigned long)(now + (unsigned long)(50) + extradelay);
  PIN_LEWA_OPEN_DOWN_TIME = (unsigned long)(now + (unsigned long)(100) + extradelay);
  PIN_LEWA_OPEN_UP_REQ = true;
  PIN_LEWA_OPEN_DOWN_REQ = true;
}

void closeLewa(unsigned long extradelay = 0){
  now = millis();
  if (extradelay == (unsigned long)(0)){
  PIN_LEWA_OPEN_UP_REQ = false;
  PIN_LEWA_OPEN_DOWN_REQ = false;
  PIN_LEWA_CLOSE_UP_REQ = false;
  PIN_LEWA_CLOSE_DOWN_REQ = false;
  PIN_LEWA_STOP_UP_REQ = false;
  PIN_LEWA_STOP_DOWN_REQ = false;
  digitalWrite(PIN_LEWA_OPEN, LOW);
  digitalWrite(PIN_LEWA_CLOSE, LOW);
  digitalWrite(PIN_LEWA_STOP, HIGH);
  }
  PIN_LEWA_CLOSE_UP_TIME = (unsigned long)(now + (unsigned long)(50) + extradelay);
  PIN_LEWA_CLOSE_DOWN_TIME = (unsigned long)(now + (unsigned long)(100) + extradelay);
  PIN_LEWA_CLOSE_UP_REQ = true;
  PIN_LEWA_CLOSE_DOWN_REQ = true;
}

void stopLewa(unsigned long extradelay = 0){
  now = millis();
  if (extradelay == (unsigned long)(0)){
  PIN_LEWA_OPEN_UP_REQ = false;
  PIN_LEWA_OPEN_DOWN_REQ = false;
  PIN_LEWA_CLOSE_UP_REQ = false;
  PIN_LEWA_CLOSE_DOWN_REQ = false;
  PIN_LEWA_STOP_UP_REQ = false;
  PIN_LEWA_STOP_DOWN_REQ = false;
  digitalWrite(PIN_LEWA_OPEN, LOW);
  digitalWrite(PIN_LEWA_CLOSE, LOW);
  digitalWrite(PIN_LEWA_STOP, HIGH);
  }
  PIN_LEWA_STOP_UP_TIME = (unsigned long)(now + (unsigned long)(50) + extradelay);
  PIN_LEWA_STOP_DOWN_TIME = (unsigned long)(now + (unsigned long)(100) + extradelay);
  PIN_LEWA_STOP_UP_REQ = true;
  PIN_LEWA_STOP_DOWN_REQ = true;
}

void updateState(){
  now = millis();
  if (STATUS_PRAWA == -1 && INPUT_FOTOKOMORKA == 1){
    STATUS_PRAWA = 0;
    stopPrawa();
    stopLewa();
    openPrawa(100);
    openLewa(100);
  }
  if (STATUS_LEWA == -1 && INPUT_FOTOKOMORKA == 1){
    STATUS_LEWA = 0;
    stopPrawa();
    stopLewa();
    openPrawa(100);
    openLewa(100);
  }

  if (INPUT_PRAWA_STATE_OPEN){
    if ( (IGNORE_PRAWA_OPEN == 0) || ( (unsigned long)(now - IGNORE_PRAWA_OPEN_SINCE) > (unsigned long)(700) ) ){
      STATUS_PRAWA = 2;
      IGNORE_PRAWA_OPEN = 0;
    }
  }

  if (INPUT_PRAWA_STATE_CLOSED){
    if ( (IGNORE_PRAWA_CLOSED == 0) || ( (unsigned long)(now - IGNORE_PRAWA_CLOSED_SINCE) > (unsigned long)(700) ) ){
      STATUS_PRAWA = -2;
      IGNORE_PRAWA_CLOSED = 0;
    }
  }

  if (INPUT_LEWA_STATE_OPEN){
    if ( (IGNORE_LEWA_OPEN == 0) || ( (unsigned long)(now - IGNORE_LEWA_OPEN_SINCE) > (unsigned long)(700) ) ){
      STATUS_LEWA = 2;
      IGNORE_LEWA_OPEN = 0;
    }
  }

  if (INPUT_LEWA_STATE_CLOSED){
    if ( (IGNORE_LEWA_CLOSED == 0) || ( (unsigned long)(now - IGNORE_LEWA_CLOSED_SINCE) > (unsigned long)(700) ) ){
      STATUS_LEWA = -2;
      IGNORE_LEWA_CLOSED = 0;
    }
  }
}

void handleRemote(bool force = false){
  now = millis();
  if ((!INPUT_PILOT) || force){
    if ((!PILOT_HANDLED) || force){
      if (!force){
      PILOT_HANDLED = 1;
      }
      if (STATUS_PRAWA == -1 || STATUS_LEWA == -1 || STATUS_PRAWA == 1 || STATUS_LEWA == 1){
        //STOP
        stopPrawa();
        stopLewa();
      }
      else{
        if (STATUS_PRAWA == 0 || STATUS_LEWA == 0){
          //oppose last
          if (!STATUS_LAST_MOVE_ACTION){
            //open
            stopPrawa();
            stopLewa();
            openPrawa(100);
            openLewa(100);
          }
          else{
            //close
            stopPrawa();
            stopLewa();
            closePrawa(100);
            closeLewa(100);
          }
        }
        else{
          if (STATUS_PRAWA == -2){
            //open
            stopPrawa();
            stopLewa();
            openPrawa(100);
            openLewa(100);
            
          }
          if (STATUS_PRAWA == 2){
            //close
            stopPrawa();
            stopLewa();
            closePrawa(100);
            closeLewa(100);
          }
        }
      }
      
    }
  }
  else{
    PILOT_HANDLED = 0;
  }
}

void handleAutoClose(){
  now = millis();

  if (INPUT_FOTOKOMORKA == 1){
    if ( (unsigned long)(now - LAST_ACTION_TIME) > (unsigned long)(AUTOCLOSE_DELAY - AUTOCLOSE_FOTO_DELAY) ){
      LAST_ACTION_TIME = (unsigned long)(now - (AUTOCLOSE_DELAY - AUTOCLOSE_FOTO_DELAY));
    }
  }
  
  if ( (!INPUT_PRAWA_STATE_CLOSED) || (!INPUT_LEWA_STATE_CLOSED) ){
    if ((unsigned long)(now - LAST_ACTION_TIME) > AUTOCLOSE_DELAY){
      stopPrawa();
      stopLewa();
      closePrawa(100);
      closeLewa(100);
      LAST_ACTION_TIME = now;
    }
  }

  if ((unsigned long)(now - LAST_ACTION_TIME) > (unsigned long)(AUTOCLOSE_DELAY + (unsigned long)(1000))){
    LAST_ACTION_TIME = (unsigned long)(now - (AUTOCLOSE_DELAY + (unsigned long)(1000)));
  }
  
}

void updatePosition(){
  if (STATUS_PRAWA == -1 && suspect_position_prawa > 1){
    suspect_position_prawa -= 1;
  }
  if (STATUS_PRAWA == 1 && suspect_position_prawa < (max_position_prawa - 1)){
    suspect_position_prawa += 1;
  }
  if (STATUS_LEWA == -1 && suspect_position_lewa > 1){
    suspect_position_lewa -= 1;
  }
  if (STATUS_LEWA == 1 && suspect_position_lewa < (max_position_lewa - 1)){
    suspect_position_lewa += 1;
  }
  if (STATUS_PRAWA == -2){
    suspect_position_prawa = 0;
  }
  if (STATUS_PRAWA == 2){
    suspect_position_prawa = max_position_prawa;
  }
  if (STATUS_LEWA == -2){
    suspect_position_lewa = 0;
  }
  if (STATUS_LEWA == 2){
    suspect_position_lewa = max_position_lewa;
  }
}

void sendPosition(){
  int procent_global = 0;
  int procent_prawa = 0;
  int procent_lewa = 0;
  procent_global = ((suspect_position_prawa + suspect_position_lewa) * 100)/(max_position_prawa + max_position_lewa);
  if ( (suspect_position_prawa != 0 || suspect_position_lewa != 0) && procent_global == 0 ){
    procent_global = 1;
  }
  if ( (suspect_position_prawa != max_position_prawa || suspect_position_lewa != max_position_lewa) && procent_global == 100 ){
    procent_global = 99;
  }

  procent_prawa = (suspect_position_prawa * 100)/(max_position_prawa);
  if ( suspect_position_prawa != 0 && procent_prawa == 0){
    procent_prawa = 1;
  }
  if ( suspect_position_prawa != max_position_prawa && procent_prawa == 100){
    procent_prawa = 99;
  }

  procent_lewa = (suspect_position_lewa * 100)/(max_position_lewa);
  if ( suspect_position_lewa != 0 && procent_lewa == 0){
    procent_lewa = 1;
  }
  if ( suspect_position_lewa != max_position_lewa && procent_lewa == 100){
    procent_lewa = 99;
  }

  if (procent_global != last_reported_procent){
    if (client.publish(PREFIX"/brama/"NUMBER"/get/Position", String(procent_global).c_str(), true)){
      last_reported_procent = procent_global;
    }
  }
  if (procent_lewa != last_reported_procent_lewa){
    if (client.publish(PREFIX"/brama/"NUMBER"/get/lewa/Position", String(procent_lewa).c_str(), true)){
      last_reported_procent_lewa = procent_lewa;
    }
  }
  if (procent_prawa != last_reported_procent_prawa){
    if (client.publish(PREFIX"/brama/"NUMBER"/get/prawa/Position", String(procent_prawa).c_str(), true)){
      last_reported_procent_prawa = procent_prawa;
    }
  }
}

void reconnect(){
  client.disconnect();
  espClient.stop();

  static const IPAddress anyaddr(0,0,0,0);
  static IPAddress dnsb(192,168,9,253);
  static ip_addr_t ader;
  ader.type = IPADDR_TYPE_V4;
  ader.u_addr.ip4.addr = static_cast<uint32_t>(dnsb);

  IPAddress current = WiFi.dnsIP(0);
  
  if (current == anyaddr){
    dns_setserver(0, &ader); 
  }
  else{
    dnsb = current;
  }
  

  //IPAddress odpo_dns;
  //int kod_dns = 0;
  //kod_dns = WiFi.hostByName(mqtt_server, odpo_dns);
  //if (kod_dns == 1){
  //  Serial.println("Zapytanie DNS udalo sie");
  //  mqtt_ip = odpo_dns;
  //}
  //else{
  //  Serial.println("DNS nie zadzialal");
  //}
  //client.setServer(mqtt_ip, 8883);
  
  if (client.connect(NAME , mqtt_login , mqtt_password)){
    mqtt_connfail_times = 0;
    client.subscribe(PREFIX"/brama/"NUMBER"/set");
    client.subscribe(PREFIX"/brama/"NUMBER"/prawa/set");
    client.subscribe(PREFIX"/brama/"NUMBER"/lewa/set");
  }
  else{
    mqtt_connfail_times = mqtt_connfail_times + 1;
  }
  
  if (mqtt_connfail_times > 11){
    mqtt_connfail_times = 0;
    client.disconnect();
    espClient.stop();
    WiFi.setAutoReconnect(false);
    WiFi.setAutoConnect(false);
    WiFi.disconnect(false, true);
    WiFi.mode(WIFI_OFF);

    WiFi.mode(WIFI_STA);
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(false);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, {0,0,0,0}, {0,0,0,0});
    WiFi.setHostname(NAME);
    WiFi.config({0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0});
    WiFi.setHostname(NAME);
  }
}

void scan_wifi_init(){
  WiFi.scanNetworks(true, false);
}

void scan_wifi_process(bool force = false){
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
          espClient.stop();
          WiFi.setAutoReconnect(false);
          WiFi.setAutoConnect(false);
          WiFi.disconnect(false, true);
          WiFi.mode(WIFI_OFF);
          
          WiFi.mode(WIFI_STA);
          WiFi.setAutoReconnect(false);
          WiFi.setAutoConnect(false);
          WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, {0,0,0,0}, {0,0,0,0});
          WiFi.setHostname(NAME);
          WiFi.config({0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0});
          WiFi.setHostname(NAME);
          WiFi.begin(ssid.c_str(), password, max_rssi_channel, max_rssi_bssid, true);
          WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, {0,0,0,0}, {0,0,0,0});
          WiFi.setHostname(NAME);
          WiFi.config({0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0});
          WiFi.setHostname(NAME);
        }
      }
    }
    else{
      if (!ustanowione){
        client.disconnect();
        espClient.stop();
        WiFi.setAutoReconnect(false);
        WiFi.setAutoConnect(false);
        WiFi.disconnect(false, true);
        WiFi.mode(WIFI_OFF);

        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(false);
        WiFi.setAutoConnect(false);
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, {0,0,0,0}, {0,0,0,0});
        WiFi.setHostname(NAME);
        WiFi.config({0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0});
        WiFi.setHostname(NAME);
        WiFi.begin(ssid.c_str(), password);
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, {0,0,0,0}, {0,0,0,0});
        WiFi.setHostname(NAME);
        WiFi.config({0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0});
        WiFi.setHostname(NAME);
      }
    }
    
  }
  else{
    if (!ustanowione){
      client.disconnect();
      espClient.stop();
      WiFi.setAutoReconnect(false);
      WiFi.setAutoConnect(false);
      WiFi.disconnect(false, true);
      WiFi.mode(WIFI_OFF);

      WiFi.mode(WIFI_STA);
      WiFi.setAutoReconnect(false);
      WiFi.setAutoConnect(false);
      WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, {0,0,0,0}, {0,0,0,0});
      WiFi.setHostname(NAME);
      WiFi.config({0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0});
      WiFi.setHostname(NAME);
      WiFi.begin(ssid.c_str(), password);
      WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, {0,0,0,0}, {0,0,0,0});
      WiFi.setHostname(NAME);
      WiFi.config({0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0});
      WiFi.setHostname(NAME);
    }
  }
  
  WiFi.scanDelete();
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
      handleRemote(true);
    }
    if (strPayload == "OPEN"){
        stopPrawa();
        stopLewa();
        openPrawa(100);
        openLewa(100);
    }
    if (strPayload == "CLOSE"){
        stopPrawa();
        stopLewa();
        closePrawa(100);
        closeLewa(100);
    }
    if (strPayload == "STOP"){
        stopPrawa();
        stopLewa();
    }
  }

  if (strTopic == PREFIX"/brama/"NUMBER"/prawa/set"){
    if (strPayload == "OPEN"){
        stopPrawa();
        openPrawa(100);
    }
    if (strPayload == "CLOSE"){
        stopPrawa();
        closePrawa(100);
    }
    if (strPayload == "STOP"){
        stopPrawa();
    }
  }

  if (strTopic == PREFIX"/brama/"NUMBER"/lewa/set"){
    if (strPayload == "OPEN"){
        stopLewa();
        openLewa(100);
    }
    if (strPayload == "CLOSE"){
        stopLewa();
        closeLewa(100);
    }
    if (strPayload == "STOP"){
        stopLewa();
    }
  }
}


void setup() {
  digitalWrite(PIN_LEWA_OPEN, LOW);
  digitalWrite(PIN_LEWA_CLOSE, LOW);
  digitalWrite(PIN_PRAWA_OPEN, LOW);
  digitalWrite(PIN_PRAWA_CLOSE, LOW);
  digitalWrite(PIN_LEWA_STOP, HIGH);
  digitalWrite(PIN_PRAWA_STOP, HIGH);
  
  pinMode(PIN_LEWA_OPEN, OUTPUT);
  pinMode(PIN_LEWA_CLOSE, OUTPUT);
  pinMode(PIN_LEWA_STOP, OUTPUT);
  pinMode(PIN_PRAWA_OPEN, OUTPUT);
  pinMode(PIN_PRAWA_CLOSE, OUTPUT);
  pinMode(PIN_PRAWA_STOP, OUTPUT);

  digitalWrite(PIN_LEWA_OPEN, LOW);
  digitalWrite(PIN_LEWA_CLOSE, LOW);
  digitalWrite(PIN_PRAWA_OPEN, LOW);
  digitalWrite(PIN_PRAWA_CLOSE, LOW);
  digitalWrite(PIN_LEWA_STOP, HIGH);
  digitalWrite(PIN_PRAWA_STOP, HIGH);

  pinMode(PIN_LEWA_STATE_OPEN, INPUT_PULLUP);
  pinMode(PIN_LEWA_STATE_CLOSED, INPUT_PULLUP);
  pinMode(PIN_PRAWA_STATE_OPEN, INPUT_PULLUP);
  pinMode(PIN_PRAWA_STATE_CLOSED, INPUT_PULLUP);
  pinMode(PIN_FOTOKOMORKA, INPUT_PULLUP);
  pinMode(PIN_PILOT, INPUT_PULLUP);
  
  Serial.begin(115200);
  Serial.println("Booting");

  WiFi.persistent(false);

  btStop();
  WiFi.mode(WIFI_OFF);
  WiFi.persistent(false);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  WiFi.mode(WIFI_OFF);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);

  WiFi.setAutoReconnect(false);
  WiFi.setAutoConnect(false);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, {0,0,0,0}, {0,0,0,0});
  WiFi.setHostname(NAME);
  WiFi.config({0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0});
  WiFi.setHostname(NAME);
  //WiFi.begin(ssid.c_str(), password);
  //WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, {0,0,0,0}, {0,0,0,0});
  //WiFi.setHostname(NAME);
  //WiFi.config({0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0});
  //WiFi.setHostname(NAME);

  espClient.setCACert(caCert);

  client.setServer(mqtt_server, 8883);
  //client.setServer(mqtt_ip, 8883);
  client.setCallback(callback);
  client.setSocketTimeout(1);

  /*
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("brama");

  // No authentication by default
  ArduinoOTA.setPassword("test");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
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
    Serial.println("Proboje skanowac wifi (regular)");
    if (WiFi.scanComplete() == -2){
      if ((unsigned long)(now - previous_wifi_establish) > wifi_afterconn_delay){
        Serial.println("Skanuje wifi (regular)");
        scan_wifi_init(); 
      }
    }
  }

  if ((unsigned long)(now - previous_wifi_process) > wifi_process_interval){
    previous_wifi_process = now;
    Serial.println("Przetwarzanie sieci");
    if (WiFi.scanComplete() >= 0){
      scan_wifi_process();
      now = millis();
      previous_wifi_establish = now;
      Serial.println("Sieci przetworzone");
    }
  }
  
  if ((unsigned long)(now - previous_wifi_conncheck) > wifi_conncheck_interval){
    previous_wifi_conncheck = now;
    Serial.println("Sprawdzanie wlan");
    if ( !WiFi.isConnected() || (WiFi.status() != WL_CONNECTED)){
      Serial.println("Nie jestem polaczony do wifi");
      if (WiFi.scanComplete() == -2){
        Serial.println("Skan nie w toku");
        if ((unsigned long)(now - previous_wifi_establish) > wifi_afterconn_delay){
          Serial.println("Inicjalizuje skan");
          scan_wifi_init(); 
        }
      }
    }
  }

  if ((unsigned long)(now - previous_mqtt_conncheck) > mqtt_conncheck_interval){
    previous_mqtt_conncheck = now;
    Serial.println("Sprawdzam mqtt");

    Serial.print("DNS #1, #2 IP: ");
    WiFi.dnsIP().printTo(Serial);
    Serial.print(", ");
    WiFi.dnsIP(1).printTo(Serial);
    Serial.println();
    
    if( !client.connected() ){
      Serial.println("Lacze ponownie mqtt");
      if ( WiFi.isConnected() && (WiFi.status() == WL_CONNECTED)){
        if(WiFi.scanComplete() == -2){
          if ((unsigned long)(now - previous_wifi_establish) > wifi_afterconn_delay){
            Serial.println("RECONNECT");
            reconnect();
          }
        }
      }
    }
  }
  
  now = millis();
  
  if ((unsigned long)(now - previous_input_update) > input_update_interval){
    previous_input_update = now;
    updateInputs();
  }

  if ((unsigned long)(now - previous_state_update) > state_update_interval){
    previous_state_update = now;
    updateState();
  }
  now = millis();
  if ((unsigned long)(now - previous_update_position) >= (unsigned long)(100)){
    updatePosition();
    if ((unsigned long)(now - previous_update_position) > (unsigned long)(1000)){
      previous_update_position = now;
    }
    else{
      previous_update_position += (unsigned long)(100);
    }
  }

  if ((unsigned long)(now - previous_autoclose_handle) > autoclose_handle_interval){
    previous_autoclose_handle = now;
    handleAutoClose();
  }

  if ((unsigned long)(now - previous_remote_handle) > remote_handle_interval){
    previous_remote_handle = now;
    handleRemote();
  }

  if ((unsigned long)(now - previous_output_update) > output_update_interval){
    previous_output_update = now;
    updateOutputs();
  }

  if ((unsigned long)(now - previous_send_position) > send_position_interval){
    previous_send_position = now;
    sendPosition();
  }

  delay(5);
}
