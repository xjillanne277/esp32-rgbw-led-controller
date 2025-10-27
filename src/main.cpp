#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NeoPixelBus.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Two OLED displays (left and right)
TwoWire WireLeft = TwoWire(0);
TwoWire WireRight = TwoWire(1);
Adafruit_SSD1306 displayLeft(SCREEN_WIDTH, SCREEN_HEIGHT, &WireLeft, -1);
Adafruit_SSD1306 displayRight(SCREEN_WIDTH, SCREEN_HEIGHT, &WireRight, -1);

// LED strip setup
#define LED_COUNT 50
#define LED_PIN_1 18
#define LED_PIN_2 4
#define LED_PIN_3 17
#define LED_PIN_4 13

// SK6812 RGBW strips using NeoPixelBus (GRBW order)
NeoPixelBus<NeoGrbwFeature, NeoEsp32Rmt4Ws2812xMethod> strip1(LED_COUNT, LED_PIN_1);
NeoPixelBus<NeoGrbwFeature, NeoEsp32Rmt5Ws2812xMethod> strip2(LED_COUNT, LED_PIN_2);
NeoPixelBus<NeoGrbwFeature, NeoEsp32Rmt6Ws2812xMethod> strip3(LED_COUNT, LED_PIN_3);
NeoPixelBus<NeoGrbwFeature, NeoEsp32Rmt7Ws2812xMethod> strip4(LED_COUNT, LED_PIN_4);

// Analog input pins for potentiometers
#define POT_R      32
#define POT_G      35
#define POT_B      34
#define POT_W      33
#define POT_BRIGHT 27

// Button input pins
#define BTN1 23
#define BTN2 22
#define BTN3 21
#define BTN4 19

// Filter and smoothing parameters
static int emaR=0, emaG=0, emaB=0, emaW=0, emaBr=0;
const uint8_t HYST = 2;
const uint8_t ALPHA_NUM = 2;
const uint8_t ALPHA_DEN = 10;

// Last stored color values for change detection
int lastR=-1, lastG=-1, lastB=-1, lastW=-1, lastBr=-1;

// Tracks which LED strips are currently selected for control
bool stripSelected[4] = {false, false, false, false};

// Button debounce setup
const int BTN_PINS[4] = { BTN1, BTN2, BTN3, BTN4 };
bool lastState[4] = {false, false, false, false};
unsigned long lastChange[4] = {0, 0, 0, 0};
const unsigned long DEBOUNCE_MS = 40;

// Convert a 12-bit analog reading to 8-bit inverted scale (0–255)
int to8bitInv(int raw) {
  return 255 - map(raw, 0, 4095, 0, 255);
}

// Return the median of three readings for noise reduction
int median3(int a, int b, int c) {
  if (a > b) { int t=a; a=b; b=t; }
  if (b > c) { int t=b; b=c; c=t; }
  if (a > b) { int t=a; a=b; b=t; }
  return b;
}

// Read analog pin, apply median filter and exponential moving average
int readFiltered8bitInverted(int pin, int &emaState) {
  int a = analogRead(pin);
  int b = analogRead(pin);
  int c = analogRead(pin);
  int m = median3(a, b, c);
  int v8 = to8bitInv(m);
  if (emaState == 0) emaState = v8;
  emaState = (emaState * (ALPHA_DEN - ALPHA_NUM) + v8 * ALPHA_NUM) / ALPHA_DEN;
  return emaState;
}

// Update both OLED displays with color and brightness info
void updateDisplays(int r, int g, int b, int w, int br) {
  int brPct = map(br, 0, 255, 0, 100);

  // Left display: LED selection + RGBW values
  displayLeft.clearDisplay();
  displayLeft.setTextColor(SSD1306_WHITE);
  displayLeft.setTextSize(1);
  displayLeft.setCursor(0, 0);
  displayLeft.print("LED:");

  const int xStart = 30;
  const int xStep  = 18;
  const int yTop   = 0;
  const int uY     = yTop + 10;

  // Show numbers 1–4 and underline selected ones
  for (int i = 0; i < 4; i++) {
    int xNum = xStart + i * xStep;
    displayLeft.setCursor(xNum, yTop);
    displayLeft.print(i + 1);
    if (stripSelected[i]) {
      displayLeft.drawLine(xNum, uY, xNum + 6, uY, SSD1306_WHITE);
    }
  }

  // Display current RGBW values
  displayLeft.setCursor(0, 20);
  displayLeft.printf("R:%3d\nG:%3d\nB:%3d\nW:%3d", r, g, b, w);
  displayLeft.display();

  // Right display: brightness bar
  displayRight.clearDisplay();
  displayRight.setTextColor(SSD1306_WHITE);
  displayRight.setTextSize(1);
  displayRight.setCursor(0, 0);
  displayRight.print("Brightness");

  const int barH = 40;
  const int fill = map(brPct, 0, 100, 0, barH);
  const int barX = 100;
  const int barY = 10;

  displayRight.drawRect(barX, barY, 10, barH, SSD1306_WHITE);
  displayRight.fillRect(barX, barY + (barH - fill), 10, fill, SSD1306_WHITE);
  displayRight.setCursor(barX - 20, barY + barH - 4);
  displayRight.printf("%3d%%", brPct);
  displayRight.display();
}

// Apply the current pot color to all selected strips
void applyColorToSelectedStrips(RgbwColor color) {
  for (int p = 0; p < LED_COUNT; p++) {
    if (stripSelected[0]) strip1.SetPixelColor(p, color);
    if (stripSelected[1]) strip2.SetPixelColor(p, color);
    if (stripSelected[2]) strip3.SetPixelColor(p, color);
    if (stripSelected[3]) strip4.SetPixelColor(p, color);
  }
  if (stripSelected[0]) strip1.Show();
  if (stripSelected[1]) strip2.Show();
  if (stripSelected[2]) strip3.Show();
  if (stripSelected[3]) strip4.Show();
}

void setup() {
  Serial.begin(115200);

  // Initialize LED strips
  strip1.Begin(); strip2.Begin(); strip3.Begin(); strip4.Begin();
  strip1.Show();  strip2.Show();  strip3.Show();  strip4.Show();

  // Configure buttons with internal pulldown
  for (int i = 0; i < 4; i++) pinMode(BTN_PINS[i], INPUT_PULLDOWN);

  // Initialize both OLED displays
  WireLeft.begin(25, 26);   // Left OLED: SDA=25, SCL=26
  WireRight.begin(14, 16);  // Right OLED: SDA=14, SCL=16

  if (!displayLeft.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("Left OLED missing");
  if (!displayRight.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("Right OLED missing");

  displayLeft.clearDisplay(); displayLeft.display();
  displayRight.clearDisplay(); displayRight.display();
}

void loop() {
  unsigned long now = millis();
  bool buttonPressed = false;

  // Read current potentiometer values
  int r  = readFiltered8bitInverted(POT_R,  emaR);
  int g  = readFiltered8bitInverted(POT_G,  emaG);
  int b  = readFiltered8bitInverted(POT_B,  emaB);
  int w  = readFiltered8bitInverted(POT_W,  emaW);
  int br = readFiltered8bitInverted(POT_BRIGHT, emaBr);

  float bf = br / 255.0f;
  RgbwColor color((int)(r * bf), (int)(g * bf), (int)(b * bf), (int)(w * bf));

  // Check button presses and toggle strip selection
  for (int i = 0; i < 4; i++) {
    bool reading = digitalRead(BTN_PINS[i]);
    if (reading != lastState[i] && (now - lastChange[i]) > DEBOUNCE_MS) {
      lastChange[i] = now;
      lastState[i] = reading;
      if (reading) {
        stripSelected[i] = !stripSelected[i];
        buttonPressed = true;
        Serial.printf("Strip %d %s\n", i + 1, stripSelected[i] ? "SELECTED" : "DESELECTED");

        // Immediately apply current color when a strip is selected
        if (stripSelected[i]) applyColorToSelectedStrips(color);
      }
    }
  }

  // Detect changes in potentiometer values
  bool changed =
    (lastR < 0) ||
    (abs(r - lastR) >= HYST) ||
    (abs(g - lastG) >= HYST) ||
    (abs(b - lastB) >= HYST) ||
    (abs(w - lastW) >= HYST) ||
    (abs(br - lastBr) >= HYST);

  // Update LEDs if color changed
  if (changed) {
    applyColorToSelectedStrips(color);
    lastR=r; lastG=g; lastB=b; lastW=w; lastBr=br;
  }

  // Displays when color changes or button is pressed
  if (changed || buttonPressed) {
    updateDisplays(r, g, b, w, br);
  }

  delay(2);
}
