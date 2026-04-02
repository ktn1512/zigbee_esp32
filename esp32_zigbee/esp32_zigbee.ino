#include <WiFi.h>
#include <PubSubClient.h>

#define RXD2 16
#define TXD2 17

// ===== WIFI =====
const char* ssid = "Mi 9T Pro";
const char* password = "khanhkhanh";

// ===== THINGSBOARD =====
const char* mqtt_server = "mqtt.eu.thingsboard.cloud";
const char* token = "lxpQWQFvVUWjMAJ7PjsB";

WiFiClient espClient;
PubSubClient client(espClient);

String data = "";
float node1_temp = 0;
float node1_hum = 0;
float node1_lux = 0;

bool hasTemp = false;
bool hasHum = false;
bool hasLux = false;

float currentLux = 0;

bool ledState = false;    // trạng thái LED hiện tại
bool autoEnabled = true;  // cho phép auto hay không

float luxOnThreshold = 50;  // ngưỡng ánh sáng (tùy chỉnh)
float luxOffThreshold = 100;
// ===== WIFI CONNECT =====
void setup_wifi() {
  Serial.println("Connecting WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi OK");
}

// ===== PARSE RPC JSON → LẤY CMD =====
String getCmdFromJson(String msg) {

  int idx = msg.indexOf("cmd");
  if (idx < 0) return "";

  int start = msg.indexOf(":", idx) + 2;
  int end = msg.indexOf("\"", start);

  if (start < 0 || end < 0) return "";

  return msg.substring(start, end);
}

// ===== MQTT CALLBACK =====
void callback(char* topic, byte* payload, unsigned int length) {

  String msg = "";

  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.println("\n=== RAW RPC ===");
  Serial.println(msg);

  String cmd = getCmdFromJson(msg);

  if (cmd != "") {

    Serial.println("SEND TO ZIGBEE:");
    Serial.println(cmd);

    Serial2.println(cmd);

    // ===== MANUAL CONTROL =====
    if (cmd.indexOf("LED:ON") >= 0) {
      ledState = true;
      autoEnabled = false;  // ❗ manual thì tắt auto

      client.publish("v1/devices/me/telemetry", "{\"led_state\":true}");
      client.publish("v1/devices/me/telemetry", "{\"auto_mode\":false}");

    } else if (cmd.indexOf("LED:OFF") >= 0) {
      ledState = false;
      autoEnabled = false;

      client.publish("v1/devices/me/telemetry", "{\"led_state\":false}");
      client.publish("v1/devices/me/telemetry", "{\"auto_mode\":false}");
    } else if (cmd.indexOf("AUTO:ON") >= 0) {
      autoEnabled = true;
      client.publish("v1/devices/me/telemetry", "{\"auto_mode\":true}");
      Serial.println("MODE: AUTO ENABLED");

    } else if (cmd.indexOf("AUTO:OFF") >= 0) {
      autoEnabled = false;
      client.publish("v1/devices/me/telemetry", "{\"auto_mode\":false}");
      Serial.println("MODE: AUTO DISABLED");
    }
  }
}

// ===== MQTT RECONNECT =====
void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting MQTT...");

    if (client.connect("ESP32", token, NULL)) {
      Serial.println("OK");

      client.subscribe("v1/devices/me/rpc/request/+");

    } else {
      Serial.println("FAILED");
      delay(2000);
    }
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  Serial.println("SYSTEM READY");
}

// ===== PARSE DATA ZIGBEE =====
void processZigbee(String input) {

  Serial.println("\n=== DATA FROM ZIGBEE ===");
  Serial.println(input);

  int p1 = input.indexOf(':');
  int p2 = input.indexOf(':', p1 + 1);

  if (p1 < 0 || p2 < 0) return;

  String node = input.substring(0, p1);
  String type = input.substring(p1 + 1, p2);
  String value = input.substring(p2 + 1);

  value.trim();

  if (node == "NODE1") {

    if (type == "TEMP") {
      node1_temp = value.toFloat();
      hasTemp = true;
    } else if (type == "HUMID") {
      node1_hum = value.toFloat();
      hasHum = true;
    } else if (type == "LUX") {
      node1_lux = value.toFloat();
      currentLux = node1_lux;
      hasLux = true;
    }

    // ✅ Khi đủ 3 giá trị → gửi 1 lần
    if (hasTemp && hasHum && hasLux) {

      String json = "{";
      json += "\"node1_temp\":" + String(node1_temp) + ",";
      json += "\"node1_hum\":" + String(node1_hum) + ",";
      json += "\"node1_light\":" + String(node1_lux);
      json += "}";

      Serial.println("SEND TO CLOUD:");
      Serial.println(json);

      client.publish("v1/devices/me/telemetry", json.c_str());

      // reset lại để chờ vòng mới
      hasTemp = false;
      hasHum = false;
      hasLux = false;
    }
    // ===== AUTO CONTROL =====
    if (autoEnabled) {

      if (currentLux < luxOnThreshold && !ledState) {

        Serial.println("AUTO: TURN ON LED");

        Serial2.println("NODE1:LED:ON");
        ledState = true;

        String stateJson = "{";
        stateJson += "\"led_state\":" + String(ledState ? true : false);
        stateJson += "}";

        client.publish("v1/devices/me/telemetry", "{\"led_state\":true,\"auto_mode\":true}");
      } else if (currentLux > luxOffThreshold && ledState) {

        Serial.println("AUTO: TURN OFF LED");
        Serial2.println("NODE1:LED:OFF");

        ledState = false;

        String stateJson = "{";
        stateJson += "\"led_state\":" + String(ledState ? 1 : 0);
        stateJson += "}";

        client.publish("v1/devices/me/telemetry", "{\"led_state\":false,\"auto_mode\":true}");
      }
    }
  }
}

// ===== LOOP =====
void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // ===== NHẬN ZIGBEE =====
  while (Serial2.available()) {
    char c = Serial2.read();

    if (c == '\n') {

      data.trim();

      if (data.length() > 0) {
        processZigbee(data);
      }

      data = "";
    } else {
      data += c;
    }
  }
}