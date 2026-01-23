#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* ---------- OLED ---------- */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

/* ---------- BLE ---------- */
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-5678-1234-5678-abcdef123456"

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;

/* ---------- State ---------- */
String currentText = "munametag";   // default text
bool deviceConnected = false;
uint16_t connId = 0;

unsigned long disconnectAt = 0;
bool disconnectScheduled = false;

/* ---------- OLED helper ---------- */
void drawText(const String& text) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println(text);
  display.display();
}

/* ---------- BLE Callbacks ---------- */
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server, esp_ble_gatts_cb_param_t* param) override {
    deviceConnected = true;
    connId = param->connect.conn_id;

    // start 10s disconnect timer ON CONNECT
    disconnectAt = millis() + 10000;
    disconnectScheduled = true;
  }

  void onDisconnect(BLEServer* server) override {
    deviceConnected = false;
    disconnectScheduled = false;
    BLEDevice::startAdvertising(); // make ESP available again
  }
};

class CharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    std::string value = characteristic->getValue();
    if (value.length() > 0) {
      currentText = String(value.c_str());
      drawText(currentText);
    }
  }
};

void setup() {
  Serial.begin(115200);

  /* OLED */
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true);
  }
  drawText(currentText);  // show default text on boot

  /* BLE */
  BLEDevice::init("ESP32_OLED");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* service = pServer->createService(SERVICE_UUID);

  pCharacteristic = service->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );

  pCharacteristic->setCallbacks(new CharacteristicCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());

  service->start();
  BLEDevice::startAdvertising();
}

void loop() {
  if (disconnectScheduled && deviceConnected && millis() >= disconnectAt) {
    pServer->disconnect(connId);   // force disconnect
    disconnectScheduled = false;
  }
}
