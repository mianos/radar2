#pragma once
#include <vector>
#include <SPIFFS.h>
#include <ArduinoJson.h>

struct SettingsManager {
  String mqttServer = "mqtt2.mianos.com";
  int mqttPort = 1883;
  String sensorName = "radar2";
  int tracking = 0;
  int presence = 10000;
  int detectionTimeout = 60000;
  String tz = "AEST-10AEDT,M10.1.0,M4.1.0/3";

  SettingsManager(); // Constructor declaration

  void fillJsonDocument(JsonDocument& doc);
  void printSettings();
  enum class SettingChange {
    None = 0,
    //VolumeChanged
  };
  std::vector<SettingChange> loadFromDocument(JsonDocument& doc);
};
