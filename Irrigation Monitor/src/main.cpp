#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb
// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to the module)
// Use Hardware Serial on Mega, Leonardo, Micro
#define SerialAT Serial1

// or Software Serial on Uno, Nano
//#include <SoftwareSerial.h>
//SoftwareSerial SerialAT(2, 3); // RX, TX

#define TINY_GSM_DEBUG SerialMon
// See all AT commands, if wanted
 #define DUMP_AT_COMMANDS

/*
   Tests enabled
*/
// Range to attempt to autobaud
#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 115200
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false


#define TINY_GSM_TEST_GPRS    true
#define TINY_GSM_TEST_GPS     true
#define TINY_GSM_POWERDOWN    true

// set GSM PIN, if any
#define GSM_PIN ""

#include "credentials.h" //Includes the following commented variables

/*
const char apn[]  = "";     //SET TO YOUR APN
const char gprsUser[] = "";
const char gprsPass[] = "";
const char deviceID[] = "";

// Server details
const char server[] = ""; //set the rest of the api call below
const int  port = 8080;
*/

#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

#include <SPI.h>
#include <SD.h>
#include <Ticker.h>

#ifdef DUMP_AT_COMMANDS  // if enabled it requires the streamDebugger lib
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif
TinyGsmClient client(modem);
HttpClient http(client, server, port);
#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds
int TIME_TO_SLEEP = 300;          // Time ESP32 will go to sleep (in seconds)

#define UART_BAUD   9600
#define PIN_DTR     25
#define PIN_TX      27
#define PIN_RX      26
#define PWR_PIN     4

#define SD_MISO     2
#define SD_MOSI     15
#define SD_SCLK     14
#define SD_CS       13
#define LED_PIN     12


int counter, lastIndex, numberOfPieces = 24;
String pieces[24], input;

esp_sleep_wakeup_cause_t wakeup_reason;

void modem_sleep() {
  Serial.println("Putting modem to sleep.");
  modem.sleepEnable();
  pinMode(PIN_DTR, OUTPUT);
  digitalWrite(PIN_DTR, HIGH);
}

void modem_wake() {
  Serial.println("Waking modem up.");
  modem.sleepEnable(false);
  pinMode(PIN_DTR, OUTPUT);
  digitalWrite(PIN_DTR, LOW);
  delay(50);
}

void modem_initialize() {
  // Set console baud rate
  Serial.begin(115200);
  delay(10);

  // Set LED OFF
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH);
  delay(300);
  digitalWrite(PWR_PIN, LOW);

  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS)) {
    Serial.println("SDCard MOUNT FAIL");
  } else {
    uint32_t cardSize = SD.cardSize() / (1024 * 1024);
    String str = "SDCard Size: " + String(cardSize) + "MB";
    Serial.println(str);
  }

  Serial.println("\nWait...");

  delay(10000);

  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  // Set GSM module baud rate
  // TinyGsmAutoBaud(SerialAT, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
  // SerialAT.begin(9600);
  modem_wake();
  delay(6000);
  modem.simUnlock(GSM_PIN);
  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  Serial.println("Initializing modem...");
  if (!modem.init()) {
    Serial.println("Failed to restart modem, attempting to continue without restarting");
  }

  String name = modem.getModemName();
  delay(500);
  Serial.println("Modem Name: " + name);

  String modemInfo = modem.getModemInfo();
  delay(500);
  Serial.println("Modem Info: " + modemInfo);

  // Set SIM7000G GPIO4 LOW ,turn off GPS power
  // CMD:AT+SGPIO=0,4,1,0
  // Only in version 20200415 is there a function to control GPS power
  modem.sendAT("+SGPIO=0,4,1,0");


  #if TINY_GSM_USE_GPRS
    // Unlock your SIM card with a PIN if needed
    if ( GSM_PIN && modem.getSimStatus() != 3 ) {
      modem.simUnlock(GSM_PIN);
    }
  #endif
  /*
    2 Automatic
    13 GSM only
    38 LTE only
    51 GSM and LTE only
  * * * */
  String res;
  do {
    res = modem.setNetworkMode(38);
    delay(500);
  } while (res != "1");

  /*
    1 CAT-M
    2 NB-Iot
    3 CAT-M and NB-IoT
  * * */
  do {
    res = modem.setPreferredMode(1);
    delay(500);
  } while (res != "1");
}

void apiCall(){
  #if TINY_GSM_USE_GPRS && defined TINY_GSM_MODEM_XBEE
    // The XBee must run the gprsConnect function BEFORE waiting for network!
    modem.gprsConnect(apn, gprsUser, gprsPass);
  #endif
    
   //modem_wake();

    SerialMon.print("Waiting for network...");
    if (!modem.waitForNetwork()) {
      SerialMon.println(" fail");
      delay(10000);
      return;
    }
    SerialMon.println(" success");

    if (modem.isNetworkConnected()) {
      SerialMon.println("Network connected");
    }


      SerialMon.print(F("Connecting to "));
      SerialMon.print(apn);
      if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(" fail");
        //delay(10000);
        return;
      }
      SerialMon.println(" success");

      if (modem.isGprsConnected()) {
        SerialMon.println("GPRS connected");
      }

      modem.sendAT("+SGPIO=0,4,1,1");
      
      delay(1000);
      float lat,  lon, speed, alt;
      
      
      modem.enableGPS();
      while (1) {
        if (modem.getGPS(&lat, &lon, &speed, &alt)) {
          Serial.printf("lat:%f lon:%f\n", lat, lon);
          break;
        } else {
          Serial.print("getGPS ");
          Serial.println(millis());
        }
        delay(2000);
      }
      modem.disableGPS();
      
      float batPercent = modem.getBattVoltage() / 1000.0;
      String dateTime = modem.getGSMDateTime(DATE_FULL);
      
      // Set SIM7000G GPIO4 LOW ,turn off GPS power
      // CMD:AT+SGPIO=0,4,1,0
      // Only in version 20200415 is there a function to control GPS power
      modem.sendAT("+SGPIO=0,4,1,0");
      Serial.println("\n---End of GPRS TEST---\n");

    SerialMon.print(F("Performing HTTP GET request... "));
    String message = "/vars/?";
    message += "id=";
    message += deviceID;
    message += "&lat=";
    message += String(lat,6);
    message += "&lon=";
    message += String(lon,6);
    message += "&bat=";
    message += String(batPercent,3);
    message += "&dateTime=";
    message += dateTime;
    message += "&speed=";
    message += String(speed, 3);
    message += "&alt=";
    message += String(alt, 1);

    int err = http.get(message);
    if (err != 0) {
      SerialMon.println(F("failed to connect"));
      modem_sleep();
      return;
    }
    
    int status = http.responseStatusCode();
    SerialMon.print(F("Response status code: "));
    SerialMon.println(status);
    if (!status) {
      modem_sleep();
      return;
    }

    SerialMon.println(F("Response Headers:"));
    while (http.headerAvailable()) {
      String headerName = http.readHeaderName();
      String headerValue = http.readHeaderValue();
      SerialMon.println("    " + headerName + " : " + headerValue);
    }

    int length = http.contentLength();
    if (length >= 0) {
      SerialMon.print(F("Content length is: "));
      SerialMon.println(length);
    }
    if (http.isResponseChunked()) {
      SerialMon.println(F("The response is chunked"));
    }

    String body = http.responseBody();
    SerialMon.println(F("Response:"));
    SerialMon.println(body);

    if (atoi(body.c_str()) != 0) { //If the body is actually a number
      TIME_TO_SLEEP = atoi(body.c_str());
    } 

    Serial.println(TIME_TO_SLEEP);
    SerialMon.print(F("Body length is: "));
    SerialMon.println(body.length());

    // Shutdown

    http.stop();
    SerialMon.println(F("Server disconnected"));

    modem_sleep();

  // #if TINY_GSM_USE_WIFI
  //     modem.networkDisconnect();
  //     SerialMon.println(F("WiFi disconnected"));
  // #endif
  // #if TINY_GSM_USE_GPRS
  //     modem.gprsDisconnect();
  //     SerialMon.println(F("GPRS disconnected"));
  // #endif
}

//Function that prints the reason by which ESP32 has been awaken from sleep
void print_wakeup_reason(){
	esp_sleep_wakeup_cause_t wakeup_reason2;
	wakeup_reason2 = esp_sleep_get_wakeup_cause();
	switch(wakeup_reason2)
	{
		case 1  : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
		case 2  : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
		case 3  : Serial.println("Wakeup caused by timer"); break;
		case 4  : Serial.println("Wakeup caused by touchpad"); break;
		case 5  : Serial.println("Wakeup caused by ULP program"); break;
		default : Serial.println("Wakeup was not caused by deep sleep"); break;
	}
}

void setup() {
  
  Serial.begin(115200);
  wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) { //If wakeup caused by timer
    SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
    modem_wake();
    apiCall();
  }
  else {
    modem_initialize();
    apiCall();
  }
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);  // Configures ESP32 to sleep for TIME_TO_SLEEP number of seconds
  delay(100);
  esp_deep_sleep_start(); // Begins configured sleep
}






void loop() {
  //apiCall();
  
  //esp_deep_sleep_start();
}