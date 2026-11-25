// Copyright (c) 2026 mf751. All Rights Reserved.

#pragma once

#include <wayland-client.h>

typedef void (*shm_render_fn)(unsigned char *pixels, int width, int height,
                              int stride, void *user_data);

struct wl_buffer *shm_create_buffer(struct wl_shm *shm, int width, int height,
                                    shm_render_fn render, void *user_data);
