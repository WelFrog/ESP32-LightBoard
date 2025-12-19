#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <BluetoothSerial.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif

//宏定义
#define LED_PIN          18       // 控制灯珠引脚
#define NUM_LEDS         64       // 灯珠总数
#define DEFAULT_BRIGHTNESS_PERCENT 100 // 默认亮度百分比

//协议常量
#define SYNC_BYTE 0xAA
#define ACK_BYTE 0xBB

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
BluetoothSerial SerialBT;
uint8_t globalBrightness = DEFAULT_BRIGHTNESS_PERCENT * 2.55;

struct CRGB {
  uint8_t g;
  uint8_t r;
  uint8_t b;
};

struct FrameData {
  float duration; // 帧时长
  CRGB pixels[NUM_LEDS];
};

struct AnimationData {
  unsigned int totalFrames;
  FrameData frames[0];
};
AnimationData* currentAnimation = nullptr;
unsigned long frameStartTime = 0;
unsigned int currentFrameIndex = 0;

enum DataState { WAITING_FOR_SYNC, WAITING_FOR_FRAME_COUNT, RECEIVING_DATA };
DataState dataState = WAITING_FOR_SYNC;
unsigned int newTotalFrames = 0;
unsigned int currentFrameBeingReceived = 0;
AnimationData* newAnimationData = nullptr;

void playAnimation();
void handleBluetoothData();
void createDefaultAnimation();

void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.setBrightness(globalBrightness);
  strip.show();
  SerialBT.begin("ESP32 LED Matrix");
  Serial.println("Bluetooth started, device name: ESP32 LED Matrix");
  createDefaultAnimation();
  
  strip.setBrightness(globalBrightness);
}

void loop() {
  playAnimation();
  handleBluetoothData();
}

void createDefaultAnimation() {
  if (currentAnimation != nullptr) {
    free(currentAnimation);
  }
  unsigned int totalFrames = NUM_LEDS;
  size_t dataSize = sizeof(AnimationData) + totalFrames * sizeof(FrameData);
  currentAnimation = (AnimationData*)malloc(dataSize);
  if (currentAnimation == nullptr) {
    Serial.println("Memory allocation for default animation failed!");
    return;
  }
  currentAnimation->totalFrames = totalFrames;
  for (unsigned int i = 0; i < totalFrames; i++) {
    currentAnimation->frames[i].duration = 0.05f;
    for (int j = 0; j < NUM_LEDS; j++) {
      currentAnimation->frames[i].pixels[j] = {0, 0, 0};
    }
    currentAnimation->frames[i].pixels[i] = {0, 0, 255};
  }
}

void playAnimation() {
  if (currentAnimation == nullptr || currentAnimation->totalFrames == 0) return;
  if (millis() - frameStartTime >= currentAnimation->frames[currentFrameIndex].duration * 1000) {
    frameStartTime = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, currentAnimation->frames[currentFrameIndex].pixels[i].r, 
                          currentAnimation->frames[currentFrameIndex].pixels[i].g, 
                          currentAnimation->frames[currentFrameIndex].pixels[i].b);
    }
    strip.show();
    currentFrameIndex = (currentFrameIndex + 1) % currentAnimation->totalFrames;
  }
}

void handleBluetoothData() {
  static size_t bytesReceived = 0;
  static size_t totalBytesToReceive = 0;

  while (SerialBT.available()) {
    switch (dataState) {
      case WAITING_FOR_SYNC:
        if (SerialBT.read() == SYNC_BYTE) {
          Serial.println("Sync byte received. Waiting for frame count...");
          dataState = WAITING_FOR_FRAME_COUNT;
          SerialBT.write(ACK_BYTE); 
        }
        break;

      case WAITING_FOR_FRAME_COUNT:
      {
        while (SerialBT.available() < 4) {
          delay(10); 
        }
        
        SerialBT.readBytes((char*)&newTotalFrames, sizeof(newTotalFrames));
        Serial.printf("Total frames to receive: %d. Allocating memory...\n", newTotalFrames);
        
        if (newAnimationData != nullptr) {
          free(newAnimationData);
        }
        size_t newAnimationDataSize = sizeof(AnimationData) + newTotalFrames * sizeof(FrameData);
        newAnimationData = (AnimationData*)malloc(newAnimationDataSize);
        if (newAnimationData == nullptr) {
          Serial.println("Memory allocation failed!");
          dataState = WAITING_FOR_SYNC; 
          return;
        }
        newAnimationData->totalFrames = newTotalFrames;
        totalBytesToReceive = newTotalFrames * sizeof(FrameData);
        bytesReceived = 0;
        currentFrameBeingReceived = 0;
        dataState = RECEIVING_DATA;
        SerialBT.write(ACK_BYTE); 
        break;
      }

      case RECEIVING_DATA:
      {
        if (bytesReceived < totalBytesToReceive) {
          size_t bytesToRead = SerialBT.available();
          size_t bytesRemaining = totalBytesToReceive - bytesReceived;
          if (bytesToRead > bytesRemaining) {
            bytesToRead = bytesRemaining;
          }

          SerialBT.readBytes(((char*)newAnimationData->frames) + bytesReceived, bytesToRead);
          bytesReceived += bytesToRead;

          if (bytesReceived % sizeof(FrameData) == 0) {
            currentFrameBeingReceived++;
            Serial.printf("Received frame %d of %d. Sent ACK.\n", currentFrameBeingReceived, newTotalFrames);
            SerialBT.write(ACK_BYTE);
          }

          if (bytesReceived == totalBytesToReceive) {
            Serial.println("All animation data received.");
            if (currentAnimation != nullptr) {
              free(currentAnimation);
            }
            currentAnimation = newAnimationData;
            newAnimationData = nullptr;
            currentFrameIndex = 0;
            frameStartTime = millis();
            dataState = WAITING_FOR_SYNC;
          }
        }
        break;
      }
    }
  }
}