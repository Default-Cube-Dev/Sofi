#ifndef WOIDSHELL_H
#define WOIDSHELL_H

#include <EGL/egl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wayland-client-protocol.h>
#include <wayland-client-core.h>
#include <wayland-egl-core.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#define WOIDSHELL_EGL_IMPLEMENTATION
#include "woidshell-egl.h"


typedef struct WS_InputSettings {
    enum zwlr_layer_surface_v1_keyboard_interactivity keyboard_interactivity;
    const struct wl_pointer_listener pointer_events;
    const struct wl_keyboard_listener keyboard_events;
} WS_InputSettings;

typedef struct WS_Input {
    struct wl_keyboard* keyboard;
    struct wl_pointer* pointer;
    struct wl_seat* seat;
} WS_Input;


typedef struct WS_ShellSettings {
    uint32_t layer;
    uint32_t width;
    uint32_t height;
    uint32_t anchor;
    uint32_t exclusive_zone;
    uint32_t margin_l;
    uint32_t margin_b;
    uint32_t margin_t;
    uint32_t margin_r;
    void (*drawfunc)(void*);
    WS_InputSettings input;
} WS_ShellSettings;

typedef struct WS_Shell{
    struct wl_display* display;
    struct wl_registry* registry;
    struct wl_compositor* compositor;
    struct wl_shm* shm;
    struct zwlr_layer_shell_v1* layer_shell;
    struct wl_surface* surface;
    struct zwlr_layer_surface_v1* layer_surface;
    struct wl_output* output;
    WS_Input input;
    WS_EGL_Egl* egl;
    WS_ShellSettings* settings;
    bool run;
} WS_Shell;

void WS_ShellInit(WS_Shell* shell);
bool WS_ShellShouldClose(WS_Shell* shell);
void WS_ShellDestroy(WS_Shell* shell);
void WS_ExecFrameCallback(WS_Shell* shell);

void WS_HandleGlobal(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
void WS_HandleGlobalRemove(void *data, struct wl_registry *registry, uint32_t name);
static const struct wl_registry_listener WS_RegistryListener = {
    .global = WS_HandleGlobal,
    .global_remove = WS_HandleGlobalRemove,
};

static void WS_LayerSurfaceConfigure(void *data, struct zwlr_layer_surface_v1 *surface,
        uint32_t serial, uint32_t w, uint32_t h);
static void WS_LayerSurfaceClosed(void* data, struct zwlr_layer_surface_v1* surface);
static const struct zwlr_layer_surface_v1_listener WS_LayerSurfaceListener = {
    .configure = WS_LayerSurfaceConfigure,
    .closed = WS_LayerSurfaceClosed,
};

static void WS_FrameDone(void* data, struct wl_callback* callback, uint32_t time);
static const struct wl_callback_listener WS_FrameListener = {
    .done = WS_FrameDone,
};

#ifdef WOIDSHELL_INPUT
void WS_SeatHandleName(void* data, struct wl_seat* seat, const char* name) {/*Who cares :D*/}

void WS_SeatHandleCapabilities(void* data, struct wl_seat* seat, uint32_t caps) { 
    WS_Shell* shell = (WS_Shell*)data;
    if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
        shell->input.keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(shell->input.keyboard, &shell->settings->input.keyboard_events, NULL);

    } else if (caps & WL_SEAT_CAPABILITY_POINTER) {
        shell->input.pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(shell->input.pointer, &shell->settings->input.pointer_events, NULL);
    }
}
struct wl_seat_listener WS_SeatListener = {
    .capabilities = WS_SeatHandleCapabilities,
    .name = WS_SeatHandleName,
};
#endif

#define WOIDSHELL_IMPLEMENTATION
#ifdef WOIDSHELL_IMPLEMENTATION

void WS_ShellInit(WS_Shell* shell){
//////////////////Setup////////////
    if (shell->settings == NULL) printf("you gotta configure the shell bruh (by changing shell->settings)\n");
    
    if(shell->egl == NULL) shell->egl = (WS_EGL_Egl*)malloc(sizeof(WS_EGL_Egl)); 

    shell->display = wl_display_connect(NULL);
    if (!shell->display) {
        printf("failed to connect to display\n");
    }
    printf("Wayland connected to display\n");
    shell->run = true;

    shell->registry = wl_display_get_registry(shell->display);
    printf("got a registry\n");
    wl_registry_add_listener(shell->registry, &WS_RegistryListener, shell);
    wl_display_roundtrip(shell->display);

    if (shell->layer_shell == NULL) {
        printf("we don't have a layer_shell :(, ur compositor doesn't support it,\n r u on gnome or something?!?!?!?!\n u stupid gnome users can't even implement a proper wlroots support for urself! u r weak I DON'T FUCK WITH YOU!\n (sorry I'm toxic cuz I just use arch (btw :) ))\n");
    } else if (shell->shm == NULL) {
        printf("wtf!?!?!?!?\n don't all Wayland compositors support wl_shm??? Try to relaunch the shell\n");
    } else if (shell->compositor == NULL) {
        printf("wtf!?!?!?!\n don't u have a Wayland compositor?!\n");
    } else if (shell->output == NULL) {
        printf("wtf!?!?!?!?\n don't all Wayland compositors support wl_output??? Try to relaunch the shell\n");
    } 
#ifdef WOIDSHELL_INPUT
    else if (shell->input.seat == NULL) {
        printf("failed to get seat, are you sitting on the flor or something?!\n");
    }

    wl_seat_add_listener(shell->input.seat, &WS_SeatListener, shell);
#endif

    WS_EGL_Init(shell->egl, shell->display);
    printf("egl initialized\n");
    
    shell->surface = wl_compositor_create_surface(shell->compositor);
    if (shell->surface == NULL) {
        printf("failed to create a surface\n");
        exit(1);
    }
    printf("created a surface\n");

    shell->layer_surface = zwlr_layer_shell_v1_get_layer_surface(shell->layer_shell, shell->surface, shell->output, shell->settings->layer, "woidshell");

    ///////////////// surface config /////////////////

	zwlr_layer_surface_v1_set_size(shell->layer_surface, shell->settings->width, shell->settings->height);
    zwlr_layer_surface_v1_set_anchor(shell->layer_surface, shell->settings->anchor);
    zwlr_layer_surface_v1_set_exclusive_zone(shell->layer_surface, shell->settings->exclusive_zone);
    zwlr_layer_surface_v1_set_margin(shell->layer_surface,
            shell->settings->margin_t, shell->settings->margin_r, 
            shell->settings->margin_b, shell->settings->margin_l);
#ifdef WOIDSHELL_INPUT
    zwlr_layer_surface_v1_set_keyboard_interactivity(shell->layer_surface,
            shell->settings->input.keyboard_interactivity);
#endif
    zwlr_layer_surface_v1_add_listener(shell->layer_surface, &WS_LayerSurfaceListener, shell);
    printf("they see me layer, they hate it :)\n");

    wl_surface_commit(shell->surface);
    wl_display_roundtrip(shell->display);
    printf("roundtriped\n");
    
    printf("shell dimentions: %ux%u\n", shell->settings->width, shell->settings->height);
    shell->egl->window = wl_egl_window_create(shell->surface, shell->settings->width, shell->settings->height);
    printf("%p\n",shell->egl->window);
    if (shell->egl->window == NULL) {
        printf("failed to create a egl window\n");
        exit(1);

    }

    shell->egl->surface = (struct wl_egl_surface*)eglCreatePlatformWindowSurfaceEXT(shell->egl->display, shell->egl->config, shell->egl->window, NULL);
    if (shell->egl->surface == NULL || shell->egl->surface == EGL_NO_SURFACE) printf("ERROR: failed to create egl suface\n");

    wl_display_roundtrip(shell->display);
}
bool WS_ShellShouldClose(WS_Shell* shell) {return wl_display_dispatch(shell->display) == -1 && !shell->run;}

void WS_ExecFrameCallback(WS_Shell* shell){
    shell->egl->frame_callback = wl_surface_frame(shell->surface);
    wl_callback_add_listener(shell->egl->frame_callback, &WS_FrameListener, shell);
    eglSwapBuffers(shell->egl->display, shell->egl->surface);
}

void WS_ShellDestroy(WS_Shell* shell){
    shell->run = false;
    
    WS_EGL_Finish(shell->egl);
    wl_registry_destroy(shell->registry);
    wl_shm_destroy(shell->shm);
    wl_surface_destroy(shell->surface);
    zwlr_layer_surface_v1_destroy(shell->layer_surface);
    zwlr_layer_shell_v1_destroy(shell->layer_shell);
    wl_compositor_destroy(shell->compositor);
    wl_output_destroy(shell->output);
    wl_display_disconnect(shell->display);
    printf("shell destroyed\n");
}

void WS_HandleGlobal(void *data, struct wl_registry *registry, uint32_t name, const char* interface, uint32_t version){
    WS_Shell* shell = (WS_Shell*)data; 
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        shell->compositor = (struct wl_compositor*)wl_registry_bind(registry, name, &wl_compositor_interface, 4);
        printf("we have a compositor\n");
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        shell->shm = (struct wl_shm*)wl_registry_bind(registry, name, &wl_shm_interface, 1);
        printf("we have an shm\n");
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
            shell->layer_shell = (struct zwlr_layer_shell_v1*)wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
            printf("we have a layer_shell!\n");
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        shell->output = (struct wl_output*)wl_registry_bind(shell->registry, name, &wl_output_interface, 4);
        printf("we have an output\n");
    } 
#ifdef WOIDSHELL_INPUT 
    else if (strcmp(interface, wl_seat_interface.name) == 0) {
        shell->input.seat = (struct wl_seat*)wl_registry_bind(shell->registry, name, &wl_seat_interface, 1);
    }
#endif

}
void WS_HandleGlobalRemove(void *data, struct wl_registry *registry, uint32_t name){
    printf("a global was removed: %d", name);
}


static void WS_LayerSurfaceConfigure(void *data, 
        struct zwlr_layer_surface_v1 *surface, uint32_t serial,
        uint32_t w, uint32_t h){
    WS_Shell* shell = (WS_Shell*)data;
   if (shell->egl->window) {
        shell->settings->width = w;
        shell->settings->height = h;
        wl_egl_window_resize(shell->egl->window, w, h, 0, 0);
    } 
   zwlr_layer_surface_v1_ack_configure(shell->layer_surface, serial);
        printf("config\n");
}
static void WS_LayerSurfaceClosed(void* data, struct zwlr_layer_surface_v1* surface){
    WS_Shell* shell = (WS_Shell*)data;
    eglDestroySurface(shell->egl->window, shell->egl->surface);
    wl_egl_window_destroy(shell->egl->window);
    zwlr_layer_surface_v1_destroy(shell->layer_surface);
    wl_surface_destroy(shell->surface);
    shell->run = false;
}

static void WS_FrameDone(void* data, struct wl_callback* callback, uint32_t time){
    WS_Shell* shell = (WS_Shell*)data;
    wl_callback_destroy(callback);
    callback = NULL;
    shell->settings->drawfunc(shell);
}
#endif // WOIDSHELL_IMPLEMENTATION
#endif // !WOIDSHELL_H



