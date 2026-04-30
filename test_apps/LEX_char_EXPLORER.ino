#include <Wire.h>

#define MAX_ADDR 0x20

// Global variables for exploration
byte testCharCode = 0;
byte lastButtonState = 0xFC;

// --- LOW LEVEL LCD DRIVERS ---
void sendNibble(byte nibble, bool isData) {
  byte base = (nibble << 4) | (isData ? 0x01 : 0x00);
  auto write = [&](byte v) { 
    Wire.beginTransmission(MAX_ADDR); 
    Wire.write(0x02); // Port 1 Output Register
    Wire.write(v); 
    Wire.endTransmission(); 
  };
  write(base);           // Set Data/RS
  write(base | 0x04);    // Pulse Enable HIGH
  delayMicroseconds(1); 
  write(base);           // Pulse Enable LOW
  delayMicroseconds(100);
}

void sendCmd(byte cmd) { sendNibble(cmd >> 4, false); sendNibble(cmd & 0x0F, false); }
void sendDat(byte dat) { sendNibble(dat >> 4, true);  sendNibble(dat & 0x0F, true); }

void lcdPrint(const char* s) { while (*s) sendDat(*s++); }
void setCursor(byte col, byte row) { sendCmd(0x80 | ((row == 0 ? 0x00 : 0x40) + col)); }

void setup() {
  Wire.begin();
  
  // MAX7313 Config: Port 1 = Outputs (LCD), Port 2 = Inputs (Buttons)
  Wire.beginTransmission(MAX_ADDR); 
  Wire.write(0x06); Wire.write(0x00); Wire.write(0xFC); 
  Wire.endTransmission();

  // Standard 4-bit Handshake
  delay(50);
  sendNibble(0x03, 0); delay(5);
  sendNibble(0x03, 0); delay(1);
  sendNibble(0x03, 0);
  sendNibble(0x02, 0);
  sendCmd(0x28); // 2 Lines, 5x8
  sendCmd(0x0C); // Display ON, Cursor OFF
  sendCmd(0x01); // Clear
  delay(5);
}

void loop() {
  // --- READ BUTTONS ---
  Wire.beginTransmission(MAX_ADDR); Wire.write(0x00); Wire.endTransmission();
  Wire.requestFrom(MAX_ADDR, 2);
  
  if (Wire.available() >= 2) {
    Wire.read(); // Skip P1
    byte p2_val = Wire.read();

    if (p2_val != lastButtonState) {
      // Right Arrow (P2.5) -> Next Glyph
      if (!(p2_val & (1 << 5)) && (lastButtonState & (1 << 5))) {
        testCharCode++;
      }
      // Left Arrow (P2.3) -> Previous Glyph
      if (!(p2_val & (1 << 3)) && (lastButtonState & (1 << 3))) {
        testCharCode--;
      }
      lastButtonState = p2_val;
    }
  }

  // --- DRAWING LOGIC ---
  char buf[20];
  
  // Row 0: Show the character
  setCursor(0, 0);
  lcdPrint("GLYPH: [");
  sendDat(testCharCode); // This displays the character from the ROM
  lcdPrint("]      ");

  // Row 1: Show the numeric codes (Hex and Decimal)
  setCursor(0, 1);
  sprintf(buf, "0x%02X | DEC:%d   ", testCharCode, testCharCode);
  lcdPrint(buf);

  delay(50);
}
