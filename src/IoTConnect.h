#pragma once
#include <Arduino.h>
#include <functional>

// =============================================================================
// IoTConnect - Librería para conexión IoT simplificada
// =============================================================================

// Callback para mensajes MQTT recibidos
using MqttMessageCallback = std::function<void(const char* topic, const char* payload)>;

// Callback para eventos de conexión/desconexión
using ConnectionCallback = std::function<void(bool connected)>;

// Estados visibles de la conexión
enum class IoTState {
  PORTAL,           // Portal cautivo activo
  WIFI_CONNECTING,  // Intentando conectar WiFi
  MQTT_CONNECTING,  // Intentando conectar MQTT
  STABILIZING,      // Estabilizando conexión
  READY             // Conectado y operativo
};

class IoTConnectClass {
public:
  // Configuración inicial (no bloqueante)
  void begin(const char* apName, const char* appName);

  // Loop principal - llamar en cada iteración (maneja la máquina de estados)
  void loop();

  // ¿Está listo para pub/sub? (WiFi + MQTT conectados)
  bool isReady();

  // ¿Está en modo configuración (portal cautivo)?
  bool isConfigMode();

  // Estado actual de la conexión (para mostrar en pantalla)
  IoTState getState() const { return _state; }
  int getWifiRetry() const { return _wifiRetryCount + 1; }
  int getWifiMaxRetries() const { return WIFI_MAX_RETRIES; }

  // Publicar mensaje MQTT
  bool publish(const char* topic, const char* payload, bool retained = false);

  // Suscribirse a topic
  bool subscribe(const char* topic);

  // Callback cuando llega un mensaje MQTT
  void onMessage(MqttMessageCallback callback);

  // Callback cuando cambia estado de conexión
  void onConnectionChange(ConnectionCallback callback);

  // Obtener datos de configuración (para construir topics)
  const char* getClientId();
  const char* getPublicId();

  // Forzar reset de configuración y volver al portal
  void resetConfig();

private:
  MqttMessageCallback _messageCallback = nullptr;
  ConnectionCallback _connectionCallback = nullptr;
  const char* _apName = "IoT-Setup";
  const char* _appName = "IoT Connect";
  IoTState _state = IoTState::PORTAL;
  bool _wasConnected = false;
  bool _justConfigured = false;
  bool _mqttBeginDone = false;
  int _mqttFailCount = 0;
  int _wifiRetryCount = 0;
  unsigned long _lastMqttRetry = 0;
  unsigned long _wifiStartTime = 0;
  unsigned long _stabilizeStart = 0;
  bool _normalOperation = false;
  bool _initialized = false;

  static const int WIFI_MAX_RETRIES = 3;
  static const int MQTT_MAX_RETRIES = 4;
  static const uint32_t WIFI_TIMEOUT_MS = 15000;
  static const uint32_t STABILIZE_MS = 2000;

  void enterPortalMode();
  void startWifiConnect();
  void handleNormalOperation();
  void notifyConnectionChange(bool connected);
};

// Instancia global singleton
extern IoTConnectClass IoTConnect;
