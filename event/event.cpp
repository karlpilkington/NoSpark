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

#include "event.h"

namespace event
{

Event::Event()
    : id(0)
    , param(0)
    , delay(0)
    , posted(0)
{
}

Event::Event(const uint16_t id, const uint32_t param)
    : id(id)
    , param(param)
    , delay(0)
    , posted(0)
{
}

bool Event::operator== (const Event &rhs) const
{
    return this->id == rhs.id;
}


}
