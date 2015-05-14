// NoSpark - OpenEVSE charger firmware
// Copyright (C) 2015 Andre Eisenbach
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// See LICENSE for a copy of the GNU General Public License or see
// it online at <http://www.gnu.org/licenses/>.

#include "customcharacters.h"

#define NUM_CUSTOM_CHARS 8

using devices::LCD16x2;

namespace
{
    const PROGMEM uint8_t MAP[NUM_CUSTOM_CHARS][8] = {
        {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04} // SEPARATOR
      , {0x00, 0x0e, 0x15, 0x17, 0x11, 0x0e, 0x00, 0x00} // CLOCK
      , {0x00, 0x1f, 0x1f, 0x11, 0x11, 0x11, 0x1e, 0x00} // CALENDAR
      , {0x07, 0x0e, 0x0c, 0x1f, 0x03, 0x06, 0x0c, 0x08} // BOLT
      , {0x1f, 0x11, 0x0a, 0x0a, 0x04, 0x0a, 0x1f, 0x1f} // HOURGLASS
      , {0x18, 0x08, 0x13, 0x19, 0x02, 0x03, 0x00, 0x00} // ZZ
      , {0x0e, 0x1f, 0x11, 0x11, 0x11, 0x1f, 0x1f, 0x00} // BATTERY1
      , {0x07, 0x08, 0x1e, 0x08, 0x1e, 0x08, 0x07, 0x00} // EURO
    };

    const PROGMEM uint8_t CUSTOM_SEGMENTS[NUM_CUSTOM_CHARS][8] = {
        {0x1c, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1c} // 0 Left bar
      , {0x07, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x07} // 1 Right bar
      , {0x1f, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} // 2 Top bar
      , {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x1f} // 3 Bottom bar
      , {0x1f, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x1f} // 4 Both bars
      , {0x0f, 0x07, 0x00, 0x00, 0x00, 0x00, 0x03, 0x07} // 5 "2" top left
      , {0x1e, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x18, 0x1c} // 6 "5" top right
      , {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x0f} // 7 "5" bottom left
    };

    const PROGMEM uint8_t LARGE_DIGITS[10][6] = {
        {0x01, 0x02, 0x00, 0x01, 0x03, 0x00}
      , {0x20, 0x20, 0x00, 0x20, 0x20, 0x00}
      , {0x05, 0x04, 0x00, 0x01, 0x03, 0x03}
      , {0x05, 0x04, 0x00, 0x07, 0x03, 0x00}
      , {0x01, 0x03, 0x00, 0x20, 0x20, 0x00}
      , {0x01, 0x04, 0x06, 0x07, 0x03, 0x00}
      , {0x01, 0x03, 0x20, 0x01, 0x03, 0x00}
      , {0x01, 0x02, 0x00, 0x20, 0x20, 0x00}
      , {0x01, 0x04, 0x00, 0x01, 0x03, 0x00}
      , {0x01, 0x04, 0x00, 0x20, 0x20, 0x00}
    };

    void loadCustomCharacters(LCD16x2 &lcd, const uint8_t p[][8])
    {
        for (uint8_t i=0; i != NUM_CUSTOM_CHARS; ++i)
            lcd.createChar_P(i, &p[i][0], 8);
    }
}

namespace ui
{

void CustomCharacters::loadCustomChars(LCD16x2 &lcd)
{
    loadCustomCharacters(lcd, MAP);
}

void CustomCharacters::loadLargeDigits(LCD16x2 &lcd)
{
    loadCustomCharacters(lcd, CUSTOM_SEGMENTS);
}

void CustomCharacters::largeDigit(LCD16x2 &lcd, const uint8_t digit, const uint8_t offset)
{
    const uint8_t *p = LARGE_DIGITS[digit];

    lcd.move(offset, 0);
    for (uint8_t i = 0; i != 3; ++i)
        lcd.write(pgm_read_byte(p+i));

    lcd.move(offset, 1);
    for (uint8_t i = 0; i != 3; ++i)
        lcd.write(pgm_read_byte(p+i+3));}
}
