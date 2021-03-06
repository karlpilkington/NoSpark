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

#include <avr/io.h>

#include "board/j1772pilot.h"
#include "board/pins.h"
#include "utils/atomic.h"
#include "utils/math.h"
#include "utils/pair.h"

#define J1772_SAMPLES 100

#define J1772_DIODE_THRESHOLD     256 // 1024 ADC_STEPS / 24V = ~43 STEPS/V
#define J1772_THRESHOLD_OFFSET    512 // Assuming this is half range (+12 <-> -12)
#define J1772_THRESHOLD_STEP      100 // 1023 / 10 ~= 100 :D

namespace
{
    using board::J1772Pilot;

    uint8_t amp2duty(const uint8_t amp)
    {
        uint16_t d = amp;
        if (amp <= 51)
            d = d * 10 / 6;
        else
            d = d * 2 / 5 + 64;
        return d;
    }

    void setPinActive(const bool active)
    {
        DDRB |= (1 << PB2);
        if (active)
            PORTB |= (1 << PB2);
        else
            PORTB &= ~(1 << PB2);
    }

    void pwmEnable(const uint8_t duty_cycle)
    {
        utils::Atomic _atomic;

        DDRB |= (1 << PB2);

        TCCR1A = (1 << COM1B1) | (1 << WGM11) | (1 << WGM10);
        TCCR1B = (1 << WGM13)  | (1 << WGM12) | (1 << CS11) | (1 << CS10);

        OCR1A = 249;
        OCR1B = (249 * duty_cycle / 100);
    }

    void pwmDisable()
    {
        utils::Atomic _atomic;
        TCCR1A = 0;
    }

    J1772Pilot::State stateFromSample(const uint16_t sample)
    {
        if (sample < J1772_THRESHOLD_OFFSET)
            return J1772Pilot::IMPLAUSIBLE;

        const J1772Pilot::State states[] = {
            J1772Pilot::STATE_E,
            J1772Pilot::STATE_D,
            J1772Pilot::STATE_C,
            J1772Pilot::STATE_B,
            J1772Pilot::STATE_A,
            J1772Pilot::STATE_A // Rounding up buffer
        };

        return states[(sample - J1772_THRESHOLD_OFFSET) / J1772_THRESHOLD_STEP];
    }

    utils::Pair<uint16_t, uint16_t> samplePin(const board::Pin& pin)
    {
        utils::Pair<uint16_t,uint16_t> reading = {1023, 0};

        for (uint8_t i = 0; i != J1772_SAMPLES; ++i)
        {
            const uint16_t value = pin.analogRead();
            reading.first = utils::min(reading.first, value);
            reading.second = utils::max(reading.second, value);
        }

        return reading;
    }
}

namespace board
{

J1772Pilot::J1772Pilot()
    : mode(LOW)
    , pinSense(PIN_J1772_STATUS)
{
    pinSense.io(Pin::IN);
}

void J1772Pilot::setMode(const Mode mode)
{
    if (mode == PWM)
        return;
    this->mode = mode;
    pwmDisable();
    setPinActive(mode == HIGH);
}

J1772Pilot::Mode J1772Pilot::getMode() const
{
    return mode;
}

void J1772Pilot::pwmAmps(const uint8_t amps)
{
    mode = PWM;
    pwmEnable(amp2duty(amps));
}

J1772Pilot::State J1772Pilot::getState() const
{
    if (J1772Pilot::getMode() == J1772Pilot::LOW)
        return NOT_READY;

    const auto sample = samplePin(pinSense);
    if (J1772Pilot::getMode() == J1772Pilot::PWM && sample.first > J1772_DIODE_THRESHOLD)
        return DIODE_CHECK_FAILED;

    return stateFromSample(sample.second);
}

}
