#include <EGL/egl.h> 
#include <SDL3/SDL_pixels.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wayland-client-protocol.h>
#include <wayland-util.h>
#define WOIDSHELL_OPENGL33
#define WOIDSHELL_IMPLEMENTATION
#define WOIDSHELL_INPUT
#include "./protocol/wlr-layer-shell-unstable-v1.h"
#include "./include/woidshell.h"
#include "./include/glad/glad.h"
#include <linux/input-event-codes.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "./style.h"
#define NOB_IMPLEMENTATION
#include "./nob.h"
#include "scancodes.h"
#include "worker.h"

const char* VertexSrc = R"(
#version 330 core
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;
out vec2 TexCoord;
void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    TexCoord = texCoord;
}
)";

const char* TexureFragmentSrc = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D ourTexture;
void main() {
    FragColor = texture(ourTexture, TexCoord);
}
)";

const char* ColorFragmentSrc = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
uniform vec4 color;
void main() {
    FragColor = color;
}
)";

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        printf("ERROR::SHADER::COMPILATION_FAILED: %s\n", infoLog);
    }
    return shader;
}

GLuint createShaderProgram(const char* vertexShaderSource, const char* fragmentShaderSource) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    int success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        printf("ERROR::PROGRAM::LINKING_FAILED\n");
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

void drawTexturedRectangle(float x, float y, float w, float h, GLuint texture) {
    float vertices[] = {
        x,     y,     0.0f, 1.0f,
        x + w, y,     1.0f, 1.0f,
        x,     y + h, 0.0f, 0.0f,
        x + w, y + h, 1.0f, 0.0f
    };

    unsigned int indices[] = {
        0, 1, 2,
        1, 3, 2
    };

    GLuint VBO, VAO, EBO;
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteBuffers(1, &VAO);
}


WS_Shell shell = {0};
GLuint texture_prog;
GLuint color_prog;  
TTF_Font* font;
Nob_String_Builder input = {0};
Nob_File_Paths files = {0};
int found = -1;
key_t queue_key;
int queue_id;

void DrawText(float x, float y, char* text, TTF_Font* font, SDL_Color color) {
    SDL_Surface* text_surface = TTF_RenderText_Blended(font, text, 0, color);
    text_surface = SDL_ConvertSurface(text_surface, SDL_PIXELFORMAT_RGBA32);

    GLuint texutre;
    glGenTextures(1, &texutre);
    glBindTexture(GL_TEXTURE_2D, texutre);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, text_surface->w, text_surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, text_surface->pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    drawTexturedRectangle(x, y, (float)text_surface->w/shell.settings->width, (float)text_surface->h/shell.settings->height, texutre);
    glDeleteTextures(1, &texutre);
}

void get_executable_files(const char* dir_path, Nob_File_Paths* files) {
    struct dirent *entry;
    struct stat file_stat;
    DIR *dir = opendir(dir_path);

    if (dir == NULL) {
        perror("Unable to open directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        
        Nob_String_Builder full_path = {0};
        nob_sb_append_cstr(&full_path, dir_path);
        nob_sb_append_cstr(&full_path, entry->d_name);

        if (stat(full_path.items, &file_stat) == 0) {
            if (S_ISREG(file_stat.st_mode) && access(full_path.items, X_OK) == 0) {
                nob_da_append(files, full_path.items);
            }
        }
    }

    closedir(dir);
}

int search_files(Nob_File_Paths* haystack, char* needle) {
    for (int i = 0; i<haystack->count; i++) {
        if (strstr(haystack->items[i], needle)) {
            return i;
        }
    }   
    return -1;
}

void launch(const char* path) {
    QueueMessage msg = {9};
    msg.type = 1;
    strncpy(msg.text, path, sizeof(msg.text)-1);
    msg.text[sizeof(msg.text)-1] = '\0';
    msgsnd(queue_id, &msg, sizeof(msg), 0);
}

void drawfunc(void* data) {
    WS_Shell* shell = (WS_Shell*)data;
    eglMakeCurrent(shell->egl->display, shell->egl->surface, shell->egl->surface, shell->egl->context);
    glViewport(0, 0, shell->settings->width, shell->settings->height);
    glClearColor((float)BG_COLOR.r/255, (float)BG_COLOR.g/255, (float)BG_COLOR.b/255, (float)BG_COLOR.a/255);
    glClear(GL_COLOR_BUFFER_BIT);
    
    if(input.count>0) {
        DrawText(-1, 0.6, input.items, font, TEXT_COLOR);
        
        if(found != -1) {
            DrawText(-1, 0.3, nob_temp_file_name(files.items[found]), font, TEXT_COLOR);
        } else {
            DrawText(-1, 0.3, "nothing found", font, TEXT_COLOR);
        }
    } else {
        DrawText(-1, 0.6, "wussup my mane! watchu trina launch?", font, TEXT_COLOR);
    }

    WS_ExecFrameCallback(shell); 
}

static void keyboard_key(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
    if (state) {
        if (key == KEY_ESC) {
            printf("quiting\n");
            exit(0);
        }

        if (input.count > 0 && key == KEY_BACKSPACE) {
            input.count --;
            input.items[input.count] = '\0';
        } else if (key == KEY_ENTER && found != -1) {
            printf("launching: %s\n", files.items[found]);
            launch(files.items[found]);
            shell.run = false;
            exit(0);
        } else {
            nob_sb_append(&input, scancode_to_ascii(key));
            input.items[input.count] = '\0';
        }
        found = search_files(&files, input.items);
        printf("key: %2x\n", key);
    }
}

static void keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size) {}
static void keyboard_enter(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {}
static void keyboard_leave(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface) {}
static void keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {}
static void keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard, int32_t rate, int32_t delay) {}

static void pointer_axis_descrete(void* data, struct wl_pointer* pointer, uint32_t timestamp, int32_t axis) {printf("scrollin_descrete: %d; %d\n", timestamp, axis);}
static void pointer_axis(void* data, struct wl_pointer* pointer, uint32_t timestamp, uint32_t axis, wl_fixed_t vector_len) {}
static void pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t pressed) {}
static void pointer_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {}
static void pointer_frame(void *data, struct wl_pointer *wl_pointer) {}
static void pointer_axis_source(void *data, struct wl_pointer *wl_pointer, uint32_t axis_source) {}
static void pointer_axis_stop(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis) {}
static void pointer_enter(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {}
static void pointer_leave(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface) {}
static void pointer_axis_value120(void *data, struct wl_pointer *wl_pointer, uint32_t axis, int32_t value120) {}
static void pointer_axis_relative_direction(void* data, struct wl_pointer* pointer, uint32_t axis, uint32_t direction) {}


int main(void) {
    WS_ShellSettings settings = {
        .layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP, 
        .width = 500,
        .height = 100,
        .anchor = 0,
        .exclusive_zone = 0,
        .margin_l = 0,
        .margin_b = 0,
        .margin_t = 0,
        .margin_r = 0,
        .drawfunc = drawfunc, 
        .input.keyboard_interactivity = ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE, 

        .input.keyboard_events.key = keyboard_key,
        .input.keyboard_events.keymap = keyboard_keymap,
        .input.keyboard_events.enter = keyboard_enter,
        .input.keyboard_events.leave = keyboard_leave,
        .input.keyboard_events.modifiers = keyboard_modifiers,
        .input.keyboard_events.repeat_info = keyboard_repeat_info,

        .input.pointer_events.axis = pointer_axis,
        .input.pointer_events.axis_discrete = pointer_axis_descrete,
        .input.pointer_events.button = pointer_button,
        .input.pointer_events.frame = pointer_frame,
        .input.pointer_events.axis_source = pointer_axis_source,
        .input.pointer_events.axis_stop = pointer_axis_stop,
        .input.pointer_events.enter = pointer_enter,
        .input.pointer_events.leave = pointer_leave,
        .input.pointer_events.motion = pointer_motion,
        .input.pointer_events.axis_value120 = pointer_axis_value120,
        .input.pointer_events.axis_relative_direction = pointer_axis_relative_direction,
    };
    
    shell.settings = &settings;

    WS_ShellInit(&shell);
    eglMakeCurrent(shell.egl->display, shell.egl->surface, shell.egl->surface, shell.egl->context);
    if (!gladLoadGLLoader((GLADloadproc)eglGetProcAddress)) {
        printf("Failed to load opengl extentions\n");
        exit(1);
    }
    glViewport(0, 0, shell.settings->width, shell.settings->height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    texture_prog = createShaderProgram(VertexSrc, TexureFragmentSrc);
    color_prog = createShaderProgram(VertexSrc, ColorFragmentSrc);
    glUseProgram(texture_prog);
    TTF_Init();
    font = TTF_OpenFont(FONT_PATH, 32);

    printf("\n\n PRESS Q TO QUIT\n\n");
    get_executable_files("/usr/bin/", &files);

    queue_key = ftok(SOFI_WORKER_QUEUE_NAME, SOFI_WORKER_QUEUE_ID);
    queue_id = msgget(queue_key, 0666 | IPC_CREAT);

    drawfunc(&shell);

    while (!WS_ShellShouldClose(&shell)) {} 
    TTF_Quit();                                        
    WS_ShellDestroy(&shell);
    return 0;
}
