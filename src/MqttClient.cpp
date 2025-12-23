#include "MqttClient.h"
#include <WiFi.h>
#include <PubSubClient.h>

static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);
static int failCount = 0;
static InternalMqttCallback userCallback = nullptr;

// Flag para indicar que la conexión está estabilizada
static bool connectionStable = false;
static unsigned long connectionStableTime = 0;

// Espera activa para dar tiempo a que el broker consolide la conexión
// y a que el stack TCP procese paquetes pendientes.
static bool waitForStability(unsigned long minMs) {
  if (!mqttClient.connected()) return false;
  unsigned long now = millis();
  unsigned long elapsed = now - connectionStableTime;
  if (elapsed >= minMs) return true;
  unsigned long remaining = minMs - elapsed;
  unsigned long start = millis();
  while (millis() - start < remaining) {
    if (!mqttClient.connected()) return false;
    mqttClient.loop();
    delay(10);
  }
  return mqttClient.connected();
}

static void internalCallback(char* topic, byte* payload, unsigned int length) {
  char* payloadStr = new char[length + 1];
  memcpy(payloadStr, payload, length);
  payloadStr[length] = '\0';
  
  Serial.printf("[MQTT] Recibido: %s -> %s\n", topic, payloadStr);
  if (userCallback) userCallback(topic, payloadStr);
  delete[] payloadStr;
}

void mqttBegin() {
  // Configurar buffer más grande para mensajes
  mqttClient.setBufferSize(1024);
  // Keepalive más largo para conexiones lentas
  mqttClient.setKeepAlive(60);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(internalCallback);
  Serial.printf("[MQTT] Configurado: %s:%d (buffer: 1024, keepalive: 60s)\n", MQTT_HOST, MQTT_PORT);
}

bool mqttConnect(const AppConfig& cfg) {
  if (mqttClient.connected()) return true;
  
  connectionStable = false;
  
  if (strlen(cfg.clientId) == 0 || strlen(cfg.token) == 0) {
    Serial.println("[MQTT] Error: clientId o token vacíos");
    return false;
  }
  
  Serial.printf("[MQTT] Conectando como %s\n", cfg.clientId);
  
  if (mqttClient.connect(cfg.clientId, cfg.clientId, cfg.token)) {
    Serial.println("[MQTT] Conectado!");
    failCount = 0;
    
    // Marcar tiempo de conexión para estabilización
    connectionStableTime = millis();
    
    // Procesar varios loops para estabilizar la conexión
    for (int i = 0; i < 10; i++) {
      mqttClient.loop();
      delay(50);
    }
    
    connectionStable = true;
    return true;
  }
  
  failCount++;
  Serial.printf("[MQTT] Error: %d (fallos: %d)\n", mqttClient.state(), failCount);
  return false;
}

void mqttLoop() {
  if (mqttClient.connected()) {
    mqttClient.loop();
  }
}

void mqttDisconnect() {
  connectionStable = false;
  if (mqttClient.connected()) {
    mqttClient.disconnect();
    Serial.println("[MQTT] Desconectado");
  }
}

bool publishOkSync(const AppConfig& cfg) {
  if (!mqttClient.connected() || strlen(cfg.publicId) == 0) return false;
  
  // Asegurar estabilidad antes de publicar sync
  for (int i = 0; i < 5; i++) {
    mqttClient.loop();
    delay(20);
  }
  
  char topic[128];
  snprintf(topic, sizeof(topic), "%s/devices/sync", cfg.publicId);
  bool result = mqttClient.publish(topic, "ok");
  mqttClient.loop();
  return result;
}

bool isMqttConnected() { return mqttClient.connected(); }
bool isMqttStable() { return connectionStable && mqttClient.connected(); }
int getMqttFailCount() { return failCount; }

bool mqttPublish(const char* topic, const char* payload, bool retained) {
  if (!mqttClient.connected()) {
    Serial.println("[MQTT] Pub fallido: no conectado");
    return false;
  }

  // Asegurar que han pasado al menos 800ms desde la conexión
  if (!waitForStability(800)) {
    Serial.println("[MQTT] Pub fallido: conexión inestable");
    return false;
  }
  
  // Procesar paquetes pendientes antes de publicar
  for (int i = 0; i < 3; i++) {
    mqttClient.loop();
    delay(10);
  }
  
  // Verificar conexión después de procesar
  if (!mqttClient.connected()) {
    Serial.println("[MQTT] Pub fallido: desconexión durante preparación");
    return false;
  }
  
  bool result = mqttClient.publish(topic, payload, retained);
  if (result) {
    Serial.printf("[MQTT] Pub OK: %s\n", topic);
    // Procesar ACK
    for (int i = 0; i < 3; i++) {
      mqttClient.loop();
      delay(10);
    }
  } else {
    Serial.printf("[MQTT] Pub FAIL: %s\n", topic);
  }
  return result;
}

bool mqttSubscribe(const char* topic) {
  if (!mqttClient.connected()) {
    Serial.println("[MQTT] Sub fallido: no conectado");
    return false;
  }

  // Asegurar un pequeño margen de estabilidad tras conectar
  if (!waitForStability(600)) {
    Serial.println("[MQTT] Sub fallido: conexión inestable");
    return false;
  }
  
  // Procesar paquetes antes de suscribirse
  mqttClient.loop();
  delay(20);
  
  bool result = mqttClient.subscribe(topic);
  if (result) {
    Serial.printf("[MQTT] Sub OK: %s\n", topic);
    // Espera corta para captar SUBACK sin penalizar con timeouts largos
    for (int i = 0; i < 10; i++) {
      if (!mqttClient.connected()) {
        Serial.println("[MQTT] Sub: desconexión durante espera");
        return false;
      }
      mqttClient.loop();
      delay(15);
    }
  } else {
    Serial.printf("[MQTT] Sub FAIL: %s\n", topic);
  }
  return result;
}

void setMqttMessageCallback(InternalMqttCallback callback) {
  userCallback = callback;
}
