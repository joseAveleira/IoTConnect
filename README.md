# 📡 IoTConnect

Librería Arduino para ESP32 que simplifica la conexión con el servidor **[joseaveleira.es/IoT](https://joseaveleira.es/IoT)**.

> Solo configura el nombre de tu app y gestiona tus mensajes. El resto es automático.

---

## ✨ Características

- 🌐 **Portal cautivo** automático para configuración WiFi
- 🔐 **Autenticación MQTT** con tu servidor IoT
- 💾 **Persistencia NVS** - recuerda la configuración tras reinicio
- 🔄 **Reconexión automática** WiFi y MQTT
- 📱 **Interfaz web responsive** para configurar desde móvil/PC
- 🎯 **API minimalista** - solo lo esencial

---

## 🔗 Requisitos

1. Cuenta en **[joseaveleira.es/IoT](https://joseaveleira.es/IoT)**
2. Dispositivo dado de alta en el panel
3. ESP32 con esta librería

---

## 📱 Tutorial: Vincular Dispositivo

### Paso 1: Da de alta tu dispositivo en el servidor

1. Entra en [joseaveleira.es/IoT](https://joseaveleira.es/IoT)
2. Inicia sesión o crea una cuenta
3. Añade un nuevo dispositivo
4. Se generará un **QR de configuración** con el `token` y `publicId` del dispositivo

![Dashboard IoT - Configuración de dispositivo](docs/tutorial-qr.png)

### Paso 2: Configura tu ESP32

```
┌─────────────────────────────────────────────────────────┐
│                                                         │
│  1️⃣  Enciende el dispositivo                            │
│      └─> Aparecerá una red WiFi (ej: "MiApp-Setup")     │
│                                                         │
│  2️⃣  Conéctate a esa red WiFi desde tu móvil            │
│      └─> Sin internet, es normal                        │
│      └─> Mantén la conexión si pregunta                 │
│                                                         │
│  3️⃣  Escanea el QR del panel web                        │
│      └─> Se abre el portal cautivo automáticamente      │
│      └─> El token y publicId se rellenan solos ✨       │
│                                                         │
│  4️⃣  Solo introduce tu WiFi y contraseña                │
│      └─> Pulsa "Conectar" y ¡listo!                     │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

> 💡 **¿Cómo funciona?** El QR contiene una URL con el `token` y `publicId` de tu dispositivo. Al escanearlo mientras estás conectado al portal cautivo del ESP32, estos datos se envían automáticamente al formulario. ¡Solo necesitas añadir los datos de tu WiFi!

### Paso 3: ¡Ya está conectado!

El dispositivo guardará la configuración y se conectará automáticamente cada vez que encienda.

---

## 🚀 Instalación

### Opción 1: Carpeta lib (local)
Copia la carpeta `IoTConnect` a la carpeta `lib/` de tu proyecto.

### Opción 2: GitHub (recomendado)
En tu `platformio.ini`:
```ini
lib_deps = 
    https://github.com/joseAveleira/IoTConnect.git
```

---

## 📖 Uso Básico

```cpp
#include <Arduino.h>
#include <IoTConnect.h>

#define AP_NAME   "MiApp-Setup"   // Nombre del WiFi en modo config
#define APP_NAME  "MiApp"          // Nombre de tu aplicación

void onMessage(const char* topic, const char* payload) {
  Serial.printf("Mensaje: %s -> %s\n", topic, payload);
  // Tu lógica aquí
}

void onConnectionChange(bool connected) {
  if (connected) {
    // Suscríbete a tus topics
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/commands", IoTConnect.getPublicId());
    IoTConnect.subscribe(topic);
  }
}

void setup() {
  IoTConnect.begin(AP_NAME, APP_NAME);
  IoTConnect.onMessage(onMessage);
  IoTConnect.onConnectionChange(onConnectionChange);
}

void loop() {
  IoTConnect.loop();
  
  // Tu código aquí
  if (IoTConnect.isReady()) {
    // Publicar cuando esté conectado
    // IoTConnect.publish("mi/topic", "payload");
  }
}
```

---

## 📚 API Reference

### Configuración

| Método | Descripción |
|--------|-------------|
| `begin(apName, appName)` | Inicializa la librería |
| `loop()` | Llamar en cada iteración |

### Estado

| Método | Descripción |
|--------|-------------|
| `isReady()` | `true` si WiFi + MQTT conectados |
| `isConfigMode()` | `true` si está en portal cautivo |
| `getClientId()` | Devuelve el Client ID configurado |
| `getPublicId()` | Devuelve el Public ID configurado |

### MQTT

| Método | Descripción |
|--------|-------------|
| `publish(topic, payload, retained)` | Publica mensaje |
| `subscribe(topic)` | Suscribe a topic |
| `onMessage(callback)` | Callback para mensajes entrantes |
| `onConnectionChange(callback)` | Callback conexión/desconexión |

### Utilidades

| Método | Descripción |
|--------|-------------|
| `resetConfig()` | Borra config y vuelve al portal |

---

## 🔧 Flujo de Configuración

```
┌─────────────────────────────────────────────────────────┐
│  1. ESP32 arranca sin config                            │
│     └─> Crea WiFi "MiApp-Setup"                         │
│                                                         │
│  2. Usuario conecta al WiFi desde móvil/PC              │
│     └─> Se abre portal cautivo automáticamente          │
│                                                         │
│  3. Usuario introduce:                                  │
│     • Client ID, Token, Public ID (desde tu backend)    │
│     • WiFi y contraseña                                 │
│                                                         │
│  4. ESP32 conecta a WiFi → MQTT → ¡Listo!               │
│     └─> Config guardada en NVS (persiste reinicios)     │
└─────────────────────────────────────────────────────────┘
```

---

## 📋 Dependencias

Se instalan automáticamente:
- `PubSubClient` ^2.8
- `ArduinoJson` ^6.21

---

## ⚙️ Configuración MQTT

Por defecto conecta a `joseaveleira.es:1883`. Para cambiar el servidor, edita [`Config.h`](lib/IoTConnect/src/Config.h):

```cpp
constexpr const char* MQTT_HOST = "tu-servidor.com";
constexpr uint16_t    MQTT_PORT = 1883;
```

---

## 🖥️ Añadir Pantalla (Opcional)

La librería no incluye soporte de pantalla por defecto. Usa los callbacks para integrar tu display:

```cpp
void onConnectionChange(bool connected) {
  if (connected) {
    miPantalla.mostrar("Conectado!");
  } else {
    miPantalla.mostrar("Sin conexión");
  }
}
```

---

## 📝 Licencia

MIT © Jose Aveleira

---

## 🌐 Servidor IoT

Esta librería está diseñada para funcionar con:

**[https://joseaveleira.es/IoT](https://joseaveleira.es/IoT)**

---

<p align="center">
  <b>¿Te ha sido útil?</b> ⭐ Dale una estrella en GitHub
</p>
