#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NeoPixelBus.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// -------- LED strips ----------
#define LED_COUNT 50

#define LED_PIN_1 18
#define LED_PIN_2 4
#define LED_PIN_3 17
#define LED_PIN_4 13

// SK6812 RGBW strips (GRBW order)
NeoPixelBus<NeoGrbwFeature, NeoEsp32Rmt4Ws2812xMethod> strip1(LED_COUNT, LED_PIN_1);
NeoPixelBus<NeoGrbwFeature, NeoEsp32Rmt5Ws2812xMethod> strip2(LED_COUNT, LED_PIN_2);
NeoPixelBus<NeoGrbwFeature, NeoEsp32Rmt6Ws2812xMethod> strip3(LED_COUNT, LED_PIN_3);
NeoPixelBus<NeoGrbwFeature, NeoEsp32Rmt7Ws2812xMethod> strip4(LED_COUNT, LED_PIN_4);

// -------- Pins ----------
#define POT_R      32
#define POT_G      35
#define POT_B      34
#define POT_W      33
#define POT_BRIGHT 27

#define BTN1       23
#define BTN2       22
#define BTN3       21
#define BTN4       19

#define OLED_SDA   25
#define OLED_SCL   26

// -------- Filtering / display control ----------
static int emaR=0, emaG=0, emaB=0, emaW=0, emaBr=0;
const uint8_t HYST = 2;
const uint8_t ALPHA_NUM = 2;
const uint8_t ALPHA_DEN = 10;
unsigned long lastUiMs = 0;
const uint16_t UI_PERIOD_MS = 60;

int lastR=-1, lastG=-1, lastB=-1, lastW=-1, lastBr=-1;
bool showWelcome = true;
bool showingMode = false;
unsigned long modeUntil = 0;

// Button debounce
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 200;

// ---------- helpers ----------
int to8bitInv(int raw) {
  return 255 - map(raw, 0, 4095, 0, 255);
}

int median3(int a, int b, int c) {
  if (a > b) { int t=a; a=b; b=t; }
  if (b > c) { int t=b; b=c; c=t; }
  if (a > b) { int t=a; a=b; b=t; }
  return b;
}

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

void flash(int count, RgbwColor baseColor) {
  for (int i = 0; i < count; i++) {
    for (int p = 0; p < LED_COUNT; p++) {
      strip1.SetPixelColor(p, baseColor);
      strip2.SetPixelColor(p, baseColor);
      strip3.SetPixelColor(p, baseColor);
      strip4.SetPixelColor(p, baseColor);
    }
    strip1.Show(); strip2.Show(); strip3.Show(); strip4.Show();
    delay(150);
    for (int p = 0; p < LED_COUNT; p++) {
      RgbwColor off(0,0,0,0);
      strip1.SetPixelColor(p, off);
      strip2.SetPixelColor(p, off);
      strip3.SetPixelColor(p, off);
      strip4.SetPixelColor(p, off);
    }
    strip1.Show(); strip2.Show(); strip3.Show(); strip4.Show();
    delay(150);
  }
}

// --- display helpers (commented out for now) ---
/*
void showWelcomeScreen() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  display.println("RGBW");
  display.setCursor(20, 20);
  display.println("LED");
  display.setCursor(0, 40);
  display.println("Controller");
  display.display();
}

void showModeScreen(int mode) {
  showingMode = true;
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(25, 20);
  display.printf("MODE %d", mode);
  display.display();
  modeUntil = millis() + 1000;
}

void drawMainScreen(int r, int g, int b, int w, int br) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.printf("R:%3d\n", r);
  display.printf("G:%3d\n", g);
  display.printf("B:%3d\n", b);
  display.printf("W:%3d\n", w);
  display.display();
}
*/

void setup() {
  Serial.begin(115200);

  // Initialize all LED strips
  strip1.Begin(); strip2.Begin(); strip3.Begin(); strip4.Begin();
  strip1.Show();  strip2.Show();  strip3.Show();  strip4.Show();
  delay(50);

  // Commented out OLED for now
  /*
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found!");
    for(;;);
  }
  showWelcomeScreen();
  */

  // Buttons
  pinMode(BTN1, INPUT_PULLDOWN);
  pinMode(BTN2, INPUT_PULLDOWN);
  pinMode(BTN3, INPUT_PULLDOWN);
  pinMode(BTN4, INPUT_PULLDOWN);
}

void loop() {
  int r  = readFiltered8bitInverted(POT_R,  emaR);
  int g  = readFiltered8bitInverted(POT_G,  emaG);
  int b  = readFiltered8bitInverted(POT_B,  emaB);
  int w  = readFiltered8bitInverted(POT_W,  emaW);
  int br = readFiltered8bitInverted(POT_BRIGHT, emaBr);

  float bf = br / 255.0f;

  // ✅ Corrected RGBW mapping
  RgbwColor color(
    (int)(r * bf),  // red
    (int)(g * bf),  // green
    (int)(b * bf),  // blue
    (int)(w * bf)   // white
  );

  bool changed =
    (lastR < 0) ||
    (abs(r - lastR) >= HYST) ||
    (abs(g - lastG) >= HYST) ||
    (abs(b - lastB) >= HYST) ||
    (abs(w - lastW) >= HYST) ||
    (abs(br - lastBr) >= HYST);

  if (changed) {
    for (int p = 0; p < LED_COUNT; p++) {
      strip1.SetPixelColor(p, color);
      strip2.SetPixelColor(p, color);
      strip3.SetPixelColor(p, color);
      strip4.SetPixelColor(p, color);
    }
    strip1.Show(); strip2.Show(); strip3.Show(); strip4.Show();
    lastR=r; lastG=g; lastB=b; lastW=w; lastBr=br;
  }

  unsigned long now = millis();

  if (now - lastButtonPress > debounceDelay) {
    if (digitalRead(BTN1)) { flash(1, color); lastButtonPress = now; }
    else if (digitalRead(BTN2)) { flash(2, color); lastButtonPress = now; }
    else if (digitalRead(BTN3)) { flash(3, color); lastButtonPress = now; }
    else if (digitalRead(BTN4)) { flash(4, color); lastButtonPress = now; }
  }

  delay(10);
}
