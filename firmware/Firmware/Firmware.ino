#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <ArduinoJson.h>
#include <cppQueue.h>

ESP8266WiFiMulti WiFiMulti;

#define SEND_STATE_BUFFER_LENGTH 1024

char sendStateBuffer[SEND_STATE_BUFFER_LENGTH];
WebSocketsServer webSocket = WebSocketsServer(81);

#define DEBUG
#define USE_SERIAL Serial
#define CHANNELS 2

const int pins[] = {D7, D6};
const char* keys[] = {"0", "1"};
int brightness[] = {0, 0};
bool do_state_update = false;
Queue send_state_to_clients(sizeof(uint8_t), 10, FIFO);

void schledule_send_state_to_client(const uint8_t num)
{
  if (!send_state_to_clients.isFull())
  {
    send_state_to_clients.push(&num);
  }
}

void schledule_state_update()
{
  do_state_update = true;
}

void send_responses()
{
  if(do_state_update)
  {
    do_state_update = false;
    update_states();
    broadcastState();
    send_state_to_clients.clean();
  }
  else
  {
    uint8_t send_to;
    while(send_state_to_clients.pop(&send_to))
    {
      sendState(send_to);
    }
  }
}

void zeroSendStateBuffer(const int len)
{
  for (int i = 0; i < len; i++)
  {
    sendStateBuffer[i] = 0;
  }
}

void broadcastState()
{
  sendState(true, 0);
}

void sendState(const uint8_t num)
{
  sendState(false, num);
}

void sendState(const bool broadcast, const uint8_t num)
{
  StaticJsonBuffer<1024> outJsonBuffer;
  JsonObject& out = outJsonBuffer.createObject();
  for (int i = 0; i < CHANNELS; i++)
  {
    out[keys[i]] = brightness[i];
  }
  int length = out.measureLength();
  out.printTo(sendStateBuffer);
  if (broadcast)
  {
    webSocket.broadcastTXT(sendStateBuffer, length);
  }
  else
  {
    webSocket.sendTXT(num, sendStateBuffer, length);
  }
  zeroSendStateBuffer(length);
}

JsonObject& parseMessage(const uint8_t* payload)
{
  StaticJsonBuffer<1024> inputBuffer;
  JsonObject& root = inputBuffer.parseObject(payload);
  return root;
}

int getValue(const JsonObject& root, const int index)
{
  const char* key = keys[index];
  if (root.containsKey(key) && root.is<int>(key))
  {
    int value = root[key];
    if (value >= 0 && value < 1024)
    {
      return value;
    }
  }
  return -1;
}

void load_new_state(const JsonObject& root, const int index)
{
  int value = getValue(root, index);
  if (value != -1)
  {
    brightness[index] = value;
  }
}

void load_new_states(const JsonObject& root)
{
  for (int i = 0; i < CHANNELS; i++)
  {
    load_new_state(root, i);
  }
}

void update_states()
{
  for (int i = 0; i < CHANNELS; i++)
  {
    analogWrite(pins[i], brightness[i]);
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  switch (type) {
    case WStype_DISCONNECTED:
      {
        break;
      }
    case WStype_CONNECTED:
      {
        schledule_send_state_to_client(num);
        break;
      }
    case WStype_TEXT:
      {
        JsonObject& root = parseMessage(payload);
        if (!root.success()) {
#ifdef DEBUG
          USE_SERIAL.printf("Invalid JSON");
#endif
        }
        else {
          load_new_states(root);
          schledule_state_update();
        }
        break;
      }
    case WStype_BIN:
      {
        break;
      }
  }
}

void setup() {
  USE_SERIAL.begin(115200);
  USE_SERIAL.setDebugOutput(true);

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();
  update_states();
  zeroSendStateBuffer(SEND_STATE_BUFFER_LENGTH);

  for (uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }

  WiFiMulti.addAP("JJNet", "JanickoAJanka.net");

  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  ESP.wdtFeed();
  webSocket.loop();
  ESP.wdtFeed();
  send_responses();
}

