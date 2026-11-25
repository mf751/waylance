// Copyright (c) 2026 mf751. All Rights Reserved.
//
#include "render.h"
#include <cairo/cairo.h>

void render_overlay(unsigned char *pixels, int width, int height, int stride) {
    // NOTE: get byte length per pixel row
    // int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);

    // NOTE: total byte size of image buffer
    int size = stride * height;

    // NOTE: make a cairo surface for the image buffer
    // CAIRO_FORMAT_ARGB32 matches WL_SHM_FORMAT_ARGB8888
    cairo_surface_t *cairo_surf = cairo_image_surface_create_for_data(
        pixels, CAIRO_FORMAT_ARGB32, width, height, stride);

    // NOTE: cairo drawing context targeting our cairo surface
    cairo_t *cr = cairo_create(cairo_surf);

    // -- drawing a background --

    // NOTE: set mode to overwrite
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

    // NOTE: simple color with 0.85 opacity
    cairo_set_source_rgba(cr, 0.12, 0.12, 0.18, 0.85);

    // NOTE: fill entire surface with color
    cairo_paint(cr);

    // -- background over --

    // -- Making a border --

    // NOTE: Switch mode to normal drawing ontop of existing
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr, 0.54, 0.71, .98, 1.0);
    cairo_set_line_width(cr, 2.0);

    // NOTE: make a rectangle smaller than background by 1px of each side
    cairo_rectangle(cr, 1, 1, width - 2, height - 2);

    // NOTE: draw rectangle outline
    cairo_stroke(cr);

    // -- border over --

    // NOTE: free drawing context
    cairo_destroy(cr);

    // NOTE: free cairo surface
    cairo_surface_destroy(cairo_surf);
}
