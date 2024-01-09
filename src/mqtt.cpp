#include <ESPDateTime.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <StringSplitter.h>
#include <memory>

#include "provision.h"

#include "mqtt.h"

enum State {
    NOT_COPYING,
    COPYING,
    WAITING_FOR_FLUSH
} currentState = NOT_COPYING;


unsigned long lastCopyTime = 0;
const unsigned long flushDelay = 1000; // 1000 ms flush delay


void RadarMqtt::callback(char* topic_str, byte* payload, unsigned int length) {
  auto topic = String(topic_str);
  auto splitter = StringSplitter(topic, '/', 4);
  int itemCount = splitter.getItemCount();
  if (itemCount < 3) {
    Serial.printf("Item count less than 3 %d '%s'\n", itemCount, topic_str);
    return;
  }
#if 1
  for (int i = 0; i < itemCount; i++) {
    String item = splitter.getItemAtIndex(i);
    Serial.println("Item @ index " + String(i) + ": " + String(item));
  }
  Serial.printf("command '%s'\n", splitter.getItemAtIndex(itemCount - 1).c_str());
#endif
  
  if (splitter.getItemAtIndex(0) == "cmnd") {
    JsonDocument jpl;
    auto err = deserializeJson(jpl, payload, length);
    if (err) {
      Serial.printf("deserializeJson() failed: '%s'\n", err.c_str());
      return;
    }
    String output;
    serializeJson(jpl, output);
    auto dest = splitter.getItemAtIndex(itemCount - 1);
    if (dest == "reprovision") {
        Serial.printf("clearing provisioning\n");
        reset_provisioning();
    } else if (dest == "restart") {
        Serial.printf("rebooting\n");
        ESP.restart();
    } else if (dest == "settings") {
      auto result = settings->loadFromDocument(jpl);
//      if (std::find(result.begin(), result.end(), SettingsManager::SettingChange::VolumeChanged) != result.end()) {
//        volume.setVolume(settings->volume / 100);
//        prev_volume = volume.volume();  // if volume is set during a play, go back to the one set by settings.
//      }
      settings->printSettings();
    }
  }
}


RadarMqtt::RadarMqtt(std::shared_ptr<SettingsManager> settings)
    : settings(settings), client(espClient) {
  client.setServer(settings->mqttServer.c_str(), settings->mqttPort);
  Serial.printf("init mqtt, server '%s'\n", settings->mqttServer.c_str());
  client.setCallback([this](char* topic_str, byte* payload, unsigned int length) {
    callback(topic_str, payload, length);
  });
}


bool RadarMqtt::reconnect() {
  String clientId =  WiFi.getHostname(); // name + '-' + String(random(0xffff), HEX);
  Serial.printf("Attempting MQTT connection... to %s name %s\n", settings->mqttServer.c_str(), clientId.c_str());
  if (client.connect(clientId.c_str())) {
    delay(1000);
    String cmnd_topic = String("cmnd/") + settings->sensorName + "/#";
    client.subscribe(cmnd_topic.c_str());
    Serial.printf("mqtt connected as sensor '%s'\n", settings->sensorName.c_str());

    JsonDocument doc;
    doc["version"] = 2;
    doc["time"] = DateTime.toISOString();
    doc["hostname"] = WiFi.getHostname();
    doc["ip"] = WiFi.localIP().toString();
    settings->fillJsonDocument(doc);
    String status_topic = "tele/" + settings->sensorName + "/init";
    String output;
    serializeJson(doc, output);
    client.publish(status_topic.c_str(), output.c_str());
    return true;
  } else {
    Serial.printf("failed to connect to %s port %d state %d\n", settings->mqttServer.c_str(), settings->mqttPort, client.state());
    delay(10000);
    return false;
  }
}

void RadarMqtt::handle() {
  if (!client.connected()) {
    if (!reconnect()) {
      return;
    }
  }
  client.loop();  // mqtt client loop
}



void RadarMqtt::mqtt_update_presence(bool entry, const Value *vv) {
  JsonDocument doc;
  doc["entry"] = entry;
  doc["time"] = DateTime.toISOString();
  if (entry) {
    vv->toJson(doc);
  }
  String status_topic = "tele/" + settings->sensorName + "/presence";
  String output;
  serializeJson(doc, output);
//  Serial.printf("sending '%s' to '%s'\n", output.c_str(), status_topic.c_str());
  client.publish(status_topic.c_str(), output.c_str());
}

void RadarMqtt::mqtt_track(const Value *vv) {
  JsonDocument doc;
  doc["time"] = DateTime.toISOString();
  vv->toJson(doc);
  String status_topic = "tele/" + settings->sensorName + "/tracking";
  String output;
  serializeJson(doc, output);
//  Serial.printf("sending '%s' to '%s'\n", output.c_str(), status_topic.c_str());
  client.publish(status_topic.c_str(), output.c_str());
}

