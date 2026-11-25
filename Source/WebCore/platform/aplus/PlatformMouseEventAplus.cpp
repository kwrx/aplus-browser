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
#include "PlatformMouseEvent.h"

#include "AplusEvents.h"
#include <wtf/MonotonicTime.h>

namespace WebCore {

static OptionSet<PlatformEvent::Modifier> modifiersFromAplus(uint8_t modifiers)
{
    OptionSet<PlatformEvent::Modifier> result;

    if (modifiers & APLUS_MOD_SHIFT) {
        result.add(PlatformEvent::Modifier::ShiftKey);
    }
    if (modifiers & APLUS_MOD_CTRL) {
        result.add(PlatformEvent::Modifier::ControlKey);
    }
    if (modifiers & APLUS_MOD_ALT) {
        result.add(PlatformEvent::Modifier::AltKey);
    }
    if (modifiers & APLUS_MOD_META) {
        result.add(PlatformEvent::Modifier::MetaKey);
    }
    if (modifiers & APLUS_MOD_CAPSLOCK) {
        result.add(PlatformEvent::Modifier::CapsLockKey);
    }
    if (modifiers & APLUS_MOD_ALTGR) {
        result.add(PlatformEvent::Modifier::AltGraphKey);
    }

    return result;
}

static MouseButton buttonFromAplus(uint8_t button)
{
    if (button & APLUS_BUTTON_LEFT) {
        return MouseButton::Left;
    }
    if (button & APLUS_BUTTON_MIDDLE) {
        return MouseButton::Middle;
    }
    if (button & APLUS_BUTTON_RIGHT) {
        return MouseButton::Right;
    }
    if (button & APLUS_BUTTON_BACK) {
        return MouseButton::Back;
    }
    if (button & APLUS_BUTTON_FORWARD) {
        return MouseButton::Forward;
    }

    return MouseButton::None;
}

static PlatformEvent::Type typeFromAplus(AplusPointerKind kind)
{
    switch (kind) {
    case APLUS_POINTER_PRESS:
        return PlatformEvent::Type::MousePressed;
    case APLUS_POINTER_RELEASE:
        return PlatformEvent::Type::MouseReleased;
    default:
        return PlatformEvent::Type::MouseMoved;
    }
}

PlatformMouseEvent::PlatformMouseEvent(const AplusPointerEvent& event)
    : PlatformMouseEvent(DoublePoint(event.x, event.y), DoublePoint(event.globalX, event.globalY),
        buttonFromAplus(event.kind == APLUS_POINTER_MOVE ? event.buttons : event.button), typeFromAplus(event.kind),
        event.kind == APLUS_POINTER_MOVE ? 0 : event.clickCount, modifiersFromAplus(event.modifiers), MonotonicTime::now(), 0,
        SyntheticClickType::NoTap, MouseEventInputSource::UserDriven)
{
    if (event.buttons & APLUS_BUTTON_LEFT) {
        m_buttons |= 1;
    }
    if (event.buttons & APLUS_BUTTON_RIGHT) {
        m_buttons |= 2;
    }
    if (event.buttons & APLUS_BUTTON_MIDDLE) {
        m_buttons |= 4;
    }
    if (event.buttons & APLUS_BUTTON_BACK) {
        m_buttons |= 8;
    }
    if (event.buttons & APLUS_BUTTON_FORWARD) {
        m_buttons |= 16;
    }
}

}
