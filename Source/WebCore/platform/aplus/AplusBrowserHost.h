/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2026 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

class AplusBrowserHost {
public:
    virtual ~AplusBrowserHost() = default;

    virtual void screenSize(int& width, int& height) = 0;
    virtual void screenAvailableRect(int& x, int& y, int& width, int& height)
    {
        x = 0;
        y = 0;
        screenSize(width, height);
    }
    virtual float screenScaleFactor() { return 1; }
    virtual bool darkMode() { return false; }
    virtual void setCursor(int cursor) = 0;
};

class AplusClipboard {
public:
    virtual ~AplusClipboard() = default;

    virtual void writeText(const char* data, size_t length) = 0;
    virtual char* readText() = 0;
};

const char* aplusConfigDir();
void aplusOpenExternal(const char* url);

void aplusSetClipboard(AplusClipboard*);
AplusClipboard* aplusClipboard();
