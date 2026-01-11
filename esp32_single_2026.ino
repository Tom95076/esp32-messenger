#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <U8g2lib.h>
#include <Wire.h>

// ================= USER =================
#define USER_NAME "Funky_Chicken"   // CHANGE PER DEVICE

// ================= PINS =================
#define ENC_A   32
#define ENC_B   33
#define ENC_BTN 25
#define LED_PIN 2

// OLED I2C
#define OLED_SDA 21
#define OLED_SCL 22

// ================= OLED =================
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0);

// ================= STATE =================
unsigned long lastBlink = 0;
bool ledState = false;

bool sendMode = false;
int lastA;

// --- encoder smoothing ---
unsigned long lastEncMove = 0;
#define ENC_DELAY 120   // ms between steps (menu feel)

// ================= MESSAGE =================
struct Message {
  String from;
  String text;
};

#define MAX_MSG 8
Message inbox[MAX_MSG];
int msgCount = 0;
int inboxIndex = 0;

// ================= QUICK MESSAGES =================
const char* quickMsg[] = {
  "HELLO",
  "WHATS UP?",
  "LETS MEET UP",
  "MAKE IT SO!",
  "BUSY",
  "BYE"
};
const int QUICK_COUNT = 6;
int quickIndex = 0;

// ================= ESP-NOW PACKET =================
typedef struct {
  char from[16];
  char msg[64];
} InfoPacket;

// ================= DRAW =================
void draw() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tf);

  u8g2.drawStr(0, 8, USER_NAME);
  u8g2.drawLine(0, 10, 128, 10);

  if (sendMode) {
    u8g2.drawStr(0, 22, quickMsg[quickIndex]);
  } else if (msgCount == 0) {
    u8g2.drawStr(0, 22, "NO MESSAGES");
  } else {
    Message &m = inbox[inboxIndex];
    u8g2.drawStr(0, 20, m.from.c_str());
    u8g2.drawStr(0, 30, m.text.c_str());
  }

  u8g2.sendBuffer();
}

// ================= ADD MESSAGE =================
void addMessage(String from, String text) {
  inbox[msgCount % MAX_MSG] = {from, text};
  msgCount++;
  inboxIndex = (msgCount - 1) % MAX_MSG;
}

// ================= ESP-NOW RECEIVE =================
void onReceive(const uint8_t *mac,
               const uint8_t *data,
               int len) {
  if (len != sizeof(InfoPacket)) return;

  InfoPacket pkt;
  memcpy(&pkt, data, sizeof(pkt));

  addMessage(pkt.from, pkt.msg);
}

// ================= SEND MESSAGE =================
void sendMessage(const char *text) {
  InfoPacket pkt = {};
  strncpy(pkt.from, USER_NAME, sizeof(pkt.from) - 1);
  strncpy(pkt.msg, text, sizeof(pkt.msg) - 1);

  uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  esp_now_send(broadcast, (uint8_t*)&pkt, sizeof(pkt));
}

// ================= SETUP =================
void setup() {
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_BTN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  Serial.begin(115200);

  Wire.begin(OLED_SDA, OLED_SCL);
  u8g2.begin();

  lastA = digitalRead(ENC_A);

  // ---- ESP-NOW (LOCKED + WORKING) ----
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) {
    while (true);
  }

  esp_now_register_recv_cb(onReceive);

  esp_now_peer_info_t peer = {};
  memset(peer.peer_addr, 0xFF, 6);
  peer.channel = 1;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

// ================= LOOP =================
void loop() {
  // Heartbeat LED
  if (millis() - lastBlink > 1000) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
    lastBlink = millis();
  }

  // -------- Encoder (smoothed menu) --------
  int a = digitalRead(ENC_A);
  if (a != lastA && millis() - lastEncMove > ENC_DELAY) {
    lastEncMove = millis();

    if (digitalRead(ENC_B) != a) {
      sendMode ? quickIndex++ : inboxIndex++;
    } else {
      sendMode ? quickIndex-- : inboxIndex--;
    }
  }
  lastA = a;

  quickIndex = constrain(quickIndex, 0, QUICK_COUNT - 1);
  inboxIndex = constrain(inboxIndex, 0, max(msgCount - 1, 0));

  // Button
  if (digitalRead(ENC_BTN) == LOW) {
    if (sendMode) {
      sendMessage(quickMsg[quickIndex]);
      sendMode = false;
    } else {
      sendMode = true;
    }
    delay(300);
  }

  draw();
}
