// Copyright (c) 2026 mf751. All Rights Reserved.

#include "render/render.h"
#include "wayland/wayland.h"

void on_render(unsigned char *pixels, int width, int height, int stride,
               void *user_data) {
    render_overlay(pixels, width, height, stride);
}

int main(void) {
    struct wayland_config cfg = {
        .namespace = "waylance", .width = 350, .height = 500};
    return wayland_run(&cfg, on_render, NULL);
}
