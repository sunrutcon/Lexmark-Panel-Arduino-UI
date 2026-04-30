#include <Wire.h>
#include "RTClib.h"

#define MAX_ADDR 0x20
RTC_DS3231 rtc;

// --- THE 8 ORIGINAL BUILDING BLOCKS (0-7) ---
byte customChars[8][8] = {
  {0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00}, // 0: Upper Bar
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F}, // 1: Lower Bar
  {0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x1F, 0x1F}, // 2: Middle Bar
  {0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}, // 3: Full Block
  {0x07, 0x0F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}, // 4: Top-Left Corner
  {0x1C, 0x1E, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}, // 5: Top-Right Corner
  {0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x0F, 0x07}, // 6: Bottom-Left Corner
  {0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1E, 0x1C}  // 7: Bottom-Right Corner
};

// Map: {TopLeft, TopMid, TopRight, BotLeft, BotMid, BotRight}
const byte digitMap[10][6] = {
  {4, 0, 5, 6, 1, 7}, // 0
  {32, 3, 32, 32, 3, 32}, // 1
  {0, 2, 5, 6, 1, 1}, // 2
  {0, 2, 5, 1, 1, 7}, // 3
  {3, 1, 3, 32, 32, 3}, // 4
  {4, 2, 0, 1, 1, 7}, // 5
  {4, 2, 0, 6, 1, 7}, // 6
  {0, 0, 3, 32, 32, 3}, // 7
  {4, 2, 5, 6, 1, 7}, // 8
  {4, 2, 5, 1, 1, 7}  // 9
};

void sendNibble(byte nibble, bool isData) {
  byte base = (nibble << 4) | (isData ? 0x01 : 0x00);
  auto write = [&](byte v) { Wire.beginTransmission(MAX_ADDR); Wire.write(0x02); Wire.write(v); Wire.endTransmission(); };
  write(base); write(base | 0x04); delayMicroseconds(1); write(base); delayMicroseconds(100);
}
void sendCmd(byte cmd) { sendNibble(cmd >> 4, false); sendNibble(cmd & 0x0F, false); }
void sendDat(byte dat) { sendNibble(dat >> 4, true);  sendNibble(dat & 0x0F, true); }
void setCursor(byte col, byte row) { sendCmd(0x80 | ((row == 0 ? 0x00 : 0x40) + col)); }

void createChar(byte loc, byte charmap[]) {
  sendCmd(0x40 | (loc << 3));
  for (int i = 0; i < 8; i++) sendDat(charmap[i]);
}

void drawDigit(int num, int col) {
  setCursor(col, 0); 
  for (int i = 0; i < 3; i++) sendDat(digitMap[num][i] == 32 ? ' ' : digitMap[num][i]);
  setCursor(col, 1); 
  for (int i = 3; i < 6; i++) sendDat(digitMap[num][i] == 32 ? ' ' : digitMap[num][i]);
}

void setup() {
  Wire.begin(); rtc.begin();
  Wire.beginTransmission(MAX_ADDR); Wire.write(0x06); Wire.write(0x00); Wire.write(0xFC); Wire.endTransmission();
  delay(50); sendNibble(0x03, 0); delay(5); sendNibble(0x03, 0); delay(1); sendNibble(0x03, 0);
  sendNibble(0x02, 0); sendCmd(0x28); sendCmd(0x0C); sendCmd(0x01); delay(5);
  for (int i = 0; i < 8; i++) { createChar(i, customChars[i]); delay(10); }
}

void loop() {
  DateTime now = rtc.now();
  bool blink = (millis() % 1000 < 500);

  // Hour Tens (Col 0-2) and Units (Col 4-6)
  drawDigit(now.hour() / 10, 0); 
  drawDigit(now.hour() % 10, 4);

  // Column 7 is left empty as a space
  setCursor(7, 0); sendDat(' ');
  setCursor(7, 1); sendDat(' ');

  // Colon using standard period at Column 8 (offsetting for its left-leaning position)
  setCursor(8, 0); 
  if (blink) sendDat('.'); else sendDat(' ');
  setCursor(8, 1); 
  if (blink) sendDat('.'); else sendDat(' ');

  // Minute Tens (Col 9-11) and Units (Col 13-15)
  drawDigit(now.minute() / 10, 9); 
  drawDigit(now.minute() % 10, 13);

  delay(200); 
}
