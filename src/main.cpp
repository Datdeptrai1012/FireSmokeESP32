#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==== OLED ====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==== WiFi ====
const char* ssid = "Thu";
const char* password = "12345678";

// ==== MQTT ====
const char* mqtt_server = "broker.emqx.io";
WiFiClient espClient;
PubSubClient client(espClient);

// ==== Pin ====
#define MQ2_PIN      34
#define FLAME_PIN    27
#define FAN_PIN      14
#define PUMP_PIN     12
#define BUZZER_PIN   25

#define FLAME_ACTIVE HIGH

int smoke_threshold = 400;

bool auto_mode = true;
bool manual_fan = false;
bool manual_pump = false;
bool manual_buzzer = false;


// ===== MQTT reconnect =====
void reconnect() {
  while (!client.connected()) {
    Serial.println("MQTT connecting...");
    if (client.connect("ESP32_FIRE_SMOKE")) {
      Serial.println("MQTT connected!");
      client.subscribe("home/control");
    }
    delay(1000);
  }
}


// ===== MQTT callback =====
void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];

  if (String(topic) == "home/control") {

    if (msg == "auto_on")  auto_mode = true;
    if (msg == "auto_off") auto_mode = false;

    if (msg == "fan_on")   { manual_fan = true; digitalWrite(FAN_PIN, HIGH); }
    if (msg == "fan_off")  { manual_fan = false; digitalWrite(FAN_PIN, LOW); }

    if (msg == "pump_on")  { manual_pump = true; digitalWrite(PUMP_PIN, HIGH); }
    if (msg == "pump_off") { manual_pump = false; digitalWrite(PUMP_PIN, LOW); }

    if (msg == "buzzer_on")  { manual_buzzer = true; digitalWrite(BUZZER_PIN, HIGH); }
    if (msg == "buzzer_off") { manual_buzzer = false; digitalWrite(BUZZER_PIN, LOW); }
  }
}


// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  ledcSetup(0, 2000, 10);   // channel 0, 2kHz, 10-bit
  ledcAttachPin(BUZZER_PIN, 0);


  pinMode(MQ2_PIN, INPUT);
  pinMode(FLAME_PIN, INPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(FAN_PIN, LOW);
  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // ==== OLED INIT ====
  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    while (1);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("FIRE SYS");
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.println("ESP32 Booting...");
  display.display();
  delay(1500);

  // ==== WiFi ====
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(200);

  // ==== MQTT ====
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  client.setBufferSize(1024);
}


// ===================== LOOP =====================
void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  int smoke = analogRead(MQ2_PIN);
  int flame = digitalRead(FLAME_PIN);
  flame = !flame;
  bool smoke_detected = (smoke > smoke_threshold);
  bool fire_detected  = (flame == FLAME_ACTIVE);

  // ======== OLED DISPLAY ========
  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Smoke: ");
  display.println(smoke);

  display.setCursor(0, 12);
  display.print("Flame: ");
  display.println(flame);

  display.setCursor(0, 30);
  if (fire_detected) {
      display.setTextSize(2);
      display.println("FIRE !!!");
  } else {
      display.setTextSize(2);
      display.println("NO FIRE");
  }

  display.display();


  // ======== AUTO MODE ========
  if (auto_mode) {

      if (smoke_detected) {
          digitalWrite(FAN_PIN, HIGH);
          delay(5000);
          ledcWriteTone(0, 1000);  // 1kHz
      } else if (!manual_fan) {
          digitalWrite(FAN_PIN, LOW);
      }

      if (fire_detected) {
          digitalWrite(PUMP_PIN, HIGH);
          delay(5000);
          ledcWriteTone(0, 2000);  // 2kHz
      } else if (!manual_pump) {
          digitalWrite(PUMP_PIN, LOW);
      }

      if (!smoke_detected && !fire_detected && !manual_buzzer) {
          ledcWriteTone(0, 0);     // tắt buzzer
      }
  }

  // ======== SEND MQTT ========
  String payload = "{";
  payload += "\"smoke\":" + String(smoke) + ",";
  payload += "\"flame\":" + String(flame) + ",";
  payload += "\"fan\":" + String(digitalRead(FAN_PIN)) + ",";
  payload += "\"pump\":" + String(digitalRead(PUMP_PIN)) + ",";
  payload += "\"buzzer\":" + String(digitalRead(BUZZER_PIN)) + ",";
  payload += "\"auto_mode\":" + String(auto_mode);
  payload += "}";

  client.publish("home/sensor/data", payload.c_str());

  delay(500);
}
