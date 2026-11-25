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
#include "PopupMenuAplus.h"

#include "AplusWebView.h"

#include <WebCore/IntRect.h>
#include <WebCore/LocalFrameView.h>

namespace WebCore {

PopupMenuAplus::PopupMenuAplus(PopupMenuClient* client, AplusWebView* view)
    : m_client(client)
    , m_view(view)
{
}

PopupMenuAplus::~PopupMenuAplus()
{
    if (m_view && m_view->popupMenu() == this) {
        m_view->setPopupMenu(nullptr);
    }
}

void PopupMenuAplus::show(const IntRect& rect, LocalFrameView& frameView, int selectedIndex)
{
    if (!m_client || !m_view || !m_view->client()) {
        return;
    }

    std::vector<AplusPopupItem> items;
    int count = m_client->listSize();
    items.reserve(count > 0 ? count : 0);

    for (int i = 0; i < count; i++) {
        AplusPopupItem item;
        item.separator = m_client->itemIsSeparator(i);
        item.label = m_client->itemIsLabel(i);
        item.enabled = m_client->itemIsEnabled(i);
        item.selected = m_client->itemIsSelected(i);
        item.text = m_client->itemText(i).utf8().toStdString();
        item.toolTip = m_client->itemToolTip(i).utf8().toStdString();
        items.push_back(std::move(item));
    }

    IntRect r = frameView.contentsToRootView(rect);

    m_visible = true;
    m_view->setPopupMenu(this);
    m_view->client()->showPopupMenu(items, r.x(), r.y(), r.width(), r.height(), selectedIndex);
}

void PopupMenuAplus::hide()
{
    if (!m_visible) {
        return;
    }

    m_visible = false;

    if (m_view && m_view->popupMenu() == this) {
        m_view->setPopupMenu(nullptr);
        if (m_view->client()) {
            m_view->client()->hidePopupMenu();
        }
    }

    if (m_client) {
        m_client->popupDidHide();
    }
}

void PopupMenuAplus::updateFromElement()
{
}

void PopupMenuAplus::disconnectClient()
{
    m_client = nullptr;
    hide();
}

void PopupMenuAplus::didSelect(int index)
{
    Ref protectedThis { *this };

    if (m_view && m_view->popupMenu() == this) {
        m_view->setPopupMenu(nullptr);
    }

    bool wasVisible = m_visible;
    m_visible = false;

    if (!m_client) {
        return;
    }

    if (index >= 0 && index < m_client->listSize()) {
        m_client->valueChanged(index);
    }

    if (wasVisible && m_client) {
        m_client->popupDidHide();
    }
}

void PopupMenuAplus::didHide()
{
    Ref protectedThis { *this };

    if (m_view && m_view->popupMenu() == this) {
        m_view->setPopupMenu(nullptr);
    }

    bool wasVisible = m_visible;
    m_visible = false;

    if (wasVisible && m_client) {
        m_client->popupDidHide();
    }
}

}
