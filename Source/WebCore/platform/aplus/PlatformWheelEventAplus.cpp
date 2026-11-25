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

#include "config.h"
#include "PlatformWheelEvent.h"

#include "AplusEvents.h"
#include "Scrollbar.h"

namespace WebCore {

PlatformWheelEvent::PlatformWheelEvent(const AplusScrollEvent& event)
    : PlatformWheelEvent(IntPoint(event.x, event.y), IntPoint(event.globalX, event.globalY), 0, 0, 0, 0, PlatformWheelEventGranularity::ScrollByPixelWheelEvent,
        event.modifiers & APLUS_MOD_SHIFT, event.modifiers & APLUS_MOD_CTRL, event.modifiers & APLUS_MOD_ALT, event.modifiers & APLUS_MOD_META)
{
    float ticksX = event.dx;
    float ticksY = event.dy;

    if (shiftKey() && !ticksX) {
        ticksX = ticksY;
        ticksY = 0;
    }

    m_wheelTicksX = ticksX;
    m_wheelTicksY = ticksY;
    m_hasPreciseScrollingDeltas = event.precise;

    float step = event.precise ? 1 : static_cast<float>(Scrollbar::pixelsPerLineStep());
    m_deltaX = ticksX * step;
    m_deltaY = ticksY * step;
}

}
