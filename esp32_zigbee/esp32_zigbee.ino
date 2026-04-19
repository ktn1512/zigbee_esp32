#include <WiFi.h>
#include <PubSubClient.h>

// ===== UART =====
#define RXD2 16
#define TXD2 17

// ===== WIFI =====
const char* ssid = "Mi 9T Pro";
const char* password = "khanhkhanh";

// ===== MQTT =====
const char* mqtt_server = "mqtt.eu.thingsboard.cloud";
const char* token = "lxpQWQFvVUWjMAJ7PjsB";

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastReconnectAttempt = 0;

// ===== UART BUFFER =====
char uartBuf[128];
uint8_t uartIdx = 0;

// ===== ROOM DATA =====
struct Room {
  float temp;
  float hum;
  float lux;
  bool led;
  bool fan;
  bool autoMode;
  bool ready;
  unsigned long lastUpdate;
  unsigned long lastOnTime;
};

Room room1, room2;

struct Cmd {
  String data;
  int id;
  unsigned long timeSent;
  int retry;
  bool waitingAck;
};

#define CMD_QUEUE_SIZE 10
Cmd cmdQueue[CMD_QUEUE_SIZE];
int cmdHead = 0;
int cmdTail = 0;
int cmdCounter = 0;

// ===== NODE2 =====
float dc = 0;
float dc_power = 0;
float ac = 0;
float ac_power = 0;

bool hasDC = false, hasPower = false;
bool hasAC = false, hasACPower = false;
// ===== CONFIG =====
unsigned long delayOff = 10000;
unsigned long timeoutNode1 = 2000;

float luxOn = 30;
float luxOff = 150;

float fanOnTemp = 23;
float fanOffTemp = 20;

// ===== WIFI =====
void setup_wifi() {
  //Serial.println("Connecting WiFi...");  //xóa
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    //Serial.println("\nWiFi connected!");  //xóa
  }
}

// ===== SEND MQTT =====
void publish(const char* msg) {
  // Serial.print("[MQTT SEND] ");  //xóa
  // Serial.println(msg);           //xóa

  client.publish("v1/devices/me/telemetry", msg);
}

// ===== SAFE CMD PARSE =====
bool extractCmd(char* msg, char* out) {
  char* p = strstr(msg, "cmd\":\"");
  if (!p) return false;

  p += 6;
  char* end = strchr(p, '"');
  if (!end) return false;

  strncpy(out, p, end - p);
  out[end - p] = '\0';
  return true;
}

void sendCmd(const char* cmd) {
  enqueueCmd(cmd);
}

void enqueueCmd(const char* raw) {

  Cmd& c = cmdQueue[cmdTail];

  c.id = cmdCounter++;

  char full[64];
  sprintf(full, "CMD:%d:%s", c.id, raw);

  c.data = String(full);
  c.retry = 0;
  c.waitingAck = false;

  cmdTail = (cmdTail + 1) % CMD_QUEUE_SIZE;
}

void processCmdQueue() {

  if (cmdHead == cmdTail) return;

  Cmd& c = cmdQueue[cmdHead];

  if (!c.waitingAck) {
    Serial2.println(c.data);
    c.timeSent = millis();
    c.waitingAck = true;
  }

  if (c.waitingAck && millis() - c.timeSent > 500) {

    if (c.retry < 3) {
      Serial2.println(c.data);
      c.timeSent = millis();
      c.retry++;
    } else {
      // bỏ nếu fail
      cmdHead = (cmdHead + 1) % CMD_QUEUE_SIZE;
    }
  }
}

// ===== MQTT CALLBACK =====
void callback(char* topic, byte* payload, unsigned int length) {
  //xóa
  // Serial.print("\n[MQTT RECV] Topic: ");
  // Serial.println(topic);

  // Serial.print("[MQTT RECV] Payload: ");
  // Serial.write(payload, length);
  // Serial.println();  //xóa

  char msg[128];
  memcpy(msg, payload, length);
  msg[length] = '\0';

  char cmd[64];

  if (!extractCmd(msg, cmd)) return;

  // thêm NODE1 nếu thiếu
  if (strstr(cmd, "NODE1") == NULL) {
    char tmp[64];
    sprintf(tmp, "NODE1:1:%s", cmd);
    strcpy(cmd, tmp);
  }

  sendCmd(cmd);

  // ===== ROOM 1 =====
  if (strcmp(cmd, "NODE1:1:LED:ON") == 0) {
    room1.led = true;
    room1.autoMode = false;
    publish("{\"room1_led\":true,\"room1_auto\":false}");
  } else if (strcmp(cmd, "NODE1:1:LED:OFF") == 0) {
    room1.led = false;
    room1.autoMode = false;
    publish("{\"room1_led\":false,\"room1_auto\":false}");
  } else if (strcmp(cmd, "NODE1:1:FAN:ON") == 0) {
    room1.fan = true;
    room1.autoMode = false;
    publish("{\"room1_fan\":true,\"room1_auto\":false}");
  } else if (strcmp(cmd, "NODE1:1:FAN:OFF") == 0) {
    room1.fan = false;
    room1.autoMode = false;
    publish("{\"room1_fan\":false,\"room1_auto\":false}");
  }

  // ===== ROOM 2 =====
  else if (strcmp(cmd, "NODE1:2:LED:ON") == 0) {
    room2.led = true;
    room2.autoMode = false;
    publish("{\"room2_led\":true,\"room2_auto\":false}");
  } else if (strcmp(cmd, "NODE1:2:LED:OFF") == 0) {
    room2.led = false;
    room2.autoMode = false;
    publish("{\"room2_led\":false,\"room2_auto\":false}");
  } else if (strcmp(cmd, "NODE1:2:FAN:ON") == 0) {
    room2.fan = true;
    room2.autoMode = false;
    publish("{\"room2_fan\":true,\"room2_auto\":false}");
  } else if (strcmp(cmd, "NODE1:2:FAN:OFF") == 0) {
    room2.fan = false;
    room2.autoMode = false;
    publish("{\"room2_fan\":false,\"room2_auto\":false}");
  }

  // ===== AUTO =====
  else if (strcmp(cmd, "NODE1:1:AUTO:ON") == 0) {
    room1.autoMode = true;
    publish("{\"room1_auto\":true}");
    handleAuto(room1, 1);
  } else if (strcmp(cmd, "NODE1:2:AUTO:ON") == 0) {
    room2.autoMode = true;
    publish("{\"room2_auto\":true}");
    handleAuto(room1, 2);
  } else if (strcmp(cmd, "NODE1:1:AUTO:OFF") == 0) {
    room1.autoMode = false;
    publish("{room1_auto\":false}");
  } else if (strcmp(cmd, "NODE1:2:AUTO:OFF") == 0) {
    room2.autoMode = false;
    publish("{\room2_auto\":false}");
  }

// Serial.print("[CMD PARSED] ");  //xóa
// Serial.println(cmd);            //xóa
}

// ===== MQTT RECONNECT =====
boolean reconnect() {
  // Serial.println("Connecting MQTT...");  //xóa

  if (client.connect("ESP32", token, NULL)) {
    //   Serial.println("MQTT connected!");  //xóa

    client.subscribe("v1/devices/me/rpc/request/+");
    return true;
  }
  return false;
}
void handleFan(Room& r, int id) {

  if (r.temp > fanOnTemp && !r.fan) {

    char cmd[32];
    sprintf(cmd, "NODE1:%d:FAN:ON", id);
    sendCmd(cmd);

    r.fan = true;
    // Serial.println("[FAN] ON");  //xóa


    if (id == 1) publish("{\"room1_fan\":true}");
    else publish("{\"room2_fan\":true}");
  }

  else if (r.temp < fanOffTemp && r.fan) {

    char cmd[32];
    sprintf(cmd, "NODE1:%d:FAN:OFF", id);
    sendCmd(cmd);

    r.fan = false;
    // Serial.println("[FAN] OFF");  //xóa


    if (id == 1) publish("{\"room1_fan\":false}");
    else publish("{\"room2_fan\":false}");
  }

  // Serial.print("[FAN] Room ");  //xóa
  // Serial.print(id);             //xóa
  // Serial.print(" Temp=");       //xóa
  // Serial.println(r.temp);       //xóa
}
// ===== AUTO CONTROL =====
void handleAuto(Room& r, int id) {

  if (!r.autoMode) return;

  // ===== LED AUTO =====
  if (r.lux >= 0) {
    if (r.lux < luxOn && !r.led) {
      char cmd[32];
      sprintf(cmd, "NODE1:%d:LED:ON", id);
      sendCmd(cmd);

      r.led = true;
      r.lastOnTime = millis();

      if (id == 1) publish("{\"room1_led\":true}");
      else publish("{\"room2_led\":true}");
    }

    else if (r.lux > luxOff && r.led) {
      if (millis() - r.lastOnTime > delayOff) {

        char cmd[32];
        sprintf(cmd, "NODE1:%d:LED:OFF", id);
        sendCmd(cmd);

        r.led = false;

        if (id == 1) publish("{\"room1_led\":false}");
        else publish("{\"room2_led\":false}");
      }
    }
  }

  // ===== FAN AUTO =====
  if (r.temp > fanOnTemp && !r.fan) {

    char cmd[32];
    sprintf(cmd, "NODE1:%d:FAN:ON", id);
    sendCmd(cmd);

    r.fan = true;

    if (id == 1) publish("{\"room1_fan\":true}");
    else publish("{\"room2_fan\":true}");
  }

  else if (r.temp < fanOffTemp && r.fan) {

    char cmd[32];
    sprintf(cmd, "NODE1:%d:FAN:OFF", id);
    sendCmd(cmd);

    r.fan = false;

    if (id == 1) publish("{\"room1_fan\":false}");
    else publish("{\"room2_fan\":false}");
  }
}

// ===== PROCESS ZIGBEE =====
void processLine(char* line) {
  if (strncmp(line, "ACK:", 4) == 0) {

    int id;
    sscanf(line, "ACK:%d", &id);

    if (cmdHead != cmdTail) {
      Cmd& c = cmdQueue[cmdHead];

      if (c.id == id) {
        cmdHead = (cmdHead + 1) % CMD_QUEUE_SIZE;
      }
    }

    return;
  }
  char node[8], type[16];
  int group;
  float value;

  if (sscanf(line, "%[^:]:%d:%[^:]:%f", node, &group, type, &value) != 4) return;

  // Serial.print("[PARSED] ");  //xóa
  // Serial.print(node);         //xóa
  // Serial.print(" | ");        //xóa
  // Serial.print(group);        //xóa
  // Serial.print(" | ");        //xóa
  // Serial.print(type);         //xóa
  // Serial.print(" | ");        //xóa
  // Serial.println(value);      //xóa

  // ===== NODE1 =====
  if (strcmp(node, "NODE1") == 0) {

    Room* r = (group == 1) ? &room1 : &room2;

    if (strcmp(type, "TEMP") == 0) {
      r->temp = value;
      handleAuto(*r, group);
    } else if (strcmp(type, "HUMID") == 0) r->hum = value;
    else if (strcmp(type, "LUX") == 0) {
      r->lux = value;
      r->ready = true;
      r->lastUpdate = millis();

      handleAuto(*r, group);
    }
  }

  // ===== NODE2 =====
  else if (strcmp(node, "NODE2") == 0) {

    if (strcmp(type, "DC") == 0) {
      dc = value;
      hasDC = true;
    } else if (strcmp(type, "DC_POWER") == 0) {
      dc_power = value;
      hasPower = true;
    } else if (strcmp(type, "AC") == 0) {
      ac = value;
      hasAC = true;
    } else if (strcmp(type, "AC_POWER") == 0) {
      ac_power = value;
      hasACPower = true;
    }
  }
}

// ===== SEND NODE1 =====
void sendNode1() {

  char json[256];
  int len = 0;

  len += sprintf(json + len, "{");

  if (room1.ready) {
    len += sprintf(json + len,
                   "\"room1_temp\":%.2f,\"room1_hum\":%.2f,\"room1_light\":%.2f,"
                   "\"room1_led\":%s,\"room1_fan\":%s,\"room1_auto\":%s,",
                   room1.temp, room1.hum, room1.lux,
                   room1.led ? "true" : "false",
                   room1.fan ? "true" : "false",
                   room1.autoMode ? "true" : "false");
  }

  if (room2.ready) {
    len += sprintf(json + len,
                   "\"room2_temp\":%.2f,\"room2_hum\":%.2f,\"room2_light\":%.2f,"
                   "\"room2_led\":%s,\"room2_fan\":%s,\"room2_auto\":%s,",
                   room2.temp, room2.hum, room2.lux,
                   room2.led ? "true" : "false",
                   room2.fan ? "true" : "false",
                   room2.autoMode ? "true" : "false");
  }

  if (json[len - 1] == ',') len--;

  sprintf(json + len, "}");

  // Serial.print("[SEND NODE1] ");  //xóa
  // Serial.println(json);           //xóa

  publish(json);

  room1.ready = false;
  room2.ready = false;
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

// ===== LOOP =====
void loop() {

  // ===== WIFI =====
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(1000);
    return;
  }

  // ===== MQTT =====
  if (!client.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      if (reconnect()) lastReconnectAttempt = 0;
    }
  } else {
    client.loop();
  }

  // ===== UART =====
  while (Serial2.available()) {

    char c = Serial2.read();

    if (c == '\n' || c == '\r') {
      uartBuf[uartIdx] = '\0';

      // Serial.print("[UART RECV] ");  //xóa

      processLine(uartBuf);
      uartIdx = 0;
    } else if (uartIdx < sizeof(uartBuf) - 1) {
      uartBuf[uartIdx++] = c;
    }
  }
  processCmdQueue();
  // ===== SEND NODE1 =====
  unsigned long now = millis();

  if ((room1.ready && room2.ready) || (room1.ready && now - room1.lastUpdate > timeoutNode1) || (room2.ready && now - room2.lastUpdate > timeoutNode1)) {

    sendNode1();
  }

  // ===== SEND NODE2 =====
  if ((hasDC && hasPower) || (hasAC && hasACPower)) {

    char json[128];
    int len = 0;

    len += sprintf(json + len, "{");

    if (hasDC && hasPower) {
      len += sprintf(json + len,
                     "\"dc_current\":%.3f,\"dc_power\":%.3f,",
                     dc, dc_power);
    }

    if (hasAC && hasACPower) {
      len += sprintf(json + len,
                     "\"ac_current\":%.3f,\"ac_power\":%.3f,",
                     ac, ac_power);
    }

    if (json[len - 1] == ',') len--;

    sprintf(json + len, "}");

    publish(json);

    hasDC = hasPower = false;
    hasAC = hasACPower = false;
  }
}