#include <Arduino.h>

// ESP8266WiFi Built-In by Ivan Grokhotkov Version 1.0.0
#include <ESP8266WiFi.h> 

// Extra library: PubSubClient by Nick O'Leary <nick.oleary@gmail.com>  V2.8.0
#include <PubSubClient.h>

// DISPLAY
//
// Adafruit SSD1306 by Adafruit V2.5.7
// Adafruit "GFX Library" by Adafruit Version 1.11.8 INSTALLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Web and WiFi
#include <WiFiClient.h>
#include <base64.h>

// Inverter Data handling 
#include "Inverter.h"

// Include Udp Channel inverter communication
#include "InverterUdp.h"

///////////////////////////////////////////////////////////////////////
// Configuration and user settings
#include "arduino_secrets.h"

// TimeSynchronization and handling
#include <NTPClient.h>
#include <TimeLib.h>

// Settings and Webserver
#include <ESP8266WebServer.h>
#include "ESP8266SettingsManager.h"


///////////////////////////////////////////////////////////////////////
// HARDWARE SPECIFIC ADAPTIONS 

// DISPLAY ------------------------------------------------------------
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

#define DURATION_SLEEP_IN_HOME_NETWORK 200  // Seconds


///////////////////////////////////////////////////////////////////////
// User Secrets imported
String WIFI_HOME_SSID = SECRET_WIFI_HOME_SSID;
String WIFI_HOME_KEY = SECRET_WIFI_HOME_KEY;

String WIFI_INVERTER_SSID = SECRET_WIFI_INVERTER_SSID;
String WIFI_INVERTER_KEY = SECRET_WIFI_INVERTER_KEY;

String WIFI_RELAIS_SSID = SECRET_WIFI_RELAIS_SSID;
String WIFI_RELAIS_KEY = SECRET_WIFI_RELAIS_KEY;

String MQTT_BROKER_HOST = SECRET_MQTT_BROKER_HOST;
int    MQTT_BROKER_PORT = SECRET_MQTT_BROKER_PORT;
String MQTT_BROKER_USER = SECRET_MQTT_BROKER_USER;
String MQTT_BROKER_PWD = SECRET_MQTT_BROKER_PWD;
String MQTT_BROKER_MAINTOPIC = SECRET_MQTT_BROKER_MAINTOPIC;

String INVERTER_WEBACCESS_USER = SECRET_INVERTER_WEBACCESS_USER;
String INVERTER_WEBACCESS_PWD = SECRET_INVERTER_WEBACCESS_PWD;

///////////////////////////////////////////////////////////////////////
// Other rather static parameters
String status_page_url = "status.html" ;

unsigned long startTime =  0;


///////////////////////////////////////////////////////////////////////
// Global variables
bool connected = false;


////////////////////////////////////////////////////////////////////
// Intializations 

// Initialize the display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Initialize the MQTT client
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

Inverter inverter;
InverterUdp inverterUdp;

const int udpServerPort = 50000; // manual port
const int udpLocalPort = 48899; // Fixed port of deye inverter
const int udpTimeoput_s = 10; // 10 Seconds Timeout 
    
// Time Synchronization
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
unsigned long updateInterval = 20000; // 1 minute
long lastUpdate;
const int maxNtpRetries = 5;
unsigned long epochTime = 0;
bool ntpTimeAvailable = false;

// Settings and Websever
ESP8266WebServer server(80);
ESP8266SettingsManager settingsManager(server);



////////////////////////////////////////////////////////////////////
// Function declarations
void mqtt_submit_data();
void mqtt_reconnect();

void wifi_connect(String ssid, String passkey, String comment);

void web_getDataFromWeb(String url, String web_user, String web_password);

void displayInverterStatus(const Inverter& inverter);


////////////////////////////////////////////////////////////////////
// SETUP Function

void setup() {
  Serial.begin(115200);
  delay(10);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  Serial.println("Initialize inverter");
  //inverter.printVariables();

  // Prepare MQTT client
  mqtt_client.setServer(MQTT_BROKER_HOST.c_str(),MQTT_BROKER_PORT);

  delay (1000);

  Serial.print("Init Time Client "); 
  timeClient.begin();
  timeClient.setTimeOffset(7200);

  wifi_connect(WIFI_HOME_SSID, WIFI_HOME_KEY, "Home Network");
    Serial.print("Connecting to Home Network first to get the time from NTP "); 
    if (connected) {
        delay(2000);
        ntpTimeAvailable = syncTime();
        displayTime();
        delay(5000);
    }

  // Debugging Erase EEPROM
  //settingsManager.eraseAndInitializeEEPROM();

}


////////////////////////////////////////////////////////////////////
// MAIN LOOP

void loop() {   
    // INVERTER NETWORK 
    wifi_connect(WIFI_INVERTER_SSID, WIFI_INVERTER_KEY, "Inverter Network");
    // If connected with the Solar Inverter
    if (connected) {
      // ToDo-Option: Replace Web scraping by Modbus Read commands
      web_getDataFromWeb(status_page_url, INVERTER_WEBACCESS_USER, INVERTER_WEBACCESS_PWD);
      
      // Starting UDP Connection inside the AP Network of inverter
      bool startCon =  inverterUdp.inverter_connect(WiFi.gatewayIP().toString(),udpServerPort, udpLocalPort, udpTimeoput_s);
      
      // Getting time from inverter, stored in inverterUDP object
      String response = inverterUdp.inverter_readtime();
      
      if (ntpTimeAvailable){
        Serial.println("Triggering time update");
        inverterUdp.inverter_settime(epochTime);
      } else {
        Serial.println("No NTP time available. Skipping sync");
      }

      // Close connection before leaving to Home Network
      inverterUdp.inverter_close();
    }

    // Output Information 
    Serial.println("Print WebInverter");
    inverter.printVariables();
    displayInverterStatus(inverter);

    // Switching to Home Network
    wifi_connect(WIFI_HOME_SSID, WIFI_HOME_KEY, "Home Network");
    if (connected) {
        // Start Websever and SettingsManager  
        settingsManager.begin();
        
        mqtt_submit_data();
        delay(3000); // Show data for 3 Seconds
        ntpTimeAvailable = syncTime();
        displayTime();
        int secondsInHomeNetwork =0;
        while (secondsInHomeNetwork < DURATION_SLEEP_IN_HOME_NETWORK ){
          // Toggle between Time and Solar Production Screen
          displayInverterStatus(inverter);
          delay(5000);
          secondsInHomeNetwork+=5;
          
          displayTime();
          delay(5000);
          secondsInHomeNetwork+=5;
        }
        settingsManager.stop();
    }

}


////////////////////////////////////////////////////////////////////
// Sync Time
bool syncTime (){
    bool synced = false;
    Serial.println("Syncing time...");
        int retries = 0;
        while (retries < maxNtpRetries) {
            timeClient.update();
            epochTime = timeClient.getEpochTime();
            if (epochTime > 0) {  // If time is valid
                synced = true;
                lastUpdate = millis();
                break; // Exit the while loop if time was retrieved and processed successfully
            } else {
                retries++;
                Serial.println("Failed to get time, retrying...");
                delay(1000); // Delay for a second before retrying
            }
      }
    return synced;

}


////////////////////////////////////////////////////////////////////
// WiFi Functions
void wifi_connect(String ssid, String passkey, String comment){
  connected = false;
  WiFi.disconnect();
  delay(1000);
  // Connect to Wi-Fi
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println(F("++ Switching WiFi ++"));
  display.print("SSID: ");
  display.println(ssid);
  display.display();
  
  
  Serial.println("----------------------------------------------------");
  Serial.print("Connecting to ");
  Serial.print(ssid);
  WiFi.begin(ssid, passkey);

 
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { // 10 sec timeout
    display.print(F("."));
    display.display();
    delay(500);
    Serial.print(".");
    attempts++;
  }

  //display.clearDisplay();
  //display.setCursor(0,0);
  if(WiFi.status() == WL_CONNECTED) {
    connected = true;
    display.println(F(" OK"));
    display.print(F("IP: "));
    display.println(WiFi.localIP());
    Serial.println("--> connected");
    display.display();
    delay(3000);

  } else {
    connected = false;
    display.println(F("Failed to connect"));
    display.println(F("Check credential"));
    display.println(F("or availability"));
    display.display();
    display_invert_blink(5,2000);
  }
}


////////////////////////////////////////////////////////////////////
// Web Parsing Section
void web_getDataFromWeb(String url, String web_user, String web_password){
  
  String serverIp = WiFi.gatewayIP().toString();
  String website = "http://" + serverIp + "/" + url;

  Serial.print("url: ");
  Serial.println(website);


  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println(F("++ Collecting Data ++"));
  display.println(serverIp);
  display.println( "/" + url);
  display.display();


  // Create an instance of WiFiClient
  WiFiClient client;

  // Connect to the server
  if (client.connect(serverIp.c_str(), 80)) {
    // Prepare the HTTP request headers including the Authorization header
    
    Serial.println("Client connected: trying Basic Auth");
    String authHeaderValue = "Basic " + base64::encode(web_user + ":" + web_password);

    client.println("GET /" + url + " HTTP/1.1");
    client.println("Host: " + serverIp);
    client.println("Authorization: " + authHeaderValue);
    client.println("Connection: close");
    client.println();

    // Wait for the server's response
    String response = "";
    String line = "";
    while (client.connected()) {
      while (client.available()) {
        char c = client.read();
            if (c == '\n') {
                // Check if line starts with "var " and append it to the response string
                if (line.startsWith("var ")) {
                    response += line + "\n";
                }
                // Clear the line string for the next line
                line = "";
            } else if (c != '\r') {
                line += c;
            }
      }
    }

    // Print the entire response
    //Serial.print(response);
    inverter.updateData(response);

    client.stop();

    Serial.printf("\nPower now: %f\n", inverter.getInverterPowerNow_W());
    Serial.printf("Energy today: %f\n", inverter.getInverterEnergyToday_kWh());
    Serial.printf("Energy total: %f\n", inverter.getInverterEnergyTotal_kWh());

    display.println(F("> Parsing > OK"));
    display.display();
    delay(2000);

  } else {
    Serial.println("Failed to connect to server.");
    display.println(F("> Parsing > FAILED!"));
    display.display();
    display_invert_blink(5,2000);
  }

}


////////////////////////////////////////////////////////////////////
// DISPLAY Section
void displayInverterStatus(const Inverter& inverter) {
  int col1 = 70;
  int col2 = 100;


  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Connect to Wi-Fi
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("++ Inverter Status ++");

  // Display Power

  display.print("Power ");
  display.setCursor(col1,8);
  display.print(inverter.getInverterPowerNow_W(), 0);
  display.setCursor(col2,8);
  display.println(" W");

  // Display Energy Today
  display.print("E (today)");
  display.setCursor(col1,16);
  display.print(inverter.getInverterEnergyToday_kWh(), 1);
  display.setCursor(col2,16);
  display.println(" kWh");

  // Display Total Energy
  display.print("E (total)");
  display.setCursor(col1,24);
  display.print(inverter.getInverterEnergyTotal_kWh(), 0);
  display.setCursor(col2,24);
  display.println(" kWh");

  display.display();
}

void displayTime() {
  int col1 = 60;
  int col2 = 100;

  tmElements_t tm;
  breakTime(epochTime, tm);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Connect to Wi-Fi
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("   ++ Time Sync ++");

  // Display Power

  display.print("Date ");
  display.setCursor(col1,8);
  display.print(String(tm.Day) +"."+ String(tm.Month) + ".20" + String((tm.Year + 1970) % 100)  );
  display.setCursor(col2,8);
  display.println("");

  // Display Energy Today
  display.print("Time");
  display.setCursor(col1,16);
  display.print(String(tm.Hour) +":"+ String(tm.Minute) +":"+ String(tm.Second) );
  display.setCursor(col2,16);
  display.println(" ");

  // Display Total Energy
  display.print("Sync");
  display.setCursor(col1,24);
  display.print(String( lastUpdate - millis() ) );
  display.setCursor(col2,24);
  display.println(" ");

  display.display();
}

void display_invert_blink(int times, int delay_ms){
    for (int i =0; i < times ; i++){
        display.invertDisplay(true);
        display.display();
        delay(delay_ms/2);
        display.invertDisplay(false);
        display.display();
        delay(delay_ms/2);
    }

}


// ToDo: TLS Based MQTT connection

////////////////////////////////////////////////////////////////////
// MQTT SECTION
void mqtt_submit_data(){
    
    // Connect to Wi-Fi
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println(F("++ Publish to MQTT ++"));
    display.println("Server: ");
    display.print(MQTT_BROKER_HOST.c_str());
    display.print(":");
    display.println(MQTT_BROKER_PORT);
    display.display();
    
    if (!mqtt_client.connected()) {
        mqtt_reconnect();
    }

     if (mqtt_client.connected()) {
        Serial.println("Connected to MQTT server");
        mqtt_client.loop();

        // Publish data to a topic
        String data = "";
        String topic = "";


      // Publish getWebdataSn
      String snTopic = MQTT_BROKER_MAINTOPIC + "/inverter/serial";
      mqtt_client.publish(snTopic.c_str(), inverter.getInverterSerial().c_str());

    /* Removing unpopulated 
      // Publish getWebdataMsvn
      String msvnTopic = MQTT_BROKER_MAINTOPIC + "/inverter/msvn";
      mqtt_client.publish(msvnTopic.c_str(), inverter.getWebdataMsvn().c_str());

      // Publish getWebdataSsvn
      String ssvnTopic = MQTT_BROKER_MAINTOPIC + "/inverter/ssvn";
      mqtt_client.publish(ssvnTopic.c_str(), inverter.getWebdataSsvn().c_str());

      // Publish getWebdataPvType
      String pvTypeTopic = MQTT_BROKER_MAINTOPIC + "/inverter/pvType";
      mqtt_client.publish(pvTypeTopic.c_str(), inverter.getWebdataPvType().c_str());

      // Publish getWebdataRateP
      String ratePTopic = MQTT_BROKER_MAINTOPIC + "/inverter/rateP";
      mqtt_client.publish(ratePTopic.c_str(), inverter.getWebdataRateP().c_str());
    */

      // Publish getInverterPowerNow_W (float)
      float nowPTopic = inverter.getInverterPowerNow_W();
      String nowPTopicName = MQTT_BROKER_MAINTOPIC + "/inverter/power_W";
      mqtt_client.publish(nowPTopicName.c_str(), String(nowPTopic).c_str());

      // Publish getInverterEnergyToday_kWh (float)
      float todayETopic = inverter.getInverterEnergyToday_kWh();
      String todayETopicName = MQTT_BROKER_MAINTOPIC + "/inverter/energy_today_kWh";
      mqtt_client.publish(todayETopicName.c_str(), String(todayETopic).c_str());

      // Publish getInverterEnergyTotal_kWh (float)
      float totalETopic = inverter.getInverterEnergyTotal_kWh();
      String totalETopicName = MQTT_BROKER_MAINTOPIC + "/inverter/energy_total_kWh";
      mqtt_client.publish(totalETopicName.c_str(), String(totalETopic).c_str());

      // Publish getWebdataAlarm
      String alarmTopic = MQTT_BROKER_MAINTOPIC + "/status/alarm";
      mqtt_client.publish(alarmTopic.c_str(), inverter.getWebdataAlarm().c_str());

      // Publish getWebdataUtime
      String utimeTopic = MQTT_BROKER_MAINTOPIC + "/status/utime";
      mqtt_client.publish(utimeTopic.c_str(), inverter.getWebdataUtime().c_str());

      // Publish getLoggerModuleID
      String coverMidTopic = MQTT_BROKER_MAINTOPIC + "/logger/serial";
      mqtt_client.publish(coverMidTopic.c_str(), inverter.getLoggerModuleID().c_str());

      // Publish getLoggerSoftwareVersion
      String coverVerTopic = MQTT_BROKER_MAINTOPIC + "/logger/version";
      mqtt_client.publish(coverVerTopic.c_str(), inverter.getLoggerSoftwareVersion().c_str());

      // Publish getLoggerWifiMode
      String coverWmodeTopic = MQTT_BROKER_MAINTOPIC + "/logger/wifi_mode";
      mqtt_client.publish(coverWmodeTopic.c_str(), inverter.getLoggerWifiMode().c_str());

      // Publish getLoggerApSsid
      String coverApSsidTopic = MQTT_BROKER_MAINTOPIC + "/logger/ApSsid";
      mqtt_client.publish(coverApSsidTopic.c_str(), inverter.getLoggerApSsid().c_str());

      // Publish getLoggerApIp
      String coverApIpTopic = MQTT_BROKER_MAINTOPIC + "/logger/ApIp";
      mqtt_client.publish(coverApIpTopic.c_str(), inverter.getLoggerApIp().c_str());

      // Publish getLoggerApMac
      String coverApMacTopic = MQTT_BROKER_MAINTOPIC + "/logger/ApMac";
      mqtt_client.publish(coverApMacTopic.c_str(), inverter.getLoggerApMac().c_str());

      // Publish getLoggerStaSsid
      String coverStaSsidTopic = MQTT_BROKER_MAINTOPIC + "/logger/StaSsid";
      mqtt_client.publish(coverStaSsidTopic.c_str(), inverter.getLoggerStaSsid().c_str());

      // Publish getLoggerStaRssi
      String coverStaRssiTopic = MQTT_BROKER_MAINTOPIC + "/logger/StaRssi";
      mqtt_client.publish(coverStaRssiTopic.c_str(), inverter.getLoggerStaRssi().c_str());

      // Publish getLoggerStaIp
      String coverStaIpTopic = MQTT_BROKER_MAINTOPIC + "/logger/StaIp";
      mqtt_client.publish(coverStaIpTopic.c_str(), inverter.getLoggerStaIp().c_str());

      // Publish getLoggerStaMac
      String coverStaMacTopic = MQTT_BROKER_MAINTOPIC + "/logger/StaMac";
      mqtt_client.publish(coverStaMacTopic.c_str(), inverter.getLoggerStaMac().c_str());

      // Publish getRemoteServerStatusA
      String statusATopic = MQTT_BROKER_MAINTOPIC + "/remote-server/statusA";
      mqtt_client.publish(statusATopic.c_str(), inverter.getRemoteServerStatusA().c_str());

      // Publish getRemoteServerStatusB
      String statusBTopic = MQTT_BROKER_MAINTOPIC + "/remote-server/statusB";
      mqtt_client.publish(statusBTopic.c_str(), inverter.getRemoteServerStatusB().c_str());

      // Publish getRemoteServerStatusC
      String statusCTopic = MQTT_BROKER_MAINTOPIC + "/remote-server/statusC";
      mqtt_client.publish(statusCTopic.c_str(), inverter.getRemoteServerStatusC().c_str());

      // Publish getLastUpdateTimestamp (unsigned long)
      unsigned long lastUpdateTimestamp = inverter.getLastUpdateTimestamp();
      String lastUpdateTopic = MQTT_BROKER_MAINTOPIC + "/lastUpdateTimestamp";
      mqtt_client.publish(lastUpdateTopic.c_str(), String(lastUpdateTimestamp).c_str());


      Serial.println("> MQTT data published"); 
      Serial.println("> MQTT CLOSED");

      mqtt_client.disconnect();

      display.print(" > published");
      display.display();

    }else{
      display.println(" > FAIL !");
      display.display();
      display_invert_blink(5,2000);
    }
     
}

void mqtt_reconnect() {
  int attempts = 0;
  while (!mqtt_client.connected() && attempts < 5) {
    Serial.print("Attempting MQTT connection...");
    
    if (mqtt_client.connect("deye-esp-solar-bridge", MQTT_BROKER_USER.c_str(), MQTT_BROKER_PWD.c_str())) {
      Serial.println("connected");
      
      display.print("connected");
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 2 seconds");
      display.print(".");
      delay(2000);
    }
    attempts++;
  } 
}
