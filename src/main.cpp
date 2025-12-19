#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <BluetoothSerial.h>
#include <Preferences.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif

// --- 宏定义，请根据你的电路板修改 ---
#define LED_PIN          18       // 控制WS2812B灯珠的引脚
#define NUM_LEDS         64       // 灯珠总数 (8x8矩阵)
#define BRIGHTNESS_PIN_A 32       // 20% 亮度电容触摸引脚
#define BRIGHTNESS_PIN_B 33       // 40% 亮度电容触摸引脚
#define BRIGHTNESS_PIN_C 25       // 60% 亮度电容触摸引脚
#define BRIGHTNESS_PIN_D 26       // 80% 亮度电容触摸引脚
#define DEFAULT_BRIGHTNESS_PERCENT 100 // 默认亮度百分比

// --- 新的协议常量 ---
#define SYNC_BYTE 0xAA      // 同步字节，数据包开始标记
#define ACK_BYTE 0xBB       // 确认字节，用于回复电脑

// --- 全局变量和对象 ---
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
BluetoothSerial SerialBT;
Preferences preferences;
uint8_t globalBrightness = DEFAULT_BRIGHTNESS_PERCENT * 2.55;

// --- 数据结构定义 ---
struct CRGB {
  uint8_t g;
  uint8_t r;
  uint8_t b;
};

struct FrameData {
  float duration; // 帧时长，单位秒
  CRGB pixels[NUM_LEDS];
};

struct AnimationData {
  unsigned int totalFrames;
  FrameData frames[0];
};
AnimationData* currentAnimation = nullptr;
unsigned long frameStartTime = 0;
unsigned int currentFrameIndex = 0;

// --- 蓝牙接收状态机变量 ---
enum DataState { WAITING_FOR_SYNC, WAITING_FOR_FRAME_COUNT, RECEIVING_DATA };
DataState dataState = WAITING_FOR_SYNC;
unsigned int newTotalFrames = 0;
unsigned int currentFrameBeingReceived = 0;
AnimationData* newAnimationData = nullptr;

// --- 函数声明 ---
void saveAnimationToFlash();
void loadAnimationFromFlash();
void playAnimation();
void readTouchSensors();
void handleBluetoothData();
void createDefaultAnimation();

// --- setup() 函数 ---
void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 LED Matrix Starting...");
  strip.begin();
  strip.setBrightness(globalBrightness);
  strip.show();
  SerialBT.begin("ESP32 LED Matrix");
  Serial.println("Bluetooth started, device name: ESP32 LED Matrix");
  preferences.begin("animation", false);
  touchAttachInterrupt(BRIGHTNESS_PIN_A, NULL, 0);
  touchAttachInterrupt(BRIGHTNESS_PIN_B, NULL, 0);
  touchAttachInterrupt(BRIGHTNESS_PIN_C, NULL, 0);
  touchAttachInterrupt(BRIGHTNESS_PIN_D, NULL, 0);

  loadAnimationFromFlash();

  if (currentAnimation == nullptr || currentAnimation->totalFrames == 0) {
    Serial.println("No valid animation data found. Creating default animation...");
    createDefaultAnimation();
    saveAnimationToFlash();
  }
  strip.setBrightness(globalBrightness);
}

// --- loop() 函数 ---
void loop() {
  playAnimation();
  readTouchSensors();
  handleBluetoothData();
}

// --- 函数实现 ---

/**
 * @brief 创建一个默认的流水灯动画
 */
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

/**
 * @brief 将当前动画数据保存到闪存
 */
void saveAnimationToFlash() {
  if (currentAnimation == nullptr) return;
  size_t dataSize = sizeof(AnimationData) + currentAnimation->totalFrames * sizeof(FrameData);
  preferences.putBytes("animation_data", (const uint8_t*)currentAnimation, dataSize);
  Serial.println("Animation data saved to flash.");
}

/**
 * @brief 从闪存加载动画数据 (已优化，添加了数据完整性检查)
 */
void loadAnimationFromFlash() {
  size_t dataSize = preferences.getBytesLength("animation_data");
  
  if (dataSize > sizeof(unsigned int)) {
    Serial.printf("Found %d bytes of data in flash. Attempting to load...\n", dataSize);

    unsigned int storedTotalFrames;
    preferences.getBytes("animation_data", &storedTotalFrames, sizeof(unsigned int));

    size_t expectedSize = sizeof(unsigned int) + storedTotalFrames * sizeof(FrameData);

    if (dataSize == expectedSize) {
      if (currentAnimation != nullptr) {
        free(currentAnimation);
      }
      currentAnimation = (AnimationData*)malloc(dataSize);
      if (currentAnimation == nullptr) {
        Serial.println("Memory allocation for stored animation failed!");
        return;
      }
      preferences.getBytes("animation_data", (uint8_t*)currentAnimation, dataSize);
      Serial.printf("Animation data loaded from flash. Total frames: %d\n", currentAnimation->totalFrames);
      currentFrameIndex = 0;
      return;
    } else {
      Serial.printf("Error: Data size mismatch. Stored: %d bytes, Expected: %d bytes. Assuming corruption.\n", dataSize, expectedSize);
    }
  } else {
    Serial.println("No or invalid data found in flash. Length is 0 or too small.");
  }
  
  if (currentAnimation != nullptr) {
    free(currentAnimation);
    currentAnimation = nullptr;
  }
}

/**
 * @brief 持续播放存储在闪存中的动画
 */
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

/**
 * @brief 读取电容触摸引脚，并调整亮度
 */
void readTouchSensors() {
  int touchValueA = touchRead(BRIGHTNESS_PIN_A);
  int touchValueB = touchRead(BRIGHTNESS_PIN_B);
  int touchValueC = touchRead(BRIGHTNESS_PIN_C);
  int touchValueD = touchRead(BRIGHTNESS_PIN_D);
  int touchThreshold = 30;

  if (touchValueA < touchThreshold) {
    globalBrightness = 20 * 2.55;
  } else if (touchValueB < touchThreshold) {
    globalBrightness = 40 * 2.55;
  } else if (touchValueC < touchThreshold) {
    globalBrightness = 60 * 2.55;
  } else if (touchValueD < touchThreshold) {
    globalBrightness = 80 * 2.55;
  } else {
    globalBrightness = 100 * 2.55;
  }
  strip.setBrightness(globalBrightness);
}

/**
 * @brief 新的蓝牙数据处理函数，采用分块接收和确认机制 (已优化)
 */
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
      { // 添加大括号创建独立作用域
        // 在尝试读取数据之前，强制等待直到有4个字节可用
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
      } // 大括号结束

      case RECEIVING_DATA:
      { // 添加大括号创建独立作用域
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
            saveAnimationToFlash();
            currentFrameIndex = 0;
            frameStartTime = millis();
            dataState = WAITING_FOR_SYNC;
          }
        }
        break;
      } // 大括号结束
    }
  }
}