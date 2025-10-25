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

// -------- Analog inputs (pots) ----------
#define POT_R      32
#define POT_G      35
#define POT_B      34
#define POT_W      33
#define POT_BRIGHT 27

// -------- Buttons ----------
#define BTN1 23
#define BTN2 22
#define BTN3 21
#define BTN4 19

// -------- Filtering / control variables ----------
static int emaR=0, emaG=0, emaB=0, emaW=0, emaBr=0;
const uint8_t HYST = 2;
const uint8_t ALPHA_NUM = 2;
const uint8_t ALPHA_DEN = 10;

int lastR=-1, lastG=-1, lastB=-1, lastW=-1, lastBr=-1;

// Track which LED strips are currently selected for control
bool stripSelected[4] = {false, false, false, false};

// --- Per-button debounce and edge detection ---
const int BTN_PINS[4] = { BTN1, BTN2, BTN3, BTN4 };
bool lastState[4] = {false, false, false, false};
unsigned long lastChange[4] = {0, 0, 0, 0};
const unsigned long DEBOUNCE_MS = 40; // faster debounce

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

// ---------- setup ----------
void setup() {
  Serial.begin(115200);

  strip1.Begin(); strip2.Begin(); strip3.Begin(); strip4.Begin();
  strip1.Show();  strip2.Show();  strip3.Show();  strip4.Show();

  for (int i = 0; i < 4; i++) {
    pinMode(BTN_PINS[i], INPUT_PULLDOWN);
  }
}

// ---------- main loop ----------
void loop() {
  unsigned long now = millis();

  // --- Fast button edge detection ---
  for (int i = 0; i < 4; i++) {
    bool reading = digitalRead(BTN_PINS[i]);

    if (reading != lastState[i] && (now - lastChange[i]) > DEBOUNCE_MS) {
      lastChange[i] = now;
      lastState[i] = reading;

      if (reading) { // rising edge (press)
        stripSelected[i] = !stripSelected[i];
        Serial.printf("Strip %d %s\n", i + 1, stripSelected[i] ? "SELECTED" : "DESELECTED");
      }
    }
  }

  // --- Pot reading and LED updates ---
  int r  = readFiltered8bitInverted(POT_R,  emaR);
  int g  = readFiltered8bitInverted(POT_G,  emaG);
  int b  = readFiltered8bitInverted(POT_B,  emaB);
  int w  = readFiltered8bitInverted(POT_W,  emaW);
  int br = readFiltered8bitInverted(POT_BRIGHT, emaBr);

  float bf = br / 255.0f;

  RgbwColor color(
    (int)(r * bf),
    (int)(g * bf),
    (int)(b * bf),
    (int)(w * bf)
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
      if (stripSelected[0]) strip1.SetPixelColor(p, color);
      if (stripSelected[1]) strip2.SetPixelColor(p, color);
      if (stripSelected[2]) strip3.SetPixelColor(p, color);
      if (stripSelected[3]) strip4.SetPixelColor(p, color);
    }

    if (stripSelected[0]) strip1.Show();
    if (stripSelected[1]) strip2.Show();
    if (stripSelected[2]) strip3.Show();
    if (stripSelected[3]) strip4.Show();

    lastR=r; lastG=g; lastB=b; lastW=w; lastBr=br;
  }

  delay(5); // ultra-smooth loop
}
