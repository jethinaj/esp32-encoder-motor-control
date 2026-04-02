#include <Arduino_GFX_Library.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// --- Motor Pin Definitions ---
#define IN1 16
#define IN2 17

// --- Display Pin Definitions ---
#define TFT_BL 27      
#define SCK_PIN 14
#define MOSI_PIN 13
#define MISO_PIN 12
#define TFT_CS 15
#define TFT_DC 2
#define TOUCH_CS 33

// Color Definitions
#define BLACK 0x0000
#define WHITE 0xFFFF
#define RED   0xF800
#define GREEN 0x07E0
#define BLUE  0x07FF  
#define GRAY  0x7BEF

// UI & Logic Variables
int speedValue = 0;   
int lastSpeedValue = 0; 
bool isOn = false;     
int activeBtn = 0; // 0=Stop, 1=FWD, 2=REV

// Layout Settings
int speedCX = 160;  
int speedCY = 220;  
int speedR  = 130;  

XPT2046_Touchscreen touch(TOUCH_CS);
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, SCK_PIN, MOSI_PIN, MISO_PIN);
Arduino_GFX *gfx = new Arduino_ILI9341(bus, -1, 1);

void setup() {
  Serial.begin(115200);

  // Motor Pins Setup
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  stopMotor();

  // Display Setup
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  gfx->begin();
  gfx->fillScreen(BLACK);

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN); 
  touch.begin();
  touch.setRotation(1);

  drawUI();
  Serial.println("System Ready: Motor + UI Integrated");
}

void loop() {
  // 1. Handle Touch Interaction
  handleTouch();

  // 2. Drive the Motor (Software PWM)
  // We only drive if the system is ON and a direction is selected
  if (isOn && speedValue > 0) {
    if (activeBtn == 1) { // Forward
      digitalWrite(IN2, LOW);
      motorPWM(IN1, speedValue);
    } 
    else if (activeBtn == 2) { // Reverse
      digitalWrite(IN1, LOW);
      motorPWM(IN2, speedValue);
    } 
    else {
      stopMotor();
    }
  } else {
    stopMotor();
  }
}

// -------- MOTOR FUNCTIONS --------

void motorPWM(int pin, int duty) {
  // Software PWM: Map 0-100 speed to 0-100 microseconds
  // Since the loop runs constantly, we do one pulse per loop iteration
  int onTime = duty; 
  int offTime = 100 - duty;

  digitalWrite(pin, HIGH);
  delayMicroseconds(onTime);
  digitalWrite(pin, LOW);
  delayMicroseconds(offTime);
}

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}

// -------- UI & TOUCH LOGIC --------

void handleTouch() {
  if (touch.touched()) {
    TS_Point p = touch.getPoint();
    int x = map(p.x, 300, 3800, 0, 320); 
    int y = map(p.y, 300, 3800, 0, 240); 

    // RPM interaction
    float dx = x - speedCX;
    float dy = y - speedCY;
    float dist = sqrt(dx * dx + dy * dy);

    if (dist > (speedR - 90) && dist < (speedR + 40) && y < speedCY) {
      float angleRad = atan2(dy, dx);
      float angleDeg = angleRad * 180.0 / 3.14159; 
      int newValue = map((int)angleDeg, -180, 0, 0, 100);
      newValue = constrain(newValue, 0, 100);

      if (newValue != speedValue) {
        lastSpeedValue = speedValue;
        speedValue = newValue;
        updateNeedle(); 
        Serial.print("Target Speed: "); Serial.println(speedValue);
      }
    }

    // Button interaction
    bool redraw = false;
    if (x > 10 && x < 100 && y > 10 && y < 50) { isOn = !isOn; redraw = true; delay(200); }
    if (x > 110 && x < 205 && y > 10 && y < 50) { activeBtn = (activeBtn == 1) ? 0 : 1; redraw = true; delay(200); }
    if (x > 215 && x < 310 && y > 10 && y < 50) { activeBtn = (activeBtn == 2) ? 0 : 2; redraw = true; delay(200); }
    
    if (redraw) drawButtons();
  }
}

// -------- DRAWING FUNCTIONS --------

void drawUI() {
  drawArc();
  drawLabels(); 
  gfx->setTextColor(WHITE); gfx->setTextSize(2); 
  gfx->setCursor(speedCX - 18, speedCY - 45); gfx->print("RPM");
  drawButtons();
  drawRPMValueText();
  drawNeedle(speedValue, RED);
}

void drawArc() {
  for (int i = 0; i <= 100; i += 2) {
    float rad = (180 + (i * 1.8)) * 0.0174533;
    uint16_t color = (i <= 60) ? GREEN : RED;
    int tickLen = (i % 10 == 0) ? 22 : 12;
    gfx->drawLine(speedCX + cos(rad) * (speedR - tickLen), speedCY + sin(rad) * (speedR - tickLen), 
                  speedCX + cos(rad) * speedR, speedCY + sin(rad) * speedR, color);
  }
}

void drawLabels() {
  gfx->setTextSize(1); gfx->setTextColor(WHITE);
  for (int i = 0; i <= 100; i += 10) {
    float rad = (180 + (i * 1.8)) * 0.0174533;
    gfx->setCursor(speedCX + cos(rad)*(speedR-40)-5, speedCY + sin(rad)*(speedR-40)-3);
    gfx->print(i);
  }
}

void drawRPMValueText() {
  gfx->fillRect(speedCX - 30, speedCY - 75, 60, 25, BLACK);
  gfx->setTextColor(BLUE); gfx->setTextSize(3);
  int offset = (speedValue < 10) ? 10 : (speedValue < 100) ? 18 : 25;
  gfx->setCursor(speedCX - offset, speedCY - 75);
  gfx->print(speedValue);
}

void updateNeedle() {
  drawNeedle(lastSpeedValue, BLACK);
  drawArc(); drawLabels();
  gfx->setTextColor(WHITE); gfx->setTextSize(2); 
  gfx->setCursor(speedCX - 18, speedCY - 45); gfx->print("RPM");
  drawRPMValueText();
  drawNeedle(speedValue, RED);
  gfx->fillCircle(speedCX, speedCY, 10, GRAY);
  gfx->drawCircle(speedCX, speedCY, 10, WHITE);
}

void drawNeedle(int value, uint16_t color) {
  float angle = 180 + (value * 1.8);
  float radTip = angle * 0.0174533;
  int xTip = speedCX + cos(radTip) * (speedR - 20);
  int yTip = speedCY + sin(radTip) * (speedR - 20);
  float radL = (angle - 5.0) * 0.0174533;
  float radR = (angle + 5.0) * 0.0174533;
  gfx->fillTriangle(xTip, yTip, speedCX + cos(radL)*12, speedCY + sin(radL)*12, speedCX, speedCY, color);
  gfx->fillTriangle(xTip, yTip, speedCX + cos(radR)*12, speedCY + sin(radR)*12, speedCX, speedCY, color);
}

void drawButtons() {
  gfx->setTextSize(2);
  // ON/OFF
  gfx->fillRect(10, 10, 90, 40, isOn ? GREEN : BLACK); 
  gfx->drawRect(10, 10, 90, 40, WHITE);
  gfx->setTextColor(isOn ? BLACK : WHITE);            
  gfx->setCursor(35, 22); gfx->print(isOn ? "ON" : "OFF");

  // FWD
  gfx->fillRect(110, 10, 95, 40, (activeBtn == 1) ? GREEN : BLACK);
  gfx->drawRect(110, 10, 95, 40, WHITE);
  gfx->setTextColor(activeBtn == 1 ? BLACK : WHITE); 
  gfx->setCursor(135, 22); gfx->print("FWD");

  // REV
  gfx->fillRect(215, 10, 95, 40, (activeBtn == 2) ? GREEN : BLACK);
  gfx->drawRect(215, 10, 95, 40, WHITE);
  gfx->setTextColor(activeBtn == 2 ? BLACK : WHITE); 
  gfx->setCursor(240, 22); gfx->print("REV");
}