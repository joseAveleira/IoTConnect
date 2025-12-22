#include "MqttClient.h"
#include <WiFi.h>
#include <PubSubClient.h>

static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);
static int failCount = 0;
static InternalMqttCallback userCallback = nullptr;

static void internalCallback(char* topic, byte* payload, unsigned int length) {
  char* payloadStr = new char[length + 1];
  memcpy(payloadStr, payload, length);
  payloadStr[length] = '\0';
  
  Serial.printf("[MQTT] Recibido: %s -> %s\n", topic, payloadStr);
  if (userCallback) userCallback(topic, payloadStr);
  delete[] payloadStr;
}

void mqttBegin() {
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(internalCallback);
  Serial.printf("[MQTT] Configurado: %s:%d\n", MQTT_HOST, MQTT_PORT);
}

bool mqttConnect(const AppConfig& cfg) {
  if (mqttClient.connected()) return true;
  
  if (strlen(cfg.clientId) == 0 || strlen(cfg.token) == 0) {
    Serial.println("[MQTT] Error: clientId o token vacíos");
    return false;
  }
  
  Serial.printf("[MQTT] Conectando como %s\n", cfg.clientId);
  
  if (mqttClient.connect(cfg.clientId, cfg.clientId, cfg.token)) {
    Serial.println("[MQTT] Conectado!");
    failCount = 0;
    return true;
  }
  
  failCount++;
  Serial.printf("[MQTT] Error: %d (fallos: %d)\n", mqttClient.state(), failCount);
  return false;
}

void mqttLoop() {
  if (mqttClient.connected()) mqttClient.loop();
}

void mqttDisconnect() {
  if (mqttClient.connected()) {
    mqttClient.disconnect();
    Serial.println("[MQTT] Desconectado");
  }
}

bool publishOkSync(const AppConfig& cfg) {
  if (!mqttClient.connected() || strlen(cfg.publicId) == 0) return false;
  
  char topic[128];
  snprintf(topic, sizeof(topic), "%s/devices/sync", cfg.publicId);
  return mqttClient.publish(topic, "ok");
}

bool isMqttConnected() { return mqttClient.connected(); }
int getMqttFailCount() { return failCount; }

bool mqttPublish(const char* topic, const char* payload, bool retained) {
  if (!mqttClient.connected()) return false;
  bool result = mqttClient.publish(topic, payload, retained);
  if (result) Serial.printf("[MQTT] Pub: %s\n", topic);
  return result;
}

bool mqttSubscribe(const char* topic) {
  if (!mqttClient.connected()) return false;
  bool result = mqttClient.subscribe(topic);
  if (result) Serial.printf("[MQTT] Sub: %s\n", topic);
  return result;
}

void setMqttMessageCallback(InternalMqttCallback callback) {
  userCallback = callback;
}
