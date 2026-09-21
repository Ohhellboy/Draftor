#include <Arduino.h>
#include <SPI.h>
#include "ST7306_OsptekLpm8.h"

SPIClass displaySpi(FSPI);
ST7306OsptekLpm8 lcd(displaySpi, 12, 11, 10, 9, 4);

void setup() {
  using Color = ST7306OsptekLpm8::Color;
  if (!lcd.begin()) return;
  lcd.fill(Color::White);
  lcd.fillRect(20, 20, 100, 100, Color::Black);
  lcd.fillRect(180, 20, 100, 100, Color::Red);
  lcd.fillRect(20, 180, 260, 2, Color::Black);
  lcd.fillRect(20, 180, 2, 180, Color::Black);
  lcd.fillRect(248, 328, 32, 32, Color::Red);
  (void)lcd.update();
}

void loop() {}
