#include "IoTConnect.h"
#include "Config.h"
#include "Portal.h"
#include "Net.h"
#include "MqttClient.h"
#include <WiFi.h>

// Instancia global singleton
IoTConnectClass IoTConnect;

void IoTConnectClass::begin(const char* apName, const char* appName) {
  _apName = apName;
  _appName = appName;
  _initialized = true;

  Serial.begin(115200);
  Serial.printf("\n=== %s IoT Connect v1.0 ===\n", _appName);

  setPortalNames(_apName, _appName);
  loadConfig(g_cfg);

  if (g_cfg.confirmed && strlen(g_cfg.ssid) > 0) {
    // Hay credenciales guardadas, intentar conectar WiFi
    startWifiConnect();
  } else {
    // Primera vez o sin config
    _justConfigured = true;
    _state = IoTState::PORTAL;
    enterPortalMode();
  }
}

void IoTConnectClass::loop() {
  if (!_initialized) return;

  switch (_state) {
    // ── Portal cautivo ──────────────────────────────────────────────
    case IoTState::PORTAL:
      portalLoop();
      // handleSave() en Portal.cpp pone g_cfg.confirmed = true
      if (g_cfg.confirmed) {
        stopPortal();
        _justConfigured = true;
        startWifiConnect();
      }
      break;

    // ── Conectando WiFi (no bloqueante) ─────────────────────────────
    case IoTState::WIFI_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[NET] WiFi conectado! IP: %s\n",
                      WiFi.localIP().toString().c_str());
        if (!_mqttBeginDone) {
          mqttBegin();
          _mqttBeginDone = true;
        }
        _state = IoTState::MQTT_CONNECTING;
        _mqttFailCount = 0;
        _lastMqttRetry = 0; // intentar MQTT de inmediato
        Serial.println("[IOT] Conectando MQTT...");
      } else if (millis() - _wifiStartTime > WIFI_TIMEOUT_MS) {
        _wifiRetryCount++;
        if (_wifiRetryCount >= WIFI_MAX_RETRIES) {
          Serial.println("[NET] WiFi falló tras varios intentos, abriendo portal");
          // NO borrar credenciales — el portal las muestra pre-rellenadas
          g_cfg.confirmed = false;
          _state = IoTState::PORTAL;
          enterPortalMode();
        } else {
          Serial.printf("[NET] WiFi timeout, reintento %d/%d\n",
                        _wifiRetryCount + 1, WIFI_MAX_RETRIES);
          WiFi.begin(g_cfg.ssid, g_cfg.pass);
          _wifiStartTime = millis();
        }
      }
      break;

    // ── Conectando MQTT ─────────────────────────────────────────────
    case IoTState::MQTT_CONNECTING: {
      if (isMqttConnected()) {
        _state = IoTState::STABILIZING;
        _stabilizeStart = millis();
        Serial.println("[IOT] MQTT conectado, estabilizando...");
        break;
      }
      unsigned long now = millis();
      if (now - _lastMqttRetry > 2000) {
        _lastMqttRetry = now;
        if (mqttConnect(g_cfg)) {
          _state = IoTState::STABILIZING;
          _stabilizeStart = millis();
          Serial.println("[IOT] MQTT conectado, estabilizando...");
        } else {
          _mqttFailCount++;
          if (_mqttFailCount >= MQTT_MAX_RETRIES) {
            Serial.printf("[MQTT] %d fallos, abriendo portal\n", MQTT_MAX_RETRIES);
            g_cfg.confirmed = false;
            _state = IoTState::PORTAL;
            enterPortalMode();
          } else {
            Serial.printf("[MQTT] Reintento %d/%d\n",
                          _mqttFailCount, MQTT_MAX_RETRIES);
          }
        }
      }
      break;
    }

    // ── Estabilizando conexión ──────────────────────────────────────
    case IoTState::STABILIZING:
      mqttLoop();
      if (millis() - _stabilizeStart > STABILIZE_MS) {
        if (isMqttConnected()) {
          if (_justConfigured) {
            Serial.println("[IOT] Enviando sync...");
            publishOkSync(g_cfg);
            _justConfigured = false;
          }
          Serial.printf("[IOT] %s conectado y estable!\n", _appName);
          _state = IoTState::READY;
          _normalOperation = true;
          _wasConnected = true;
          notifyConnectionChange(true);
        } else {
          Serial.println("[IOT] Conexión perdida durante estabilización");
          _state = IoTState::MQTT_CONNECTING;
          _mqttFailCount = 0;
          _lastMqttRetry = 0;
        }
      }
      break;

    // ── Operación normal ────────────────────────────────────────────
    case IoTState::READY:
      handleNormalOperation();
      break;
  }
}

void IoTConnectClass::enterPortalMode() {
  Serial.printf("[IOT] Portal: %s en 192.168.4.1\n", _apName);
  startPortal();
}

void IoTConnectClass::startWifiConnect() {
  _state = IoTState::WIFI_CONNECTING;
  _wifiRetryCount = 0;
  _wifiStartTime = millis();

  if (strlen(g_cfg.ssid) == 0) {
    Serial.println("[NET] SSID vacío, abriendo portal");
    g_cfg.confirmed = false;
    _state = IoTState::PORTAL;
    enterPortalMode();
    return;
  }

  Serial.printf("[NET] Conectando WiFi: %s (intento 1/%d)\n",
                g_cfg.ssid, WIFI_MAX_RETRIES);
  WiFi.mode(WIFI_STA);
  WiFi.begin(g_cfg.ssid, g_cfg.pass);
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
          if (_mqttFailCount >= MQTT_MAX_RETRIES) {
            Serial.printf("[MQTT] %d fallos en operación, abriendo portal\n",
                          MQTT_MAX_RETRIES);
            // NO borrar credenciales
            g_cfg.confirmed = false;
            _normalOperation = false;
            _state = IoTState::PORTAL;
            enterPortalMode();
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
  return _state == IoTState::READY && isWifiConnected() && isMqttConnected();
}

bool IoTConnectClass::isConfigMode() {
  return _state == IoTState::PORTAL;
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
  _state = IoTState::PORTAL;
  startPortal();
}
