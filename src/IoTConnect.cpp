#include "IoTConnect.h"
#include "Config.h"
#include "Portal.h"
#include "Net.h"
#include "MqttClient.h"

// Instancia global singleton
IoTConnectClass IoTConnect;

void IoTConnectClass::begin(const char* apName, const char* appName) {
  _apName = apName;
  _appName = appName;
  _initialized = true;
  bool justConfigured = false;  // Flag para saber si viene del portal
  
  Serial.begin(115200);
  Serial.printf("\n=== %s IoT Connect v1.0 ===\n", _appName);
  
  setPortalNames(_apName, _appName);
  loadConfig(g_cfg);
  
  if (!g_cfg.confirmed) {
    justConfigured = true;  // Primera configuración
    enterPortalMode();
    while (!g_cfg.confirmed) {
      handlePortalLoop();
      delay(10);
    }
    stopPortal();
  }
  
  Serial.println("[IOT] Conectando WiFi...");
  if (!connectWifi(g_cfg)) {
    Serial.println("[NET] WiFi falló, volviendo a portal");
    clearConfig();
    memset(&g_cfg, 0, sizeof(g_cfg));
    
    justConfigured = true;  // Reconfiguración
    enterPortalMode();
    while (!g_cfg.confirmed) {
      handlePortalLoop();
      delay(10);
    }
    stopPortal();
    
    if (!connectWifi(g_cfg)) {
      Serial.println("[NET] WiFi falló nuevamente, reiniciando...");
      ESP.restart();
    }
  }
  
  mqttBegin();
  Serial.println("[IOT] Conectando MQTT...");
  _mqttFailCount = 0;
  
  while (!mqttConnect(g_cfg)) {
    _mqttFailCount++;
    
    if (_mqttFailCount >= 4) {
      Serial.println("[MQTT] 4 fallos, volviendo a portal");
      clearConfig();
      memset(&g_cfg, 0, sizeof(g_cfg));
      
      justConfigured = true;  // Reconfiguración tras fallo MQTT
      enterPortalMode();
      while (!g_cfg.confirmed) {
        handlePortalLoop();
        delay(10);
      }
      stopPortal();
      
      if (!connectWifi(g_cfg)) {
        ESP.restart();
      }
      _mqttFailCount = 0;
    } else {
      Serial.printf("[MQTT] Reintento %d/4...\n", _mqttFailCount);
      delay(2000);
    }
  }
  
  // Solo publicar sync si es primera configuración desde el portal
  if (justConfigured) {
    Serial.println("[IOT] Primera configuración, enviando sync...");
    // Procesar varios loops antes del sync
    for (int i = 0; i < 10; i++) {
      mqttLoop();
      delay(100);
    }
    
    if (publishOkSync(g_cfg)) {
      Serial.printf("[IOT] Sync enviado a %s/devices/sync\n", g_cfg.publicId);
    } else {
      Serial.println("[IOT] Error enviando sync");
    }
  }
  
  // Procesar paquetes MQTT y estabilizar conexión ANTES de notificar
  Serial.println("[IOT] Estabilizando conexión...");
  for (int i = 0; i < 20; i++) {
    mqttLoop();
    delay(50);
  }
  
  // Verificar que sigue conectado después de estabilizar
  if (!isMqttConnected()) {
    Serial.println("[IOT] Conexión perdida durante estabilización, reintentando...");
    if (!mqttConnect(g_cfg)) {
      Serial.println("[IOT] Reconexión fallida, reiniciando...");
      ESP.restart();
    }
    // Estabilizar de nuevo
    for (int i = 0; i < 20; i++) {
      mqttLoop();
      delay(50);
    }
  }
  
  Serial.printf("[IOT] %s conectado y estable!\n", _appName);
  _normalOperation = true;
  _wasConnected = true;
  
  // Ahora sí notificar - la conexión está estable
  notifyConnectionChange(true);
}

void IoTConnectClass::loop() {
  if (!_initialized) return;
  
  if (isPortalActive()) {
    handlePortalLoop();
    delay(10);
  } else if (_normalOperation) {
    handleNormalOperation();
    delay(100);
  }
}

void IoTConnectClass::enterPortalMode() {
  Serial.printf("[IOT] Portal: %s en 192.168.4.1\n", _apName);
  startPortal();
}

void IoTConnectClass::handlePortalLoop() {
  portalLoop();
}

void IoTConnectClass::handleNormalOperation() {
  ensureWifi();
  
  if (isWifiConnected()) {
    if (isMqttConnected()) {
      mqttLoop();
      _mqttFailCount = 0;
      
      if (!_wasConnected) {
        // Estabilizar conexión BIEN antes de notificar
        Serial.println("[IOT] Reconexión detectada, estabilizando...");
        for (int i = 0; i < 20; i++) {
          if (!isMqttConnected()) break;
          mqttLoop();
          delay(50);
        }
        
        // Solo notificar si sigue conectado
        if (isMqttConnected()) {
          _wasConnected = true;
          Serial.println("[IOT] Conexión estable, notificando...");
          notifyConnectionChange(true);
        }
      }
    } else {
      if (_wasConnected) {
        _wasConnected = false;
        notifyConnectionChange(false);
      }
      
      unsigned long now = millis();
      if (now - _lastMqttRetry > 5000) {
        _lastMqttRetry = now;
        
        if (!mqttConnect(g_cfg)) {
          _mqttFailCount++;
          if (_mqttFailCount >= 4) {
            Serial.println("[MQTT] 4 fallos, volviendo a portal");
            clearConfig();
            memset(&g_cfg, 0, sizeof(g_cfg));
            _normalOperation = false;
            startPortal();
          }
        }
      }
    }
  }
}

void IoTConnectClass::notifyConnectionChange(bool connected) {
  if (_connectionCallback) _connectionCallback(connected);
}

bool IoTConnectClass::isReady() {
  return _normalOperation && isWifiConnected() && isMqttConnected();
}

bool IoTConnectClass::isConfigMode() {
  return isPortalActive();
}

bool IoTConnectClass::publish(const char* topic, const char* payload, bool retained) {
  if (!isReady() || !isMqttStable()) return false;
  return mqttPublish(topic, payload, retained);
}

bool IoTConnectClass::subscribe(const char* topic) {
  if (!isReady() || !isMqttStable()) return false;
  return mqttSubscribe(topic);
}

void IoTConnectClass::onMessage(MqttMessageCallback callback) {
  _messageCallback = callback;
  setMqttMessageCallback([this](const char* topic, const char* payload) {
    if (_messageCallback) _messageCallback(topic, payload);
  });
}

void IoTConnectClass::onConnectionChange(ConnectionCallback callback) {
  _connectionCallback = callback;
}

const char* IoTConnectClass::getClientId() { return g_cfg.clientId; }
const char* IoTConnectClass::getPublicId() { return g_cfg.publicId; }

void IoTConnectClass::resetConfig() {
  Serial.println("[IOT] Reset config");
  clearConfig();
  memset(&g_cfg, 0, sizeof(g_cfg));
  _normalOperation = false;
  startPortal();
}
