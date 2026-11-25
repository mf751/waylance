// Copyright (c) 2026 mf751. All Rights Reserved.
#define _GNU_SOURCE

#include "shm.h"
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client-protocol.h>

// INFO: called when compositor finishes reading our buffer
static void buffer_release(void *data, struct wl_buffer *buffer) {
    (void)data;
    wl_buffer_destroy(buffer);
}

// INFO: Callback table for `wl_buffer` events
static const struct wl_buffer_listener wl_buffer_listener = {
    .release = buffer_release,
};

struct wl_buffer *shm_create_buffer(struct wl_shm *shm, int width, int height,
                                    shm_render_fn render, void *user_data) {
    // NOTE: 4 bytes per pixel * a row of pixels = total bytes per row of pixels
    // 1 byte for A, and 1 for R, G, B same as WL_SHM_FORMAT_ARGB8888
    int stride = width * 4;

    // NOTE: total bytes per row * number of rows = total bytes size
    int size = stride * height;

    // INFO: anonymous ram file descriptor to send via unix socket to
    // compositor which represents our image buffer in ram
    int fd = memfd_create("waylance-shm", MFD_CLOEXEC);
    if (fd < 0 || ftruncate(fd, size) < 0) {
        // NOTE: close fd if it was created
        if (fd >= 0)
            close(fd);

        // NOTE: return failed
        return NULL;
    }

    // NOTE: map the fd to proccess's address space (compositor will to do
    // the same) read + write perms and share flag so writes happen to shared
    // memory pool immediatly
    unsigned char *pixels =
        mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pixels == MAP_FAILED) {
        close(fd);
        return NULL;
    }

    // NOTE: call the given rendering function to the shared memory pool
    render(pixels, width, height, stride, user_data);

    // NOTE: unmap the fd from proccess's user address space
    munmap(pixels, size);

    // NOTE: send fd to wayland socket so the compositor can
    // map the shared memory pool
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);

    // NOTE: make a wl_buffer of that pool with our dimensions
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(
        pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);

    // NOTE: attach buffer release callback
    wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);

    wl_shm_pool_destroy(pool);
    close(fd);

    // NOTE: return ready-to-display buffer
    return buffer;
}
