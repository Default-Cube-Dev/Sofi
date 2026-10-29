#ifndef WOIDSHELL_EGL_H
#define WOIDSHELL_EGL_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglplatform.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <wayland-client-protocol.h>
#include <wayland-egl-core.h>
#include <wayland-egl.h>

#define CASE_STR( value ) case value: return #value; 

typedef struct WS_EGL_Egl {
    struct wl_egl_window* window;
    struct wl_egl_surface* surface;
    EGLDisplay display;
    EGLConfig config;
    EGLContext context;
    struct wl_callback* frame_callback;

} WS_EGL_Egl;

#ifdef WOIDSHELL_OPENGLES2
const EGLint WS_EGL_ConfigAttribs[] = {
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_ALPHA_SIZE, 8,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_NONE,
};
#endif

#ifdef WOIDSHELL_OPENGL33
const EGLint WS_EGL_ConfigAttribs[] = {
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_ALPHA_SIZE, 8,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
	EGL_NONE,
};
#endif

#ifdef WOIDSHELL_OPENGLES2
const EGLint WS_EGL_ContextAttribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE,
};
#endif

#ifdef WOIDSHELL_OPENGL33
const EGLint WS_EGL_ContextAttribs[] = {
    EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
    EGL_CONTEXT_MINOR_VERSION_KHR, 3,
    EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
    EGL_NONE
};
#endif

void WS_EGL_Finish(WS_EGL_Egl* egl);
void WS_EGL_Init(WS_EGL_Egl* egl, struct wl_display* wl_display); 
#define WOIDSHELL_EGL_IMPLEMENTATION
#ifdef WOIDSHELL_EGL_IMPLEMENTATION

PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;
PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC eglCreatePlatformWindowSurfaceEXT;

const char* eglGetErrorString(EGLint error){

    switch( error ){
        CASE_STR( EGL_SUCCESS             )
        CASE_STR( EGL_NOT_INITIALIZED     )
        CASE_STR( EGL_BAD_ACCESS          )
        CASE_STR( EGL_BAD_ALLOC           )
        CASE_STR( EGL_BAD_ATTRIBUTE       )
        CASE_STR( EGL_BAD_CONTEXT         )
        CASE_STR( EGL_BAD_CONFIG          )
        CASE_STR( EGL_BAD_CURRENT_SURFACE )
        CASE_STR( EGL_BAD_DISPLAY         )
        CASE_STR( EGL_BAD_SURFACE         )
        CASE_STR( EGL_BAD_MATCH           )
        CASE_STR( EGL_BAD_PARAMETER       )
        CASE_STR( EGL_BAD_NATIVE_PIXMAP   )
        CASE_STR( EGL_BAD_NATIVE_WINDOW   )
        CASE_STR( EGL_CONTEXT_LOST        )
        default: return "Unknown";
    }
}
#undef CASE_STR

void WS_EGL_Init(WS_EGL_Egl* egl, struct wl_display* wl_display) {

    const char* client_exts = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (client_exts == NULL) {
        printf("ERROR: egl client extensions not supported: %s\n", eglGetErrorString(eglGetError()));
    }
    
    if (!strstr(client_exts, "EGL_EXT_platform_base")) {
        printf("EGL_EXT_platform_base not supported\n");
    }
    if (!strstr(client_exts, "EGL_EXT_platform_wayland")) {
        printf("EGL_EXT_platform_wayland not supported\n");
    }

#ifdef WOIDSHELL_OPENGL33
    if (!strstr(client_exts, "EGL_KHR_create_context")) {
        printf("EGL_KHR_create_context not supported\n");
    }
    if (!strstr(client_exts, "EGL_KHR_openGL_profile")) {
        printf("EGL_KHR_openGL_profile not supported\n");
    }
#endif   

    eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if(eglGetPlatformDisplayEXT == NULL) {
        printf("ERROR: failed to get eglGetPlatformDisplayEXT\n");
    }
    
    eglCreatePlatformWindowSurfaceEXT = (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");
    if(eglCreatePlatformWindowSurfaceEXT == NULL) {
        printf("ERROR: failed to get eglCreatePlatformWindowSurfaceEXT\n");
    }
    
    egl->display = (EGLDisplay*)eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_EXT, wl_display, NULL);
    if(egl->display == NULL || egl->display == EGL_NO_DISPLAY) {
        printf("ERROR: failed to create display\n");
    }
    
    if (eglInitialize(egl->display, NULL, NULL) == EGL_FALSE) {
        printf("failed to eglInitialize egl\n");
    }

#ifdef WOIDSHELL_OPENGLES2
if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    printf("Failed to bind OpenGLES2 API: %s\n", eglGetErrorString(eglGetError()));
    exit(1);
}
#endif

#ifdef WOIDSHELL_OPENGL33
if (!eglBindAPI(EGL_OPENGL_API)) {
    printf("Failed to bind OpenGLES2 API: %s\n", eglGetErrorString(eglGetError()));
    exit(1);
}
#endif

    
    EGLint matched;
    if (!eglChooseConfig(egl->display, WS_EGL_ConfigAttribs, &(egl->config), 1, &matched)) {
        printf("failed to chose config\n");
    }
    
    if (matched == 0) {
        printf("failed to match egl config\n");
    }
    
    egl->context = (EGLContext*)eglCreateContext(egl->display, egl->config, EGL_NO_CONTEXT, WS_EGL_ContextAttribs);
    if (egl->context == NULL || egl->context == EGL_NO_CONTEXT) {
        printf("failed to create context: %s\n", eglGetErrorString(eglGetError()));
    }
}
void WS_EGL_Finish(WS_EGL_Egl* egl) {
	eglMakeCurrent(egl->display, EGL_NO_SURFACE,
		EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(egl->display, egl->context);
	eglTerminate(egl->display);
	eglReleaseThread();
}
#endif // WOIDSHELL_EGL_IMPLEMENTATION
#endif // WOIDSHELL_EGL_H
