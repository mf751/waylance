// Copyright (c) 2026 mf751. All Rights Reserved.

#include "render/render.h"
#include "wayland/wayland.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void on_render(unsigned char *pixels, int width, int height, int stride,
               void *user_data) {
    render_overlay(pixels, width, height, stride);
}

int main(int argc, char *argv[]) {
    struct wayland_config cfg = {
        .namespace = "waylance", .width = 350, .height = 500};

    if (argc == 3) {
        int64_t w = strtol(argv[1], NULL, 10);
        int64_t h = strtol(argv[2], NULL, 10);
        if (w <= 0 || h <= 0) {
            fprintf(stderr, "Usage: %s [width height]\n", argv[0]);
            return 1;
        }

        cfg.width = (uint32_t)w;
        cfg.height = (uint32_t)h;
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [width height]\n", argv[0]);
        return 1;
    }

    return wayland_run(&cfg, on_render, NULL);
}
