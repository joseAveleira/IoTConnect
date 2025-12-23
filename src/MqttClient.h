#pragma once
#include <Arduino.h>
#include <functional>
#include "Config.h"

// Callback para mensajes MQTT
using InternalMqttCallback = std::function<void(const char* topic, const char* payload)>;

// Funciones del cliente MQTT
void mqttBegin();
bool mqttConnect(const AppConfig& cfg);
void mqttLoop();
void mqttDisconnect();
bool publishOkSync(const AppConfig& cfg);

// Funciones para IoTConnect
bool mqttPublish(const char* topic, const char* payload, bool retained = false);
bool mqttSubscribe(const char* topic);
void setMqttMessageCallback(InternalMqttCallback callback);

// Estado del cliente MQTT
bool isMqttConnected();
bool isMqttStable();
int getMqttFailCount();
