// Copyright (c) 2026 mf751. All Rights Reserved.

#include "wayland.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wayland-client-protocol.h>

#include "../../protocols/wlr-layer-shell-unstable-v1-client-protocol.h"
#include "shm.h"

// NOTE: Avoid linking xdg_shell_protocol.c since we don't use popups
const struct wl_interface xdg_popup_interface = {0};

struct app {
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct zwlr_layer_shell_v1 *layer_shell;

    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer_surface;

    void *user_data;
    shm_render_fn render;
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
        // INFO: requests a bind with the compositor global which returns a live
        // proxy handle
        app->compositor =
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
    struct wl_buffer *buf =
        shm_create_buffer(app->shm, width, height, app->render, app->user_data);

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

int wayland_run(const struct wayland_config *cfg, shm_render_fn render,
                void *user_data) {
    struct app app = {
        .running = true, .render = render, .user_data = user_data};

    struct wl_display *display = wl_display_connect(NULL);

    if (!display) {
        fprintf(stderr, "failed to connect to wayland display\n");
        return 1;
    }

    struct wl_registry *registry = wl_display_get_registry(display);

    // NOTE: pass app's reference so it's given to the callback function
    wl_registry_add_listener(registry, &registry_listener, &app);

    // NOTE: initiate protocols, the compositor will use the callbacks in
    // registry_listener
    wl_display_roundtrip(display);

    if (!app.compositor || !app.shm || !app.layer_shell) {
        fprintf(stderr, "Missing required globals (does your compositor "
                        "support wlr-layer-shell?)\n");
        wl_display_disconnect(display);
        return 1;
    }

    // INFO: wl_surface is just a canvas
    app.surface = wl_compositor_create_surface(app.compositor);

    // NOTE: attach an overlay role to our surface
    app.layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        app.layer_shell, app.surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        cfg->namespace);

    zwlr_layer_surface_v1_set_size(app.layer_surface, cfg->width, cfg->height);

    // NOTE: don't reserve space (don't push windows)
    zwlr_layer_surface_v1_set_exclusive_zone(app.layer_surface, 0);

    // NOTE: make it not take keyboard focus
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        app.layer_surface, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);

    // NOTE: pass app's reference so it's given to the callback function
    zwlr_layer_surface_v1_add_listener(app.layer_surface,
                                       &layer_surface_listener, &app);

    // NOTE:  initial commit to make compositor use the callbacks in
    // layer_surface_listener
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
