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
#include "GraphicsLayer.h"

#include "GraphicsLayerFactory.h"

namespace WebCore {

class GraphicsLayerAplus final : public GraphicsLayer {
public:
    GraphicsLayerAplus(Type type, GraphicsLayerClient& client)
        : GraphicsLayer(type, client)
    {
    }

    ~GraphicsLayerAplus() final
    {
        willBeDestroyed();
    }

    void setNeedsDisplay() final { }
    void setNeedsDisplayInRect(const FloatRect&, ShouldClipToLayer) final { }
};

Ref<GraphicsLayer> GraphicsLayer::create(GraphicsLayerFactory* factory, GraphicsLayerClient& client, Type type)
{
    if (!factory) {
        return adoptRef(*new GraphicsLayerAplus(type, client));
    }
    return factory->createGraphicsLayer(type, client);
}

}
