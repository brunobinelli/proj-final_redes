#include <WiFi.h>
#include <esp_now.h>
#include <WebServer.h>
#include <DNSServer.h> 

// --- PIN DEFINITIONS ---
#define PIN_TRIG 26
#define PIN_ECHO 25
#define PIN_BUZZER 14
#define PIN_RELAY 27

// --- CONSTANTS ---
const int DISTANCE_THRESHOLD = 30; // cm
const unsigned long COUNTDOWN_TIME = 15000; // 15 seconds
const byte DNS_PORT = 53; // DNS Port

// --- GLOBAL VARIABLES ---
bool alarmArmed = false;
bool doorOpen = false;
bool countdownActive = false;
bool buzzerActive = false;
unsigned long countdownStartTime = 0;
String lastReceivedHistory = "Waiting for data... Refresh page.";

// Structure for communication
typedef struct struct_message {
  int msgType; // 0 = Log, 1 = Request History, 2 = History Response
  char payload[200];
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// Web Server on port 80
WebServer server(80);
DNSServer dnsServer; 

// Broadcast Address (Sends to everyone)
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// --- HELPER FUNCTIONS ---

String getSystemTime() {
  unsigned long totalSeconds = millis() / 1000;
  int minutes = (totalSeconds / 60) % 60;
  int hours = (totalSeconds / 3600);
  int seconds = totalSeconds % 60;
  char timeString[20];
  sprintf(timeString, "%02d:%02d:%02d (Uptime)", hours, minutes, seconds);
  return String(timeString);
}

void sendLog(String text) {
  myData.msgType = 0; // Log
  String timeStr = getSystemTime();
  String fullLog = "[" + timeStr + "] " + text;
  
  fullLog.toCharArray(myData.payload, 200);
  
  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  Serial.println("Log sent: " + fullLog);
}

void requestHistory() {
  myData.msgType = 1; // Request
  strcpy(myData.payload, "GET");
  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
}

// --- CALLBACK MODIFIED FOR ESP32 BOARD V3.0+ ---
void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *incomingData, int len) {
  struct_message *incomingMsg = (struct_message *) incomingData;
  if (incomingMsg->msgType == 2) {
    lastReceivedHistory = String(incomingMsg->payload);
  }
}

// Ultrasonic Reader
float readDistance() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  
  long duration = pulseIn(PIN_ECHO, HIGH);
  float distance = duration * 0.034 / 2;
  return distance;
}

// --- WEB SERVER HANDLERS ---

String getHTML() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  
  // --- BEGIN CSS ---
  html += "<style>";
  html += ":root { --bg-gradient-start: #020617; --bg-gradient-end: #0f172a; --card-bg: rgba(15, 23, 42, 0.95); --text-main: #e5e7eb; --text-muted: #9ca3af; --accent: #38bdf8; --border-soft: rgba(148, 163, 184, 0.3); }";
  html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
  html += "body { font-family: Arial, sans-serif; min-height: 100vh; display: flex; align-items: center; justify-content: center; background: radial-gradient(circle at top, #1e293b 0, transparent 55%), linear-gradient(135deg, var(--bg-gradient-start), var(--bg-gradient-end)); color: var(--text-main); }";
  html += ".dashboard-wrapper { width: 100%; max-width: 420px; padding: 16px; }";
  html += ".card { background: var(--card-bg); border-radius: 18px; border: 1px solid var(--border-soft); box-shadow: 0 18px 45px rgba(15, 23, 42, 0.9), 0 0 0 1px rgba(15, 23, 42, 0.9); padding: 24px 20px 20px; }";
  html += ".card-header { display: flex; flex-direction: column; gap: 4px; margin-bottom: 18px; }";
  html += ".card-title { font-size: 22px; font-weight: 600; display: flex; align-items: center; gap: 8px; }";
  html += ".card-subtitle { font-size: 13px; color: var(--text-muted); }";
  html += ".status-panel { background: rgba(15, 23, 42, 0.9); border-radius: 14px; border: 1px solid rgba(75, 85, 99, 0.6); padding: 12px 14px; margin-bottom: 18px; }";
  html += ".status-row { display: flex; align-items: center; justify-content: space-between; margin-bottom: 8px; font-size: 14px; }";
  html += ".status-row:last-child { margin-bottom: 0; }";
  html += ".status-label { color: var(--text-muted); }";
  html += ".status-badge { display: inline-flex; align-items: center; justify-content: center; padding: 4px 10px; border-radius: 999px; font-size: 11px; font-weight: 600; letter-spacing: 0.06em; text-transform: uppercase; }";
  
  html += ".status-armed { background: rgba(34, 197, 94, 0.15); color: #4ade80; border: 1px solid rgba(74, 222, 128, 0.6); }";
  html += ".status-disarmed { background: rgba(239, 68, 68, 0.15); color: #fca5a5; border: 1px solid rgba(248, 113, 113, 0.6); }";
  html += ".status-door-closed { background: rgba(59, 130, 246, 0.12); color: #93c5fd; border: 1px solid rgba(96, 165, 250, 0.65); }";
  html += ".status-door-open { background: rgba(234, 179, 8, 0.14); color: #facc15; border: 1px solid rgba(250, 204, 21, 0.7); }";
  
  html += ".actions { display: flex; flex-direction: column; gap: 10px; margin-top: 8px; }";
  html += "a { text-decoration: none; }";
  html += "button { width: 100%; padding: 14px 12px; font-size: 16px; font-weight: 600; cursor: pointer; border: none; border-radius: 999px; text-transform: uppercase; letter-spacing: 0.06em; transition: transform 0.12s ease-out, filter 0.18s ease-out; box-shadow: 0 10px 25px rgba(15, 23, 42, 0.6); color: #f9fafb; background: linear-gradient(135deg, #2563eb, #38bdf8); }";
  html += "button:active { transform: translateY(1px); }";
  html += ".on { background: linear-gradient(135deg, #16a34a, #22c55e); }"; // Verde
  html += ".off { background: linear-gradient(135deg, #b91c1c, #ef4444); }"; // Vermelho
  html += ".warn { background: linear-gradient(135deg, #d97706, #f97316); color: #111827; }"; // Laranja
  
  html += "@media (max-width: 480px) { .card { padding: 20px 16px 18px; } .card-title { font-size: 20px; } button { font-size: 14px; padding: 12px 10px; } }";
  html += "</style></head>";
  // --- END DO CSS ---

  html += "<body>";
  html += "<div class='dashboard-wrapper'><div class='card'>";
  
  // Header
  html += "<div class='card-header'>";
  html += "<h1 class='card-title'><span class='icon'>🏠</span> Home Alarm Control</h1>";
  html += "<p class='card-subtitle'>Monitoramento residencial em tempo real</p>";
  html += "</div>";

  if(buzzerActive) {
      html += "<div style='background:rgba(239,68,68,0.2); border:1px solid #ef4444; padding:10px; border-radius:8px; margin-bottom:15px; text-align:center;'>";
      html += "<h2 style='color:#ef4444; font-size:18px; margin:0;'>⚠️ ALARM TRIGGERED!</h2></div>";
  }
  if(countdownActive) {
      html += "<div style='background:rgba(249,115,22,0.2); border:1px solid #f97316; padding:10px; border-radius:8px; margin-bottom:15px; text-align:center;'>";
      html += "<h2 style='color:#f97316; font-size:18px; margin:0;'>COUNTDOWN: " + String((COUNTDOWN_TIME - (millis() - countdownStartTime))/1000) + "s</h2></div>";
  }

  // --- STATUS PANEL ---
  html += "<div class='status-panel'>";
  
  
  html += "<div class='status-row'><span class='status-label'>Status geral</span>";
  String statusClass = alarmArmed ? "status-armed" : "status-disarmed";
  String statusText = alarmArmed ? "ARMED" : "DISARMED";
  html += "<span class='status-badge " + statusClass + "'><b>" + statusText + "</b></span></div>";

  html += "<div class='status-row'><span class='status-label'>Porta principal</span>";
  String doorClass = doorOpen ? "status-door-open" : "status-door-closed";
  String doorText = doorOpen ? "OPEN" : "CLOSED";
  html += "<span class='status-badge " + doorClass + "'><b>" + doorText + "</b></span></div>";
  
  html += "</div>"; 

  // --- ACTIONS (Botões) ---
  html += "<div class='actions'>";

  // Botão 1: Arm/Disarm
  if (alarmArmed) {
    html += "<a href='/disarm'><button class='off'>DISARM ALARM</button></a>";
  } else {
    html += "<a href='/arm'><button class='on'>ARM ALARM</button></a>";
  }

  // Botão 2: Open/Close Door
  if (doorOpen) {
    html += "<a href='/close'><button>CLOSE DOOR</button></a>";
  } else {
    html += "<a href='/open'><button>OPEN DOOR</button></a>";
  }

  // Botão 3: History
  html += "<a href='/history'><button class='warn'>VIEW HISTORY</button></a>";

  html += "</div>"; // Fecha actions
  html += "</div></div>"; // Fecha card e dashboard-wrapper
  html += "</body></html>";
  
  return html;
}

void handleRoot() {
  server.send(200, "text/html", getHTML());
}

void handleArm() {
  alarmArmed = true;
  buzzerActive = false;
  countdownActive = false;
  sendLog("User ARMED the system via Web");
  
  // Feedback Sonoro: Bip Longo (1s)
  digitalWrite(PIN_BUZZER, HIGH);
  delay(1000); 
  digitalWrite(PIN_BUZZER, LOW);
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleDisarm() {
  alarmArmed = false;
  buzzerActive = false;
  countdownActive = false;
  
  // Feedback Sonoro: 3 Bips Rápidos
  for(int i=0; i<3; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(100);
    digitalWrite(PIN_BUZZER, LOW);
    delay(100);
  }
  
  sendLog("User DISARMED the system via Web");
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleOpen() {
  doorOpen = true;
  digitalWrite(PIN_RELAY, HIGH); 
  
  sendLog("Door Opened");
  
  if (alarmArmed && !countdownActive && !buzzerActive) {
    countdownActive = true;
    countdownStartTime = millis();
    sendLog("WARNING: Countdown Started (15s)");
  }
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleClose() {
  doorOpen = false;
  digitalWrite(PIN_RELAY, LOW); 
  sendLog("Door Closed");
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleHistory() {
  requestHistory(); 
  delay(200); 
  
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";

  // --- BEGIN CSS  ---
  html += "<style>";
  html += ":root { --bg-gradient-start: #020617; --bg-gradient-end: #0f172a; --card-bg: rgba(15, 23, 42, 0.95); --text-main: #e5e7eb; --text-muted: #9ca3af; --accent: #38bdf8; --border-soft: rgba(148, 163, 184, 0.3); --log-bg: #020617; --log-border: #1f2937; --log-scrollbar: #4b5563; }";
  html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
  html += "body { font-family: Arial, sans-serif; min-height: 100vh; display: flex; align-items: center; justify-content: center; background: radial-gradient(circle at top, #1e293b 0, transparent 55%), linear-gradient(135deg, var(--bg-gradient-start), var(--bg-gradient-end)); color: var(--text-main); }";
  html += ".dashboard-wrapper { width: 100%; max-width: 720px; padding: 16px; }";
  html += ".card { background: var(--card-bg); border-radius: 18px; border: 1px solid var(--border-soft); box-shadow: 0 18px 45px rgba(15, 23, 42, 0.9), 0 0 0 1px rgba(15, 23, 42, 0.9); padding: 22px 20px 18px; }";
  
  // Header Styles
  html += ".card-header { display: flex; flex-direction: column; gap: 4px; margin-bottom: 14px; }";
  html += ".card-title { font-size: 22px; font-weight: 600; display: flex; align-items: center; gap: 8px; }";
  html += ".card-subtitle { font-size: 13px; color: var(--text-muted); }";
  
  // Logs Meta
  html += ".logs-meta { display: flex; justify-content: space-between; align-items: center; font-size: 12px; color: var(--text-muted); margin-bottom: 8px; }";
  html += ".logs-meta span.badge { padding: 3px 9px; border-radius: 999px; border: 1px solid rgba(56, 189, 248, 0.6); color: #a5f3fc; background: rgba(8, 47, 73, 0.6); text-transform: uppercase; letter-spacing: 0.08em; font-size: 10px; font-weight: 600; }";
  
  // Console/Logs Container
  html += ".logs-container { background: radial-gradient(circle at top left, #0b1120 0, #020617 55%); border-radius: 14px; border: 1px solid var(--log-border); padding: 10px 10px 8px; position: relative; }";
  html += ".logs-toolbar { display: flex; gap: 6px; margin-bottom: 6px; }";
  html += ".logs-dot { width: 9px; height: 9px; border-radius: 999px; background: #ef4444; opacity: 0.9; }";
  html += ".logs-dot:nth-child(2) { background: #facc15; } .logs-dot:nth-child(3) { background: #22c55e; }";
  html += ".logs-label { position: absolute; top: 8px; right: 10px; font-size: 10px; letter-spacing: 0.12em; text-transform: uppercase; color: rgba(148, 163, 184, 0.75); }";
  
  // Textarea Styles
  html += "textarea { font-size: 13px; line-height: 1.45; width: 100%; height: 55vh; border-radius: 10px; border: 1px solid rgba(55, 65, 81, 0.9); padding: 10px 12px; font-family: 'JetBrains Mono', 'Fira Code', monospace; resize: none; background-color: var(--log-bg); color: #e5e7eb; box-shadow: inset 0 0 0 1px rgba(15, 23, 42, 0.9); outline: none; text-align: left; white-space: pre; overflow: auto; }";
  html += "textarea::selection { background: rgba(56, 189, 248, 0.35); }";
  
  // Buttons
  html += ".actions { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 14px; justify-content: flex-end; }";
  html += "a { text-decoration: none; }";
  html += "button { padding: 9px 16px; min-width: 110px; font-size: 13px; font-weight: 600; cursor: pointer; border: none; border-radius: 999px; text-transform: uppercase; letter-spacing: 0.08em; transition: all 0.12s ease-out; box-shadow: 0 10px 25px rgba(15, 23, 42, 0.6); color: #f9fafb; background: linear-gradient(135deg, #4b5563, #6b7280); }";
  html += "button:hover { transform: translateY(-1px); filter: brightness(1.05); box-shadow: 0 16px 38px rgba(15, 23, 42, 0.85); }";
  html += "button:active { transform: translateY(1px); }";
  html += "a[href='/history'] button { background: linear-gradient(135deg, #2563eb, #38bdf8); }"; // Destaque para o Refresh
  
  html += "@media (max-width: 600px) { .card { padding: 20px 14px 16px; } textarea { height: 50vh; font-size: 12px; } .actions { justify-content: center; } button { width: 100%; } }";
  html += "</style></head>";
  // --- END CSS ---

  html += "<body>";
  html += "<div class='dashboard-wrapper'><div class='card'>";
  
  // Header
  html += "<div class='card-header'>";
  html += "<h1 class='card-title'><span class='icon'>📡</span> System Logs</h1>";
  html += "<p class='card-subtitle'>Últimos eventos do sistema (mais recentes no topo)</p>";
  html += "</div>";

  // Meta info
  html += "<div class='logs-meta'><span>Last events (Newest first)</span><span class='badge'>Live feed</span></div>";

  // Container dos Logs (Fake terminal)
  html += "<div class='logs-container'>";
  html += "<div class='logs-toolbar'><span class='logs-dot'></span><span class='logs-dot'></span><span class='logs-dot'></span></div>";
  html += "<span class='logs-label'>MONITOR</span>";
  
  // --- TEXTAREA COM DADOS REAIS ---
  html += "<textarea readonly>" + lastReceivedHistory + "</textarea>";
  html += "</div>";

  // Botões de Ação
  html += "<div class='actions'>";
  html += "<a href='/'><button>Back</button></a>";
  html += "<a href='/history'><button>Refresh Data</button></a>";
  html += "</div>";

  html += "</div></div></body></html>";
  
  server.send(200, "text/html", html);
}

// --- SETUP & LOOP ---

void setup() {
  Serial.begin(115200);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  
  digitalWrite(PIN_RELAY, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Alarm_System", "12345678");
  
  Serial.print("AP Started. IP: ");
  Serial.println(WiFi.softAPIP());

  // Setup DNS Server
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
  
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  server.on("/", handleRoot);
  server.on("/arm", handleArm);
  server.on("/disarm", handleDisarm);
  server.on("/open", handleOpen);
  server.on("/close", handleClose);
  server.on("/history", handleHistory);
  
  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest(); 
  server.handleClient();

  static unsigned long lastSensorRead = 0;
  if (millis() - lastSensorRead > 500) { 
    float dist = readDistance();
    if (dist > 0 && dist < DISTANCE_THRESHOLD) {
      Serial.println("Motion Detected!");
      static unsigned long lastMotionLog = 0;
      if (millis() - lastMotionLog > 5000) {
        sendLog("Motion Detected by Sensor");
        lastMotionLog = millis();
      }
    }
    lastSensorRead = millis();
  }

  if (countdownActive) {
    if (millis() - countdownStartTime > COUNTDOWN_TIME) {
      countdownActive = false;
      buzzerActive = true;
      digitalWrite(PIN_BUZZER, HIGH);
      sendLog("ALARM TRIGGERED: Timeout reached!");
    }
  }

  if (!alarmArmed) {
    digitalWrite(PIN_BUZZER, LOW);
    buzzerActive = false;
    countdownActive = false;
  }
}