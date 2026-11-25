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
#include "DragImage.h"

#include "Image.h"
#include "NativeImage.h"
#include "RefPtrCairo.h"
#include <cairo.h>

namespace WebCore {

IntSize dragImageSize(DragImageRef image)
{
    if (!image) {
        return { };
    }

    return { cairo_image_surface_get_width(image.get()), cairo_image_surface_get_height(image.get()) };
}

void deleteDragImage(DragImageRef)
{
}

DragImageRef scaleDragImage(DragImageRef image, FloatSize scale)
{
    if (!image) {
        return nullptr;
    }

    IntSize imageSize = dragImageSize(image);
    IntSize scaledSize(imageSize);
    scaledSize.scale(scale.width(), scale.height());
    if (imageSize == scaledSize || scaledSize.isEmpty()) {
        return image;
    }

    RefPtr<cairo_surface_t> scaledSurface = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, scaledSize.width(), scaledSize.height()));
    RefPtr<cairo_t> context = adoptRef(cairo_create(scaledSurface.get()));
    cairo_scale(context.get(), scale.width(), scale.height());
    cairo_set_source_surface(context.get(), image.get(), 0, 0);
    cairo_pattern_set_extend(cairo_get_source(context.get()), CAIRO_EXTEND_PAD);
    cairo_pattern_set_filter(cairo_get_source(context.get()), CAIRO_FILTER_BILINEAR);
    cairo_set_operator(context.get(), CAIRO_OPERATOR_SOURCE);
    cairo_paint(context.get());

    return scaledSurface;
}

DragImageRef dissolveDragImageToFraction(DragImageRef image, float fraction)
{
    if (!image) {
        return nullptr;
    }

    RefPtr<cairo_t> context = adoptRef(cairo_create(image.get()));
    cairo_set_operator(context.get(), CAIRO_OPERATOR_DEST_IN);
    cairo_set_source_rgba(context.get(), 0, 0, 0, fraction);
    cairo_paint(context.get());

    return image;
}

DragImageRef createDragImageFromImage(Image* image, ImageOrientation, GraphicsClient*, float)
{
    if (!image) {
        return nullptr;
    }

    RefPtr nativeImage = image->currentNativeImage();
    if (!nativeImage) {
        return nullptr;
    }

    cairo_surface_t* source = nativeImage->platformImage().get();
    int width = cairo_image_surface_get_width(source);
    int height = cairo_image_surface_get_height(source);

    RefPtr<cairo_surface_t> surface = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height));
    RefPtr<cairo_t> context = adoptRef(cairo_create(surface.get()));
    cairo_set_source_surface(context.get(), source, 0, 0);
    cairo_set_operator(context.get(), CAIRO_OPERATOR_SOURCE);
    cairo_paint(context.get());

    return surface;
}

DragImageRef createDragImageIconForCachedImageFilename(const String&)
{
    return nullptr;
}

}
