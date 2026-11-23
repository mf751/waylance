// Copyright (c) 2026 mf751. All Rights Reserved.

#define _GNU_SOURCE
#include <cairo/cairo.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <wayland-util.h>

#include "./wlr-layer-shell-unstable-v1-client-protocol.h"

// NOTE: Avoid linking xdg_shell_protocol.c since we don't use popups
const struct wl_interface xdg_popup_interface = {0};

struct app {
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct zwlr_layer_shell_v1 *layer_shell;

    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer_surface;

    bool running;
};

// INFO: Called for every global the compositor provides
static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface,
                            uint32_t version) {
    (void)version;
    // NOTE: re-point to the app instance in main that was given via data
    struct app *app = data;

    // NOTE: If global is compositor
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        app->compositor =
            // INFO: requests a bind with the global which returns a live proxy
            // handle
            wl_registry_bind(registry, name, &wl_compositor_interface, 4);

        // NOTE: If global is shared memory
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        app->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);

        // NOTE: If global is layer shell
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        app->layer_shell =
            wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
    }
}

// INFO: Called for every global removal the compositor provides (eg. monitor
// unplugged)
static void registry_global_remove(void *data, struct wl_registry *registry,
                                   uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
}

// INFO: Callback table for `wl_registry` events
static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

// INFO: called when compositor finishes reading our buffer
static void buffer_release(void *data, struct wl_buffer *buffer) {
    (void)data;
    wl_buffer_destroy(buffer);
}

// INFO: Callback table for `wl_buffer` events
static const struct wl_buffer_listener wl_buffer_listener = {
    .release = buffer_release,
};

static struct wl_buffer *draw_overlay(struct wl_shm *shm, int width,
                                      int height) {
    // NOTE: get byte length per pixel row
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);

    // NOTE: total byte size of image buffer
    int size = stride * height;

    // INFO: anonymous ram file descriptor to send via unix socket to compositor
    // which represents our image buffer in ram
    int fd = memfd_create("waylance-shm", MFD_CLOEXEC);
    if (fd < 0 || ftruncate(fd, size) < 0) {
        // NOTE: close fd if it was created
        if (fd >= 0)
            close(fd);

        // NOTE: return drawing failed
        return NULL;
    }

    // NOTE: map the fd to proccess's address space (other end will have to do
    // the same) read + write and share perms so other end can access it too
    unsigned char *pixels =
        mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pixels == MAP_FAILED) {
        close(fd);
        return NULL;
    }

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

    // NOTE: unmap the fd (image buffer)
    munmap(pixels, size);

    // NOTE: send fd to wayland socket so the compositor can
    // map the image buffer (memory pool)
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

// INFO: called by compositor when the overlay is ready or changed size
static void layer_surface_configure(void *data,
                                    struct zwlr_layer_surface_v1 *layer_surface,
                                    uint32_t serial, uint32_t width,
                                    uint32_t height) {
    // NOTE: Convert `void *data` back into our `struct app *` pointer
    struct app *app = data;

    // NOTE: Required Ack by protocol
    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);

    // NOTE: returns a drawn buffer ready-to-display
    struct wl_buffer *buf = draw_overlay(app->shm, (int)width, (int)height);
    if (!buf)
        return;

    // NOTE: attach drawn buffer to wayland surface offset 0, 0
    wl_surface_attach(app->surface, buf, 0, 0);

    // NOTE: inform compositor to redraw the entire surface
    wl_surface_damage(app->surface, 0, 0, (int)width, (int)height);

    // NOTE: apply all changes (ack + attach + damage)
    wl_surface_commit(app->surface);
}

// INFO: Called when compositor closes overlay
static void layer_surface_closed(void *data,
                                 struct zwlr_layer_surface_v1 *layer_surface) {
    (void)layer_surface;

    // NOTE: Convert `void *data` back into our `struct app *` pointer
    struct app *app = data;

    // NOTE: end app lifecycle
    app->running = false;
}

// INFO: callback table for `zwlr_layer_surface_v1` events
static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

int main(void) {
    struct app app = {.running = true};

    struct wl_display *display = wl_display_connect(NULL);

    if (!display) {
        fprintf(stderr, "failed to connect to wayland display\n");
        return 1;
    }

    struct wl_registry *registry = wl_display_get_registry(display);

    // NOTE: pass app's reference so it's given to the callback function
    wl_registry_add_listener(registry, &registry_listener, &app);
    wl_display_roundtrip(display);

    if (!app.compositor || !app.shm || !app.layer_shell) {
        fprintf(stderr, "Missing required globals (does your compositor "
                        "support wlr-layer-shell?)\n");
        wl_display_disconnect(display);
        return 1;
    }

    // INFO: wl_surface is just a canvas
    app.surface = wl_compositor_create_surface(app.compositor);

    // NOTE: attaches an overlay role to our surface
    app.layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        app.layer_shell, app.surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        "waylance");

    zwlr_layer_surface_v1_set_size(app.layer_surface, 500, 350);

    // NOTE: don't reserve space (don't push windows)
    zwlr_layer_surface_v1_set_exclusive_zone(app.layer_surface, 0);

    // NOTE: make it not take keyboard focus
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        app.layer_surface, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);

    // NOTE: pass app's reference so it's given to the callback function
    zwlr_layer_surface_v1_add_listener(app.layer_surface,
                                       &layer_surface_listener, &app);

    // NOTE:  initial commit to make compositor call app's config event
    wl_surface_commit(app.surface);

    // NOTE: keep dispatching while app.running is true and no dispatch errors
    while (app.running && wl_display_dispatch(display) != -1) {
    }

    // NOTE: unattach the overlay role of our surface
    zwlr_layer_surface_v1_destroy(app.layer_surface);

    wl_surface_destroy(app.surface);

    wl_display_disconnect(display);

    return 0;
}
