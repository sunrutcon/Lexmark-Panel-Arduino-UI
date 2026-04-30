# 📟 Lexmark LCD Control Panel System
> A multi-app Arduino firmware for repurposed Lexmark printer panels.

---

## 🛠 Hardware Architecture

### I2C Address Map
*   **MAX7313 (I/O Expander):** `0x20`
*   **DS3231 (RTC Chip):** `0x68`
*   **On-board EEPROMs:** `0x50` & `0x57`

### Pin Mapping (Port 2)

| Pin | Label / Icon | Type | Function |
| :--- | :--- | :--- | :--- |
| **P2.0** | Backlight | Output | LCD Backlight (Active-Low) |
| **P2.1** | Green LED | Output | Status Indicator (Flashes on button press) |
| **P2.2** | **Red X** | Input | Reset Stopwatch / Backlight Toggle / Backspace |
| **P2.3** | **Left Arrow** | Input | Navigate Left / Decrease Value |
| **P2.4** | **Check (✔)** | Input | Select / Start-Stop / Save Setting |
| **P2.5** | **Right Arrow** | Input | Navigate Right / Increase Value |
| **P2.6** | **Curved Arrow** | Input | **Global Home:** Return to Menu from any App |
| **P2.7** | Outline Arrow | Input | Unassigned |

---

## 🚀 Built-in Applications

### 1. Dashboard 
*   **View 0:** Large HH:MM:SS clock and DD.MM.YYYY date.
*   **View 1:** Time/Date + Live Temperature from RTC sensor.
*   *Navigate views using Left/Right arrows.*

### 2. Time & Date Setup
*   **Blinking UI:** The value currently being edited will flash.
*   **Logic:** Press **Check** to cycle through segments (e.g., Hours -> Minutes).
*   **Saving:** Final press of **Check** writes new data to the DS3231 hardware.

### 3. Stopwatch ⏱️
*   **Format:** `HH:MM:SS.d` (Tenths of seconds included).
*   **Controls:** Start/Stop with **Check**, Reset with **Red X** (when paused).
*   **Background Indicator:** A blinking lowercase **'s'** appears at `(15,0)` on all screens if the timer is active.

### 4. Text Writer ✍️
*   **Alphabet:** `A-Z`, `0-9`, and punctuation.
*   **Navigation:** Cycle letters with Arrows, move cursor with **Check**, delete with **Red X**.

### 5. Character Explorer 🔍
*   **Purpose:** Browse all 256 internal LCD glyphs.
*   **Display:** Shows actual glyph and its corresponding Hexadecimal code.

---

## ⚙️ System Specifications
*   **Display Method:** Direct-Write (I2C) with hardware refresh every 30ms.
*   **Custom Graphics:** Degree symbol (`°`) generated in CGRAM slot 0.
*   **Stability:** Active-Low pull-up logic for all buttons via MAX7313.
*   **Timekeeping:** Battery-backed DS3231 maintains accuracy during power loss.
