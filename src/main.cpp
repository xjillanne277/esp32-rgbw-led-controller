#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NeoPixelBus.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// -------- LED strip ----------
#define LED_PIN       13
#define LED_COUNT     10
NeoPixelBus<NeoRgbwwFeature, NeoEsp32I2s0800KbpsMethod> strip(LED_COUNT, LED_PIN);

// -------- Pins ----------
#define POT_R         32
#define POT_G         35
#define POT_B         34
#define POT_W         33
#define POT_BRIGHT    27

#define BTN1          23
#define BTN2          22
#define BTN3          21
#define BTN4          19

#define OLED_SDA      25
#define OLED_SCL      26

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

void flash(int count, RgbwwColor baseColor) {
  for (int i = 0; i < count; i++) {
    strip.SetPixelColor(0, baseColor);
    strip.Show();
    delay(150);
    strip.SetPixelColor(0, RgbwwColor(0, 0, 0, 0, 0));
    strip.Show();
    delay(150);
  }
}

// --- display helpers ---
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
  modeUntil = millis() + 1000;  // show for 1s
}

void drawMainScreen(int r, int g, int b, int w, int br) {
  display.clearDisplay();
  display.setTextSize(2);

  // Left column: RGBW
  display.setCursor(0, 0);
  display.printf("R:%3d\n", r);
  display.printf("G:%3d\n", g);
  display.printf("B:%3d\n", b);
  display.printf("W:%3d\n", w);

  // Right side: Brightness bar + value
  int barX = 90;
  int barY = 0;
  int barH = 50;
  int barW = 10;
  int barFill = map(br, 0, 255, 0, barH);

  display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);
  display.fillRect(barX, barY + (barH - barFill), barW, barFill, SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(barX - 2, barY + barH + 1);
  display.printf("%3d", br);

  display.display();
}

void setup() {
  Serial.begin(115200);
  strip.Begin();
  strip.Show();

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found!");
    for(;;);
  }

  showWelcomeScreen();

  // Buttons as INPUT_PULLDOWN (since you have real pulldowns)
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
  RgbwwColor color((int)(g * bf), (int)(r * bf), (int)(b * bf), (int)(w * bf), 0);

  // Update LEDs if changed
  bool changed =
    (lastR < 0) ||
    (abs(r - lastR) >= HYST) ||
    (abs(g - lastG) >= HYST) ||
    (abs(b - lastB) >= HYST) ||
    (abs(w - lastW) >= HYST) ||
    (abs(br - lastBr) >= HYST);

  if (changed) {
    strip.SetPixelColor(0, color);
    strip.Show();
    lastR=r; lastG=g; lastB=b; lastW=w; lastBr=br;
  }

  unsigned long now = millis();

  // --- Buttons: active HIGH (pull-downs) ---
  if (now - lastButtonPress > debounceDelay) {
    if (digitalRead(BTN1)) { showModeScreen(1); flash(1, color); lastButtonPress = now; }
    else if (digitalRead(BTN2)) { showModeScreen(2); flash(2, color); lastButtonPress = now; }
    else if (digitalRead(BTN3)) { showModeScreen(3); flash(3, color); lastButtonPress = now; }
    else if (digitalRead(BTN4)) { showModeScreen(4); flash(4, color); lastButtonPress = now; }
  }

  // --- If mode screen timeout passed, return to status display ---
  if (showingMode && millis() > modeUntil) {
    showingMode = false;
  }

  // --- Show status display (after welcome, unless currently in mode) ---
  if (!showingMode && (now - lastUiMs >= UI_PERIOD_MS)) {
    drawMainScreen(r, g, b, w, br);
    lastUiMs = now;
  }

  // If a pot was moved and we’re still on the welcome screen, switch to status display
  if (showWelcome && changed) {
    showWelcome = false;
    drawMainScreen(r, g, b, w, br);
  }

  delay(10);
}
