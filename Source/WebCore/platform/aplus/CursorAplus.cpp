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
#include "Cursor.h"

#include <aplus/ui.h>

namespace WebCore {

void Cursor::ensurePlatformCursor() const
{
    intptr_t shape = UI_CURSOR_DEFAULT;

    switch (m_type) {
    case Type::IBeam:
    case Type::VerticalText:
        shape = UI_CURSOR_TEXT;
        break;
    default:
        break;
    }

    m_platformCursor = reinterpret_cast<void*>(shape);
}

}
