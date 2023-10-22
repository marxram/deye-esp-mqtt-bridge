#include "ESP8266SettingsManager.h"
#include <EEPROM.h>
//#include <map>
#include <Ticker.h> // Include Ticker library
#include "arduino_secrets.h"

#define DEBUG true  // Set to true to enable debug messages, false to disable

ESP8266SettingsManager::ESP8266SettingsManager(ESP8266WebServer& server) : server(server) {}


void ESP8266SettingsManager::eraseAndInitializeEEPROM() {
  EEPROM.begin(EEPROM_NUM_ENTRIES * EEPROM_SIZE_ENTRIES);
  
  // Fill the entire EEPROM with zeros
  for (int address = 0; address < EEPROM.length(); address++) {
    EEPROM.write(address, 0);
  }

  // Commit and end EEPROM
  EEPROM.commit();
  EEPROM.end();
}

void ESP8266SettingsManager::begin() {
  serverActive = true;
  EEPROM.begin(EEPROM_NUM_ENTRIES * EEPROM_SIZE_ENTRIES);
  loadSettings();
  server.on("/", HTTP_GET, std::bind(&ESP8266SettingsManager::handleRoot, this));
  server.on("/save", HTTP_POST, std::bind(&ESP8266SettingsManager::handleSave, this));
  server.begin();
  debugPrint("Server started.");
  // Start handling
  timer.attach(15.0, std::bind(&ESP8266SettingsManager::loop, this)); // Trigger the loop function every 100ms
}

void ESP8266SettingsManager::stop() {
  serverActive = false;
  debugPrint("Server stopped.");
  timer.detach();
  server.stop();
}

void ESP8266SettingsManager::loop() {
  if (serverActive) {
    // It's time to check for clients
    server.handleClient();  // Check for and handle incoming client requests;
    debugPrint("handle Client..");
  }
}

void ESP8266SettingsManager::loadSettings() {
  String settingLabels[] = {
    "WIFI_HOME_SSID", 
    "WIFI_HOME_KEY",
    "WIFI_INVERTER_SSID",
    "WIFI_INVERTER_KEY",
    "WIFI_RELAIS_SSID",
    "WIFI_RELAIS_KEY",
    "MQTT_BROKER_HOST", 
    "MQTT_BROKER_PORT",
    "MQTT_BROKER_USER", 
    "MQTT_BROKER_PWD",
    "MQTT_BROKER_MAINTOPIC",
    "INVERTER_WEBACCESS_USER", 
    "INVERTER_WEBACCESS_PWD"
  };

  for (const String& label : settingLabels) {
    String setting = readEEPROM(label);
    if (setting.length() > 0) {
      settings[label] = setting;
      debugPrint("Loaded setting: " + label + " = " + settings[label]);
    } else {
      settings[label] = getDefaultSetting(label);
      writeEEPROM(label, settings[label]);
      debugPrint("Default setting applied: " + label + " = " + settings[label]);
    }
  }
}

String ESP8266SettingsManager::getDefaultSetting(const String& label) {
  if (label == "WIFI_HOME_SSID") return SECRET_WIFI_HOME_SSID;
  else if (label == "WIFI_HOME_KEY") return SECRET_WIFI_HOME_KEY;
  else if (label == "WIFI_INVERTER_SSID") return SECRET_WIFI_INVERTER_SSID;
  else if (label == "WIFI_INVERTER_KEY") return SECRET_WIFI_INVERTER_KEY;
  else if (label == "WIFI_RELAIS_SSID") return SECRET_WIFI_RELAIS_SSID;
  else if (label == "WIFI_RELAIS_KEY") return SECRET_WIFI_RELAIS_KEY;
  else if (label == "MQTT_BROKER_HOST") return SECRET_MQTT_BROKER_HOST;
  else if (label == "MQTT_BROKER_PORT") return String(SECRET_MQTT_BROKER_PORT);
  else if (label == "MQTT_BROKER_USER") return SECRET_MQTT_BROKER_USER;
  else if (label == "MQTT_BROKER_PWD") return SECRET_MQTT_BROKER_PWD;
  else if (label == "MQTT_BROKER_MAINTOPIC") return SECRET_MQTT_BROKER_MAINTOPIC;
  else if (label == "INVERTER_WEBACCESS_USER") return SECRET_INVERTER_WEBACCESS_USER;
  else if (label == "INVERTER_WEBACCESS_PWD") return SECRET_INVERTER_WEBACCESS_PWD;
  // Add more cases for additional settings if needed
  else return "";
}

void ESP8266SettingsManager::writeEEPROM(const String& label, const String& data) {
  int address = -1; // Initialize address to an invalid value
  
  if (label == "WIFI_HOME_SSID") {
    address = ESP8266SettingsManager::EEPROM_ADDRESS_WIFI_HOME_SSID;
  } else if (label == "WIFI_HOME_KEY") {
    address = ESP8266SettingsManager::EEPROM_ADDRESS_WIFI_HOME_KEY;
  } else if (label == "WIFI_INVERTER_SSID") {
    address = ESP8266SettingsManager::EEPROM_ADDRESS_WIFI_INVERTER_SSID;
  } else if (label == "WIFI_INVERTER_KEY") {
    address = ESP8266SettingsManager::EEPROM_ADDRESS_WIFI_INVERTER_KEY;
  } else if (label == "WIFI_RELAIS_SSID") {
    address = ESP8266SettingsManager::EEPROM_ADDRESS_WIFI_RELAIS_SSID;
  } else if (label == "WIFI_RELAIS_KEY") {
    address = ESP8266SettingsManager::EEPROM_ADDRESS_WIFI_RELAIS_KEY;
  } else if (label == "MQTT_BROKER_HOST") {
    address = ESP8266SettingsManager::EEPROM_ADDRESS_MQTT_BROKER_HOST;
  } else if (label == "MQTT_BROKER_PORT") {
    address = ESP8266SettingsManager::EEPROM_ADDRESS_MQTT_BROKER_PORT;
  } else if (label == "MQTT_BROKER_USER") {
    address = ESP8266SettingsManager::EEPROM_ADDRESS_MQTT_BROKER_USER;
  } else if (label == "MQTT_BROKER_PWD") {
    address = ESP8266SettingsManager::EEPROM_ADDRESS_MQTT_BROKER_PWD;
  } else if (label == "MQTT_BROKER_MAINTOPIC") {
    address = ESP8266SettingsManager::EEPROM_ADDRESS_MQTT_BROKER_MAINTOPIC;
  } else if (label == "INVERTER_WEBACCESS_USER") {
    address = ESP8266SettingsManager::EEPROM_ADDRESS_INVERTER_WEBACCESS_USER;
  } else if (label == "INVERTER_WEBACCESS_PWD") {
    address = ESP8266SettingsManager::EEPROM_ADDRESS_INVERTER_WEBACCESS_PWD;
  }
  
  if (address != -1) {
    EEPROM.begin(512);
    for (int i = 0; i < data.length(); i++) {
      EEPROM.write(address + i, data[i]);
    }
    EEPROM.write(address + data.length(), '\0');
    EEPROM.commit();
    EEPROM.end();
  }

  debugPrint("Setting saved to EEPROM: " + label + " = " + data);

}

String ESP8266SettingsManager::readEEPROM(const String& label) {
  String data = "";
  char c;
  int address = -1; // Initialize address to an invalid value
  
  if (label == "WIFI_HOME_SSID") {
    address = EEPROM_ADDRESS_WIFI_HOME_SSID;
  } else if (label == "WIFI_HOME_KEY") {
    address = EEPROM_ADDRESS_WIFI_HOME_KEY;
  } else if (label == "WIFI_INVERTER_SSID") {
    address = EEPROM_ADDRESS_WIFI_INVERTER_SSID;
  } else if (label == "WIFI_INVERTER_KEY") {
    address = EEPROM_ADDRESS_WIFI_INVERTER_KEY;
  } else if (label == "WIFI_RELAIS_SSID") {
    address = EEPROM_ADDRESS_WIFI_RELAIS_SSID;
  } else if (label == "WIFI_RELAIS_KEY") {
    address = EEPROM_ADDRESS_WIFI_RELAIS_KEY;
  } else if (label == "MQTT_BROKER_HOST") {
    address = EEPROM_ADDRESS_MQTT_BROKER_HOST;
  } else if (label == "MQTT_BROKER_PORT") {
    address = EEPROM_ADDRESS_MQTT_BROKER_PORT;
  } else if (label == "MQTT_BROKER_USER") {
    address = EEPROM_ADDRESS_MQTT_BROKER_USER;
  } else if (label == "MQTT_BROKER_PWD") {
    address = EEPROM_ADDRESS_MQTT_BROKER_PWD;
  } else if (label == "MQTT_BROKER_MAINTOPIC") {
    address = EEPROM_ADDRESS_MQTT_BROKER_MAINTOPIC;
  } else if (label == "INVERTER_WEBACCESS_USER") {
    address = EEPROM_ADDRESS_INVERTER_WEBACCESS_USER;
  } else if (label == "INVERTER_WEBACCESS_PWD") {
    address = EEPROM_ADDRESS_INVERTER_WEBACCESS_PWD;
  }
  
  if (address != -1) {
    EEPROM.begin(512);
    for (int i = 0; i < 512; i++) {
      c = EEPROM.read(address + i);
      if (c == '\0') {
        break;
      }
      data += c;
    }
    EEPROM.end();
  }
  
  debugPrint("Setting read from EEPROM: " + label + " = " + data);

  return data;
}

void ESP8266SettingsManager::handleRoot() {
  String html = "<html><body>";
  html += "<h1>ESP8266 Settings</h1>";
  for (const auto& entry : settings) {
    html += "<p>" + getSettingHTML(entry.first, entry.second) + "</p>";
  }
  html += "<p><form method='POST' action='/save'><input type='submit' value='Save'></form></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
  debugPrint("Sent HTML response to client.");
  debugPrint(html);
}

void ESP8266SettingsManager::handleSave() {
  for (const auto& entry : settings) {
    String value = server.arg(entry.first);
    if (value.length() > 0) {
      settings[entry.first] = value;
      writeEEPROM(entry.first, value);
      debugPrint("Updated setting: " + entry.first + " = " + value);
    }
  }
  server.send(200, "text/plain", "Settings saved.");
  debugPrint("Settings saved.");
}

String ESP8266SettingsManager::getSettingHTML(const String& label, const String& value) {
  String html = "<label>" + label + ": </label>";
  html += "<input type='text' name='" + label + "' value='" + value + "'><br>";
  return html;
}

void ESP8266SettingsManager::setSetting(const String& label, const String& value) {
  if (settings.find(label) != settings.end()) {
    settings[label] = value;
    writeEEPROM(label, value);
  }
}

String ESP8266SettingsManager::getSetting(const String& label) {
  if (settings.find(label) != settings.end()) {
    return settings[label];
  } else {
    return "";
  }
}

void ESP8266SettingsManager::debugPrint(const String& message) {
  if (DEBUG) {
    Serial.println("[WEB] " + message);
  }
}