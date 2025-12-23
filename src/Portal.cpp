#include "Portal.h"
#include "Config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <cstring>

static WebServer server(80);
static DNSServer dnsServer;
static bool portalActive = false;
static unsigned long lastScanTime = 0;
static String scanResults = "";

// HTML del portal (PROGMEM para ahorrar RAM)
const char HTML_PORTAL[] PROGMEM = R"html(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>{{APP_NAME}} Setup</title>
    <style>
        * { box-sizing: border-box; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; margin: 0; padding: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }
        .container { max-width: 480px; margin: 0 auto; background: white; padding: 30px; border-radius: 16px; box-shadow: 0 10px 40px rgba(0,0,0,0.2); }
        h1 { color: #333; text-align: center; margin: 0 0 10px 0; font-size: 24px; }
        .subtitle { text-align: center; color: #666; margin-bottom: 25px; font-size: 14px; }
        .section { background: #f8f9fa; border-radius: 12px; padding: 20px; margin-bottom: 20px; }
        .section-title { font-size: 14px; font-weight: 600; color: #667eea; margin-bottom: 15px; text-transform: uppercase; letter-spacing: 0.5px; }
        .form-group { margin-bottom: 15px; }
        .form-group:last-child { margin-bottom: 0; }
        label { display: block; margin-bottom: 6px; font-weight: 500; color: #444; font-size: 14px; }
        input, select { width: 100%; padding: 12px; border: 2px solid #e1e5e9; border-radius: 8px; font-size: 15px; transition: border-color 0.2s; }
        input:focus, select:focus { outline: none; border-color: #667eea; }
        .input-password { position: relative; }
        .input-password input { padding-right: 45px; }
        .toggle-pass { position: absolute; right: 12px; top: 50%; transform: translateY(-50%); background: none; border: none; cursor: pointer; padding: 5px; }
        .eye { width: 20px; height: 12px; border: 2px solid #888; border-radius: 70% 15%; position: relative; transform: rotate(0deg); }
        .eye::before { content: ''; position: absolute; width: 6px; height: 6px; background: #888; border-radius: 50%; top: 50%; left: 50%; transform: translate(-50%, -50%); }
        .eye.visible { border-color: #667eea; }
        .eye.visible::before { background: #667eea; }
        .eye.hidden-pass::after { content: ''; position: absolute; width: 24px; height: 2px; background: #888; top: 50%; left: -2px; transform: translateY(-50%) rotate(-45deg); }
        .toggle-pass:hover .eye { border-color: #667eea; }
        .toggle-pass:hover .eye::before { background: #667eea; }
        .btn-container { display: flex; flex-direction: column; gap: 12px; margin-top: 25px; }
        .btn { width: 100%; padding: 14px; border: none; border-radius: 10px; font-size: 16px; font-weight: 600; cursor: pointer; transition: transform 0.1s, box-shadow 0.2s; }
        .btn:active { transform: scale(0.98); }
        .btn-primary { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; box-shadow: 0 4px 15px rgba(102, 126, 234, 0.4); }
        .btn-primary:hover { box-shadow: 0 6px 20px rgba(102, 126, 234, 0.5); }
        .btn-secondary { background: #fff; color: #666; border: 2px solid #ddd; }
        .btn-secondary:hover { border-color: #999; color: #333; }
        .status { padding: 12px; margin-top: 15px; border-radius: 8px; background: #c6f6d5; color: #276749; font-size: 14px; text-align: center; }
        .hidden { display: none; }
        .hint { font-size: 12px; color: #888; margin-top: 4px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>{{APP_NAME}}</h1>
        <p class="subtitle">Configuracion del dispositivo IoT</p>
        
        <form method="POST" action="/save">
            <div class="section">
                <div class="section-title">Credenciales IoT</div>
                <div class="form-group">
                    <label for="clientid">Client ID</label>
                    <input type="text" id="clientid" name="clientid" value="{{CLIENT_ID}}" placeholder="Ej: device_abc123" required>
                </div>
                
                <div class="form-group">
                    <label for="token">Token</label>
                    <div class="input-password">
                        <input type="password" id="token" name="token" value="{{TOKEN}}" placeholder="Token de autenticacion" required>
                        <button type="button" class="toggle-pass" onclick="togglePassword('token', this)"><div class="eye hidden-pass"></div></button>
                    </div>
                </div>
                
                <div class="form-group">
                    <label for="publicid">Public ID</label>
                    <input type="text" id="publicid" name="publicid" value="{{PUBLIC_ID}}" placeholder="Identificador publico" required>
                </div>
            </div>
            
            <div class="section">
                <div class="section-title">Conexion WiFi</div>
                <div class="form-group">
                    <label for="ssid_select">Redes disponibles</label>
                    <select id="ssid_select">
                        <option value="">Cargando redes...</option>
                    </select>
                    <p class="hint">Selecciona una red o escribe el nombre manualmente</p>
                </div>
                
                <div class="form-group">
                    <label for="ssid">Nombre de red (SSID)</label>
                    <input type="text" id="ssid" name="ssid" value="{{SSID}}" placeholder="Nombre de tu red WiFi" required>
                </div>
                
                <div class="form-group">
                    <label for="pass">Contrasena WiFi</label>
                    <div class="input-password">
                        <input type="password" id="pass" name="pass" value="{{PASS}}" placeholder="Contrasena de la red">
                        <button type="button" class="toggle-pass" onclick="togglePassword('pass', this)"><div class="eye hidden-pass"></div></button>
                    </div>
                </div>
            </div>
            
            <div class="btn-container">
                <button type="submit" class="btn btn-primary">Guardar y Conectar</button>
                <button type="button" class="btn btn-secondary" onclick="resetConfig()">Borrar configuracion y reiniciar</button>
            </div>
        </form>
        
        <div id="status" class="status hidden"></div>
    </div>

    <script>
        function togglePassword(inputId, btn) {
            const input = document.getElementById(inputId);
            const eye = btn.querySelector('.eye');
            if (input.type === 'password') {
                input.type = 'text';
                eye.classList.remove('hidden-pass');
                eye.classList.add('visible');
            } else {
                input.type = 'password';
                eye.classList.add('hidden-pass');
                eye.classList.remove('visible');
            }
        }
        
        function getUrlParam(name) {
            const urlParams = new URLSearchParams(window.location.search);
            return urlParams.get(name) || urlParams.get(name.toLowerCase());
        }
        
        window.onload = function() {
            const clientId = getUrlParam('clientId') || getUrlParam('clientid');
            const publicId = getUrlParam('publicId') || getUrlParam('publicid');
            const token = getUrlParam('token');
            
            if (clientId && !document.getElementById('clientid').value) {
                document.getElementById('clientid').value = clientId;
            }
            if (publicId && !document.getElementById('publicid').value) {
                document.getElementById('publicid').value = publicId;
            }
            if (token && !document.getElementById('token').value) {
                document.getElementById('token').value = token;
            }
            
            loadNetworks();
        };
        
        function getSignal(rssi) {
            if (rssi >= -50) return '●●●●';
            if (rssi >= -60) return '●●●○';
            if (rssi >= -70) return '●●○○';
            return '●○○○';
        }
        
        function loadNetworks() {
            fetch('/scan')
                .then(response => response.json())
                .then(data => {
                    const select = document.getElementById('ssid_select');
                    select.innerHTML = '<option value="">Seleccionar red...</option>';
                    
                    data.networks.forEach(network => {
                        const option = document.createElement('option');
                        option.value = network.ssid;
                        const signal = getSignal(network.rssi);
                        const lock = network.enc ? ' *' : '';
                        option.textContent = network.ssid + ' ' + signal + lock;
                        select.appendChild(option);
                    });
                })
                .catch(err => {
                    console.error('Error cargando redes:', err);
                    document.getElementById('ssid_select').innerHTML = '<option value="">Error cargando redes</option>';
                });
        }
        
        document.getElementById('ssid_select').onchange = function() {
            if (this.value) {
                document.getElementById('ssid').value = this.value;
            }
        };
        
        function resetConfig() {
            if (confirm('Se borrara la configuracion guardada y el dispositivo se reiniciara.\\n\\n¿Continuar?')) {
                fetch('/reset', { method: 'POST' })
                    .then(() => {
                        document.getElementById('status').textContent = 'Configuracion borrada. Reiniciando...';
                        document.getElementById('status').classList.remove('hidden');
                    });
            }
        }
    </script>
</body>
</html>
)html";

const char HTML_CONNECTING[] PROGMEM = R"html(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>{{APP_NAME}} - Conectando</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; text-align: center; }
        .container { max-width: 400px; margin: 50px auto; background: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .spinner { border: 4px solid #f3f3f3; border-top: 4px solid #007cba; border-radius: 50%; width: 40px; height: 40px; animation: spin 1s linear infinite; margin: 20px auto; }
        @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }
    </style>
</head>
<body>
    <div class="container">
        <h1>Conectando...</h1>
        <div class="spinner"></div>
        <p>Configuración guardada. Conectando a WiFi y MQTT...</p>
        <p>El dispositivo se reiniciará automáticamente.</p>
    </div>
    <script>
        setTimeout(() => { window.location.href = '/'; }, 10000);
    </script>
</body>
</html>
)html";

String replaceTemplate(const char* tmpl, const AppConfig& cfg) {
  String html = String(tmpl);
  html.replace("{{APP_NAME}}", g_appName);
  html.replace("{{CLIENT_ID}}", cfg.clientId);
  html.replace("{{TOKEN}}", cfg.token);
  html.replace("{{PUBLIC_ID}}", cfg.publicId);
  html.replace("{{SSID}}", cfg.ssid);
  html.replace("{{PASS}}", cfg.pass);
  return html;
}

void handleRoot() {
  bool hasQRData = false;
  
  if (server.hasArg("clientId") || server.hasArg("clientid")) {
    const char* clientId = server.hasArg("clientId") ? server.arg("clientId").c_str() : server.arg("clientid").c_str();
    strlcpy(g_cfg.clientId, clientId, sizeof(g_cfg.clientId));
    Serial.printf("[CFG] Client ID desde GET: %s\n", g_cfg.clientId);
    hasQRData = true;
  }
  
  if (server.hasArg("token")) {
    strlcpy(g_cfg.token, server.arg("token").c_str(), sizeof(g_cfg.token));
    Serial.printf("[CFG] Token desde GET: %s\n", g_cfg.token);
    hasQRData = true;
  }
  
  if (server.hasArg("publicId") || server.hasArg("publicid")) {
    const char* publicId = server.hasArg("publicId") ? server.arg("publicId").c_str() : server.arg("publicid").c_str();
    strlcpy(g_cfg.publicId, publicId, sizeof(g_cfg.publicId));
    Serial.printf("[CFG] Public ID desde GET: %s\n", g_cfg.publicId);
    hasQRData = true;
  }

  if (hasQRData) {
    Serial.println("[PORTAL] Cliente conectado con datos QR");
  }

  String html = replaceTemplate(HTML_PORTAL, g_cfg);
  server.send(200, "text/html", html);
}

void handleScan() {
  Serial.println("[NET] Petición /scan recibida");
  
  unsigned long now = millis();
  
  // Usar cache si el escaneo es reciente
  if (now - lastScanTime < 5000 && scanResults.length() > 2) {
    Serial.println("[NET] Usando cache de escaneo");
    server.send(200, "application/json", scanResults);
    return;
  }

  Serial.println("[NET] Escaneando redes WiFi...");
  
  // NO cambiar el modo WiFi aquí - ya está configurado en startPortal()
  // WiFi.mode(WIFI_AP_STA); // <-- REMOVIDO - causaba el problema
  
  int n = WiFi.scanNetworks(false, false, false, 300);  // Scan más rápido
  
  DynamicJsonDocument doc(4096);
  JsonArray networks = doc.createNestedArray("networks");
  
  Serial.printf("[NET] Encontradas %d redes\n", n);
  
  if (n > 0) {
    for (int i = 0; i < n && i < 20; i++) {  // Limitar a 20 redes
      JsonObject net = networks.createNestedObject();
      net["ssid"] = WiFi.SSID(i);
      net["rssi"] = WiFi.RSSI(i);
      net["enc"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
  }
  
  scanResults = "";
  serializeJson(doc, scanResults);
  lastScanTime = now;
  
  WiFi.scanDelete();  // Limpiar resultados del scan
  
  server.send(200, "application/json", scanResults);
  Serial.println("[NET] Respuesta /scan enviada");
}

void handleSave() {
  Serial.println("[CFG] Guardando configuración desde POST");
  
  if (server.hasArg("clientid")) {
    strlcpy(g_cfg.clientId, server.arg("clientid").c_str(), sizeof(g_cfg.clientId));
  }
  if (server.hasArg("token")) {
    strlcpy(g_cfg.token, server.arg("token").c_str(), sizeof(g_cfg.token));
  }
  if (server.hasArg("publicid")) {
    strlcpy(g_cfg.publicId, server.arg("publicid").c_str(), sizeof(g_cfg.publicId));
  }
  if (server.hasArg("ssid")) {
    strlcpy(g_cfg.ssid, server.arg("ssid").c_str(), sizeof(g_cfg.ssid));
  }
  if (server.hasArg("pass")) {
    strlcpy(g_cfg.pass, server.arg("pass").c_str(), sizeof(g_cfg.pass));
  }
  
  g_cfg.confirmed = true;
  
  if (saveConfig(g_cfg)) {
    server.send(200, "text/html", HTML_CONNECTING);
    Serial.println("[CFG] Configuración guardada, saliendo del portal");
  } else {
    server.send(500, "text/plain", "Error guardando configuración");
  }
}

void handleReset() {
  Serial.println("[CFG] Reset solicitado desde web");
  clearConfig();
  server.send(200, "text/plain", "Configuración reseteada. Reiniciando...");
  delay(1000);
  ESP.restart();
}

void handleCaptivePortal() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void startPortal() {
  if (portalActive) return;
  
  Serial.println("[NET] Iniciando portal cautivo...");
  
  // Configurar modo AP+STA ANTES de todo
  WiFi.mode(WIFI_AP_STA);
  delay(100);
  
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(g_apName, "");
  
  Serial.printf("[NET] AP iniciado: %s en 192.168.4.1\n", g_apName);
  
  // Hacer escaneo inicial de redes ANTES de iniciar el servidor
  Serial.println("[NET] Escaneo inicial de redes...");
  int n = WiFi.scanNetworks(false, false, false, 300);
  Serial.printf("[NET] Escaneo inicial: %d redes encontradas\n", n);
  
  // Pre-generar el JSON de redes
  DynamicJsonDocument doc(4096);
  JsonArray networks = doc.createNestedArray("networks");
  if (n > 0) {
    for (int i = 0; i < n && i < 20; i++) {
      JsonObject net = networks.createNestedObject();
      net["ssid"] = WiFi.SSID(i);
      net["rssi"] = WiFi.RSSI(i);
      net["enc"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
  }
  scanResults = "";
  serializeJson(doc, scanResults);
  lastScanTime = millis();
  WiFi.scanDelete();
  
  // Iniciar DNS Server
  dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
  
  // Registrar rutas del servidor web
  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/reset", HTTP_POST, handleReset);
  
  // Rutas para portales cautivos de diferentes sistemas
  server.on("/generate_204", HTTP_GET, handleCaptivePortal);
  server.on("/hotspot-detect.html", HTTP_GET, handleCaptivePortal);
  server.on("/favicon.ico", HTTP_GET, []() {
    server.send(204);  // No content para favicon
  });
  
  // Handler para rutas no encontradas
  server.onNotFound([]() {
    String uri = server.uri();
    Serial.printf("[NET] Request no encontrado: %s\n", uri.c_str());
    
    // Si es una petición API, devolver error JSON
    if (uri.startsWith("/api")) {
      server.send(404, "application/json", "{\"error\":\"not found\"}");
      return;
    }
    
    // Para cualquier otra cosa, redirigir al portal
    handleCaptivePortal();
  });
  
  server.begin();
  portalActive = true;
  
  Serial.println("[NET] Portal cautivo activo en http://192.168.4.1/");
}

void stopPortal() {
  if (!portalActive) return;
  
  Serial.println("[NET] Deteniendo portal cautivo...");
  
  server.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  
  portalActive = false;
  
  Serial.println("[NET] Portal cautivo detenido");
}

void portalLoop() {
  if (!portalActive) return;
  
  dnsServer.processNextRequest();
  server.handleClient();
}

bool isPortalActive() {
  return portalActive;
}
