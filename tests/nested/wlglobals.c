/* Minimal Wayland client: prints "<interface> <version>" for every global the
 * compositor advertises (one line per global, duplicates such as multiple
 * wl_output are kept). */
#include <stdio.h>
#include <wayland-client.h>

static void global(void* data, struct wl_registry* reg, uint32_t name, const char* iface, uint32_t version) {
    (void)data; (void)reg; (void)name;
    printf("%s %u\n", iface, version);
}

static void global_remove(void* data, struct wl_registry* reg, uint32_t name) {
    (void)data; (void)reg; (void)name;
}

static const struct wl_registry_listener listener = {global, global_remove};

int main(void) {
    struct wl_display* d = wl_display_connect(NULL);
    if (!d) {
        fprintf(stderr, "wlglobals: cannot connect to the Wayland display\n");
        return 1;
    }
    struct wl_registry* r = wl_display_get_registry(d);
    wl_registry_add_listener(r, &listener, NULL);
    if (wl_display_roundtrip(d) < 0) {
        fprintf(stderr, "wlglobals: roundtrip failed\n");
        return 1;
    }
    wl_registry_destroy(r);
    wl_display_disconnect(d);
    return 0;
}
