#ifndef ESP8266_SETTINGS_MANAGER_H
#define ESP8266_SETTINGS_MANAGER_H
#include <map>
#include <Ticker.h> // Include Ticker library

#include <ESP8266WebServer.h>

class ESP8266SettingsManager {
public:
  ESP8266SettingsManager(ESP8266WebServer& server);

  void begin();
  void stop();
  void eraseAndInitializeEEPROM(); 

  void setSetting(const String& label, const String& value);
  String getSetting(const String& label);

private:
  const int EEPROM_NUM_ENTRIES                      = 13;
  const int EEPROM_SIZE_ENTRIES                     = 128;

  const int EEPROM_ADDRESS_WIFI_HOME_SSID           =  0 * EEPROM_SIZE_ENTRIES;
  const int EEPROM_ADDRESS_WIFI_HOME_KEY            =  1 * EEPROM_SIZE_ENTRIES; 
  const int EEPROM_ADDRESS_WIFI_INVERTER_SSID       =  2 * EEPROM_SIZE_ENTRIES;
  const int EEPROM_ADDRESS_WIFI_INVERTER_KEY        =  3 * EEPROM_SIZE_ENTRIES;
  const int EEPROM_ADDRESS_WIFI_RELAIS_SSID         =  4 * EEPROM_SIZE_ENTRIES;
  const int EEPROM_ADDRESS_WIFI_RELAIS_KEY          =  5 * EEPROM_SIZE_ENTRIES;
  const int EEPROM_ADDRESS_MQTT_BROKER_HOST         =  6 * EEPROM_SIZE_ENTRIES;
  const int EEPROM_ADDRESS_MQTT_BROKER_PORT         =  7 * EEPROM_SIZE_ENTRIES;
  const int EEPROM_ADDRESS_MQTT_BROKER_USER         =  8 * EEPROM_SIZE_ENTRIES;
  const int EEPROM_ADDRESS_MQTT_BROKER_PWD          =  9 * EEPROM_SIZE_ENTRIES;
  const int EEPROM_ADDRESS_MQTT_BROKER_MAINTOPIC    = 10 * EEPROM_SIZE_ENTRIES;
  const int EEPROM_ADDRESS_INVERTER_WEBACCESS_USER  = 11 * EEPROM_SIZE_ENTRIES;
  const int EEPROM_ADDRESS_INVERTER_WEBACCESS_PWD   = 12 * EEPROM_SIZE_ENTRIES;

  bool serverActive = false;
  Ticker timer; // Declare the Ticker object
  

  std::map<String, String> settings;
  ESP8266WebServer& server;

  void loop();
  void loadSettings();
  String getDefaultSetting(const String& label);
  void writeEEPROM(const String& label, const String& data);
  String readEEPROM(const String& label);
  void handleRoot();
  void handleSave();
  String getSettingHTML(const String& label, const String& value);
  void debugPrint(const String& message);
};

#endif
