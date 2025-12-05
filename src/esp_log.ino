#include <WiFi.h>
#include <esp_now.h>

String logHistory = "";
int logCount = 0;

// Structure for communication
typedef struct struct_message {
  int msgType; // 0 = Log, 1 = Request History, 2 = History Response
  char payload[200];
} struct_message;

struct_message incomingData;
struct_message outgoingData;
esp_now_peer_info_t peerInfo;

void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *incomingDataPtr, int len) {
  
  // Extract MAC address from the new info structure
  const uint8_t * mac = info->src_addr;

  memcpy(&incomingData, incomingDataPtr, sizeof(incomingData));
  
  // LOG RECEIVED
  if (incomingData.msgType == 0) {
    String newLog = String(incomingData.payload);
    Serial.println("New Log: " + newLog);
    
    // Append to history
    logHistory = newLog + "\n" + logHistory;
    
    if (logHistory.length() > 1000) {
      logHistory = logHistory.substring(0, 1000);
    }
  }
  
  // HISTORY REQUEST RECEIVED
  else if (incomingData.msgType == 1) {
    Serial.println("History Requested by Controller");
    
    outgoingData.msgType = 2; // Response type
    
    String safePayload = logHistory.substring(0, 190); 
    safePayload.toCharArray(outgoingData.payload, 200);
    
    // Register the sender as a peer dynamically to reply
    if (!esp_now_is_peer_exist(mac)) {
      memcpy(peerInfo.peer_addr, mac, 6);
      peerInfo.channel = 0;  
      peerInfo.encrypt = false;
      esp_now_add_peer(&peerInfo);
    }
    
    esp_now_send(mac, (uint8_t *) &outgoingData, sizeof(outgoingData));
    Serial.println("History Sent");
  }
}

void setup() {
  Serial.begin(115200);
  
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  esp_now_register_recv_cb(OnDataRecv);
  
  Serial.println("Logger Device Ready.");
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  delay(1000);
}
