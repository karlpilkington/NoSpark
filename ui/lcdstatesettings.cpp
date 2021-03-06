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

#include <util/delay.h>
#include <stdlib.h>

#include "devices/ds3231.h"
#include "event/loop.h"
#include "evse/state.h"
#include "system/timer.h"
#include "system/watchdog.h"
#include "utils/bcd.h"
#include "utils/math.h"
#include "customcharacters.h"
#include "events.h"
#include "lcdstatesettings.h"
#include "strings.h"

#define SETTINGS_TIMEOUT    10000
#define BLINK_TIMEOUT       500

#define KWH_LIMIT_MAX       15

#define UNINITIALIZED       0xFF

#define NOT_ADJUSTING       0x00
#define ADJUST_SINGLE       0x01

#define ADJUST_HH           0x01
#define ADJUST_MM           0x02

#define ADJUST_DD           0x01
#define ADJUST_MM           0x02
#define ADJUST_YY           0x03

#define ADJUST_TIMER_ONOFF  0x01
#define ADJUST_T1_HH        0x02
#define ADJUST_T1_MM        0x03
#define ADJUST_T2_HH        0x04
#define ADJUST_T2_MM        0x05

#define ADJUST_CURRENCY     0x01
#define ADJUST_ONES         0x02
#define ADJUST_TENTH        0x03
#define ADJUST_HUNDREDTH    0x04

using devices::DS3231;
using devices::LCD16x2;
using evse::EepromSettings;
using evse::State;
using stream::LcdStream;
using stream::PGM;
using system::Timer;
using system::Watchdog;

namespace
{
    static uint8_t temp_buffer[8] = {0};

    void write_time(stream::LcdStream &lcd, uint8_t hh, uint8_t mm)
    {
        lcd << stream::PAD_ZERO <<  hh << ':' << stream::PAD_ZERO << mm;
    }
}

namespace ui
{

LcdStateSettings::LcdStateSettings(stream::LcdStream &lcd)
    : LcdState(lcd)
    , page(0)
    , option(NOT_ADJUSTING)
    , value(UNINITIALIZED)
    , last_action(0)
    , blink_state(BLINK_TIMEOUT)
    , pageHandlers {
        &LcdStateSettings::pageSetCurrent
      , &LcdStateSettings::pageChargeTimer
      , &LcdStateSettings::pageKwhLimit
      , &LcdStateSettings::pageSetTime
      , &LcdStateSettings::pageSetDate
      , &LcdStateSettings::pageKwhCost
      , &LcdStateSettings::pageSleepmode
      , &LcdStateSettings::pageReset
      , &LcdStateSettings::pageExit
    }
{
    CustomCharacters::loadCustomChars(lcd.getLCD());
    EepromSettings::load(settings);
}

bool LcdStateSettings::draw()
{
    if (timedOut())
        return false;

    lcd.setBacklight(LCD16x2::WHITE);

    if (page >= SETTINGS_PAGES)
        page = 0;

    return (this->*pageHandlers[page])();
}

bool LcdStateSettings::pageSetTime()
{
    lcd.move(0,0);
    lcd << static_cast<char>(CustomCharacters::CLOCK);
    lcd << PGM << STR_SET_CLOCK;
    lcd.move(2, 1);

    if (option > ADJUST_MM)
    {
        DS3231::get().writeRaw(temp_buffer, 8);
        option = NOT_ADJUSTING;
    }

    if (option == NOT_ADJUSTING)
    {
        temp_buffer[0] = 0;
        DS3231::get().readRaw(temp_buffer, 8);
    }

    if (value == UNINITIALIZED)
        value = 0;

    const uint8_t hh = utils::bcd2dec(temp_buffer[3]);
    if (option == ADJUST_HH)
        temp_buffer[3] = utils::dec2bcd((hh + value) % 24);

    const uint8_t mm = utils::bcd2dec(temp_buffer[2]);
    if (option == ADJUST_MM)
        temp_buffer[2] = utils::dec2bcd((mm + value) % 60);

    lcd << stream::PAD_BCD << temp_buffer[3] << ':';
    lcd << stream::PAD_BCD << temp_buffer[2];

    if (option != NOT_ADJUSTING && !blink_state.get())
    {
        const uint8_t offset[2] = {2, 5};
        lcd.move(offset[option - 1], 1);
        lcd << "  ";
    }

    value = 0;
    return true;
}

bool LcdStateSettings::pageSetDate()
{
    lcd.move(0,0);
    lcd << static_cast<char>(CustomCharacters::CALENDAR);
    lcd << PGM << STR_SET_DATE;
    lcd.move(2, 1);

    if (option > ADJUST_YY)
    {
        DS3231::get().writeRaw(temp_buffer, 8);
        option = NOT_ADJUSTING;
    }

    if (option == NOT_ADJUSTING)
    {
        temp_buffer[0] = 0;
        DS3231::get().readRaw(temp_buffer, 8);
    }

    if (value == UNINITIALIZED)
        value = 0;

    const uint8_t dd = utils::bcd2dec(temp_buffer[5]);
    if (option == ADJUST_DD)
        temp_buffer[5] = utils::dec2bcd(utils::max((dd + value) % 32, 1));

    const uint8_t mm = utils::bcd2dec(temp_buffer[6]);
    if (option == ADJUST_MM)
        temp_buffer[6] = utils::dec2bcd(utils::max((mm + value) % 13, 1));

    const uint8_t yy = utils::bcd2dec(temp_buffer[7]);
    if (option == ADJUST_YY)
        temp_buffer[7] = utils::dec2bcd((yy + value) % 30); // <-- Year 2030 issue :)

    lcd << stream::PAD_BCD << temp_buffer[5] << '.';
    lcd << stream::PAD_BCD << temp_buffer[6] << '.';
    lcd << "20" << stream::PAD_BCD << temp_buffer[7];

    if (option != NOT_ADJUSTING && !blink_state.get())
    {
        const uint8_t offset[3] = {2, 5, 8};
        lcd.move(offset[option - 1], 1);
        lcd << stream::Spaces(option == ADJUST_YY ? 4 : 2);
    }

    value = 0;
    return true;
}

bool LcdStateSettings::pageSetCurrent()
{
    const uint8_t currents[] = {10, 16, 20, 24, 30, 35, 40, 45, 50};

    // Initialize amps
    if (value == UNINITIALIZED)
        value = State::get().max_amps_target;

    // Save new state if we're done adjusting
    if (option > ADJUST_SINGLE)
    {
        if (value != State::get().max_amps_target)
        {
            settings.max_current = value;
            EepromSettings::save(settings);

            State::get().max_amps_target = value;
            event::Loop::post(event::Event(EVENT_MAX_AMPS_CHANGED, value));
        }
        option = NOT_ADJUSTING;
    }

    // Snap to currents above if we're still adjusting
    if (option == ADJUST_SINGLE)
    {
        if (value > currents[sizeof(currents) - 1])
            value = currents[0];

        // Snap
        uint8_t i = 0;
        while (currents[i] < value)
            ++i;
        value = currents[i];
    }

    // Draw screen, flashing value while adjusting

    lcd.move(0, 0);
    lcd << static_cast<char>(CustomCharacters::BOLT);
    lcd << PGM << STR_SET_CURRENT;

    lcd.move(2, 1);
    if (option == NOT_ADJUSTING || blink_state.get())
        lcd << value << "A ";
    else
        lcd << stream::Spaces(5);

    return true;
}

bool LcdStateSettings::pageChargeTimer()
{
    if ((option > ADJUST_T2_MM)
     || (option > ADJUST_TIMER_ONOFF && temp_buffer[0] == 0))
    {
        settings.charge_timer_enabled = temp_buffer[0];
        settings.charge_timer_t1 = (temp_buffer[1] << 8) | temp_buffer[2];
        settings.charge_timer_t2 = (temp_buffer[3] << 8) | temp_buffer[4];;
        EepromSettings::save(settings);

        option = NOT_ADJUSTING;
    }

    if (option == NOT_ADJUSTING)
    {
        // TODO: Unpack from EEPROM
        temp_buffer[0] = settings.charge_timer_enabled;
        temp_buffer[1] = settings.charge_timer_t1 >> 8;
        temp_buffer[2] = settings.charge_timer_t1 & 0xFF;
        temp_buffer[3] = settings.charge_timer_t2 >> 8;
        temp_buffer[4] = settings.charge_timer_t2 & 0xFF;
    }

    if (value == UNINITIALIZED)
        value = 0;

    lcd.move(0,0);
    lcd << static_cast<char>(CustomCharacters::HOURGLASS);
    lcd << PGM << STR_SET_CHARGETIMER;

    lcd.move(2, 1);

    if (option == ADJUST_TIMER_ONOFF)
        temp_buffer[0] = (temp_buffer[0] + value) % 2;

    if (option == ADJUST_T1_HH)
        temp_buffer[1] = (temp_buffer[1] + value) % 24;

    if (option == ADJUST_T1_MM)
        temp_buffer[2] = (temp_buffer[2] + value * 15) % 60;

    if (option == ADJUST_T2_HH)
        temp_buffer[3] = (temp_buffer[3] + value) % 24;

    if (option == ADJUST_T2_MM)
        temp_buffer[4] = (temp_buffer[4] + value * 15) % 60;

    value = 0;

    if (temp_buffer[0] == 0)
    {
        if (option == NOT_ADJUSTING || blink_state.get())
            lcd << PGM << STR_OFF << stream::Spaces(12);
        else
            lcd << stream::Spaces(14);

    } else {
        lcd << PGM << STR_ON << ' ';

        write_time(lcd, temp_buffer[1], temp_buffer[2]);
        lcd << static_cast<char>(126);
        write_time(lcd, temp_buffer[3], temp_buffer[4]);

        if (blink_state.get())
        {
            const uint8_t offset[5] = {2, 5, 8, 11, 14};
            lcd.move(offset[option - 1], 1);
            lcd << "  ";
        }
    }

    return true;
}

bool LcdStateSettings::pageKwhLimit()
{
    if (value == UNINITIALIZED)
        value = settings.kwh_limit;

    // Save new state if we're done adjusting
    if (option > ADJUST_SINGLE)
    {
        settings.kwh_limit = value;
        EepromSettings::save(settings);
        option = NOT_ADJUSTING;
    }

    if (option == ADJUST_SINGLE)
    {
        if (value > KWH_LIMIT_MAX)
            value = 0;
    }

    // Draw screen, flashing value while adjusting

    lcd.move(0, 0);
    lcd << static_cast<char>(CustomCharacters::BATTERY1);
    lcd << PGM << STR_SET_KWH_LIMIT;

    lcd.move(2, 1);
    if (option == NOT_ADJUSTING || blink_state.get())
    {
        if (value == 0)
            lcd << PGM << STR_OFF;
        else
            lcd << value << " kWh  ";
    } else {
        lcd << stream::Spaces(13);
    }

    return true;
}

bool LcdStateSettings::pageKwhCost()
{
    const char currencies[3] = {'$', CustomCharacters::EURO, '\\'}; // Backslash = Yen

    if (value == UNINITIALIZED)
        value = 0;

    if (option > ADJUST_HUNDREDTH)
    {
        EepromSettings::save(settings);
        option = NOT_ADJUSTING;
    }

    uint16_t hundredth = settings.kwh_cost;
    uint16_t ones = hundredth / 100;
    hundredth -= ones * 100;
    uint16_t tenth = hundredth / 10;
    hundredth -= tenth * 10;

    if (option == ADJUST_CURRENCY)
        settings.kwh_currency = (settings.kwh_currency + value) % 3;

    if (option == ADJUST_ONES)
        ones = (ones + value) % 10;

    if (option == ADJUST_TENTH)
        tenth = (tenth + value) % 10;

    if (option == ADJUST_HUNDREDTH)
        hundredth = (hundredth + value) % 10;

    settings.kwh_cost = hundredth + (tenth * 10) + (ones * 100);

    // Draw screen, flashing value while adjusting

    lcd.move(0, 0);
    lcd << '$' << PGM << STR_SET_KWH_COST;

    lcd.move(2, 1);
    lcd << static_cast<char>(currencies[settings.kwh_currency]) << ' ';
    lcd << static_cast<char>('0' + ones) << '.';
    lcd << static_cast<char>('0' + tenth);
    lcd << static_cast<char>('0' + hundredth);

    if (option != NOT_ADJUSTING && !blink_state.get())
    {
        const uint8_t offset[4] = {2, 4, 6, 7};
        lcd.move(offset[option - 1], 1);
        lcd << " ";
    }

    value = 0;
    return true;
}

bool LcdStateSettings::pageSleepmode()
{
    if (value == UNINITIALIZED)
        value = settings.sleep_mode;

    // Save new state if we're done adjusting
    if (option > ADJUST_SINGLE)
    {
        settings.sleep_mode = value;
        EepromSettings::save(settings);
        option = NOT_ADJUSTING;
    }

    // Draw screen, flashing value while adjusting

    lcd.move(0, 0);
    lcd << static_cast<char>(CustomCharacters::ZZ);
    lcd << PGM << STR_SET_SLEEPMODE;

    lcd.move(2, 1);
    if (option == NOT_ADJUSTING || blink_state.get())
    {
        switch (value)
        {
            default:
                value = 0;
                // fall-through intended
            case 0:
                lcd << PGM << STR_SET_SLEEPMODE_TIME;
                break;
            case 1:
                lcd << PGM << STR_SET_SLEEPMODE_OFF;
                break;
            case 2:
                lcd << PGM << STR_SET_SLEEPMODE_DISABLED;
                break;
        }
    } else {
        lcd << stream::Spaces(13);
    }

    return true;
}

bool LcdStateSettings::pageReset()
{
    lcd.move(0, 0);
    lcd << '!' << PGM << STR_SET_RESET;
    if (option != NOT_ADJUSTING)
        Watchdog::forceRestart();
    return true;
}

bool LcdStateSettings::pageExit()
{
    lcd.move(0, 0);
    lcd << static_cast<char>(0x7F); // <- back arrow
    lcd << PGM << STR_SET_EXIT;
    return (option == NOT_ADJUSTING);
}

void LcdStateSettings::select()
{
    if (option == NOT_ADJUSTING)
    {
        ++page;
        value = UNINITIALIZED;
        lcd.clear();
    } else {
        ++value;
    }

    resetTimeout();
}

void LcdStateSettings::advance()
{
    ++option;
    resetTimeout();
}

bool LcdStateSettings::timedOut()
{
    if (last_action == 0)
        resetTimeout();
    return (Timer::millis() - last_action) > SETTINGS_TIMEOUT;
}

void LcdStateSettings::resetTimeout()
{
    last_action = Timer::millis();
}

}
