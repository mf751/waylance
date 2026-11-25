// Copyright (c) 2026 mf751. All Rights Reserved.
#pragma once

#include "shm.h"
#include <stdint.h>

struct wayland_config {
    const char *namespace;
    uint32_t width;
    uint32_t height;
};

int wayland_run(const struct wayland_config *cfg, shm_render_fn render,
                void *user_data);
