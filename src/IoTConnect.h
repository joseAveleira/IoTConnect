#pragma once
#include <Arduino.h>
#include <functional>

// =============================================================================
// IoTConnect - Librería para conexión IoT simplificada
// =============================================================================
// Uso básico:
//
//   #include <IoTConnect.h>
//   
//   void onMessage(const char* topic, const char* payload) { ... }
//   
//   void setup() {
//     IoTConnect.begin("MiApp-Setup", "MiApp");
//     IoTConnect.onMessage(onMessage);
//   }
//   
//   void loop() {
//     IoTConnect.loop();
//     if (IoTConnect.isReady()) {
//       IoTConnect.publish("topic", "payload");
//     }
//   }
// =============================================================================

// Callback para mensajes MQTT recibidos
using MqttMessageCallback = std::function<void(const char* topic, const char* payload)>;

// Callback para eventos de conexión/desconexión
using ConnectionCallback = std::function<void(bool connected)>;

class IoTConnectClass {
public:
  // Configuración inicial
  // apName: nombre de la red WiFi del portal cautivo (ej: "MiApp-Setup")
  // appName: nombre de la aplicación mostrado en el portal (ej: "MiApp")
  void begin(const char* apName, const char* appName);
  
  // Loop principal - llamar en cada iteración
  void loop();
  
  // ¿Está listo para pub/sub? (WiFi + MQTT conectados)
  bool isReady();
  
  // ¿Está en modo configuración (portal cautivo)?
  bool isConfigMode();
  
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
  bool _wasConnected = false;
  int _mqttFailCount = 0;
  unsigned long _lastMqttRetry = 0;
  bool _normalOperation = false;
  bool _initialized = false;
  
  void enterPortalMode();
  void handlePortalLoop();
  void handleNormalOperation();
  void notifyConnectionChange(bool connected);
};

// Instancia global singleton
extern IoTConnectClass IoTConnect;
