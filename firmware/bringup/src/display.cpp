#include "display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SD.h>
#include <SPI.h>

#include "pins.h"

namespace display {

static SPIClass tftSpi(FSPI);
static Adafruit_ST7789 *tft = nullptr;
static constexpr int BL_CH = 0;

bool begin() {
  if (tft) return true;
  tftSpi.begin(PIN_TFT_SCLK, PIN_TFT_MISO, PIN_TFT_MOSI, -1);
  ledcSetup(BL_CH, 5000, 8);
  ledcAttachPin(PIN_TFT_BLK, BL_CH);
  backlight(255);
  // SD card shares the bus: keep its CS high while the display talks.
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  tft = new Adafruit_ST7789(&tftSpi, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
  tft->init(240, 320);
  tft->setSPISpeed(40000000);
  tft->setRotation(1);  // landscape 320x240
  tft->fillScreen(ST77XX_BLACK);
  return true;
}

void backlight(uint8_t level) { ledcWrite(BL_CH, level); }

void testPattern() {
  if (!tft) return;
  const uint16_t c[] = {ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE, ST77XX_WHITE, ST77XX_BLACK};
  for (uint16_t col : c) {
    tft->fillScreen(col);
    delay(300);
  }
  int w = tft->width(), h = tft->height();
  for (int i = 0; i < 8; i++) tft->fillRect(i * w / 8, 0, w / 8, h / 2, tft->color565(i * 32, 255 - i * 32, 128));
  tft->drawRect(0, 0, w, h, ST77XX_WHITE);  // edge pixels: check none are cut off
  tft->setTextColor(ST77XX_WHITE);
  tft->setTextSize(2);
  tft->setCursor(8, h / 2 + 12);
  tft->print("IoT_SYNTHPATCH");
  tft->setCursor(8, h / 2 + 36);
  tft->printf("%dx%d ST7789", w, h);
}

void status(const char *line1, const char *line2) {
  if (!tft) return;
  tft->fillRect(0, 0, tft->width(), 40, ST77XX_BLACK);
  tft->setTextSize(2);
  tft->setTextColor(ST77XX_CYAN);
  tft->setCursor(4, 2);
  tft->print(line1);
  if (line2) {
    tft->setTextColor(ST77XX_WHITE);
    tft->setCursor(4, 22);
    tft->print(line2);
  }
}

void scope(const float *v, int n, float vRange, const char *label) {
  if (!tft || n < 2) return;
  const int top = 40, w = tft->width(), h = tft->height() - top;
  static int16_t prevLo[320], prevHi[320];
  static bool havePrev = false;
  int mid = top + h / 2;
  if (!havePrev) tft->fillRect(0, top, w, h, ST77XX_BLACK);
  int lastY = mid;
  for (int x = 0; x < w && x < 320; x++) {
    int i = (long)x * (n - 1) / (w - 1);
    float f = constrain(v[i] / vRange, -1.0f, 1.0f);
    int y = mid - (int)(f * (h / 2 - 1));
    int lo = min(y, x ? lastY : y), hi = max(y, x ? lastY : y);
    if (havePrev) tft->drawFastVLine(x, prevLo[x], prevHi[x] - prevLo[x] + 1, ST77XX_BLACK);
    tft->drawPixel(x, mid, 0x4208);  // centre line
    tft->drawFastVLine(x, lo, hi - lo + 1, ST77XX_GREEN);
    prevLo[x] = lo;
    prevHi[x] = hi;
    lastY = y;
  }
  havePrev = true;
  status(label);
}

bool sdTest(Print &out) {
  begin();
  if (!SD.begin(PIN_SD_CS, tftSpi, 20000000)) {
    out.println("  SD.begin failed (no card, or not FAT32)");
    return false;
  }
  out.printf("  card type %d, size %llu MB\n", SD.cardType(), SD.cardSize() / (1024 * 1024));
  File root = SD.open("/");
  for (File f = root.openNextFile(); f; f = root.openNextFile())
    out.printf("  %s%s  %u\n", f.name(), f.isDirectory() ? "/" : "", (unsigned)f.size());
  SD.end();
  return true;
}

}  // namespace display
