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

#include <WebCore/PopupMenu.h>
#include <WebCore/PopupMenuClient.h>

class AplusWebView;

namespace WebCore {

class PopupMenuAplus final : public PopupMenu {
public:
    PopupMenuAplus(PopupMenuClient*, AplusWebView*);
    ~PopupMenuAplus();

    void show(const IntRect&, LocalFrameView&, int selectedIndex) override;
    void hide() override;
    void updateFromElement() override;
    void disconnectClient() override;

    void didSelect(int index);
    void didHide();

private:
    PopupMenuClient* m_client;
    AplusWebView* m_view;
    bool m_visible { false };
};

}
