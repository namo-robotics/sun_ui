/** Hosts shared drawing packets without introducing a GTK dependency. */
#include "native.h"
#include <SDL3/SDL.h>
#include <glib.h>
#include <string.h>
/** Keeps one window's state distinct from SDL's process-global event queue. */
struct GuWindow {
  SDL_Window *window;
  SDL_GLContext context;
  GuSurface *surface;
  char error[2048];
  Uint32 id;
};
static GPtrArray *windows;
static char startup_error[2048];
/** Copies an SDL failure before a later operation can overwrite it. */
static int error(GuWindow *w) {
  g_strlcpy(w ? w->error : startup_error, SDL_GetError(), 2048);
  return 0;
}
GuWindow *gu_window_new(GuSurface *surface, const char *title, int width,
                        int height) {
  gu_thread();
  startup_error[0] = 0;
  if (width <= 0 || height <= 0) {
    g_strlcpy(startup_error, "Window dimensions must be positive", 2048);
    return NULL;
  }
  if (!windows) {
    /* The application, not this optional host, owns process signal policy. */
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
#ifdef __linux__
    /* SDL may load libdecor during initialization even when decorations are
     * disabled. */
    const char *driver = g_getenv("SDL_VIDEO_DRIVER");
    if (g_getenv("WAYLAND_DISPLAY") && (!driver || strstr(driver, "wayland"))) {
      if (!g_getenv("LIBDECOR_PLUGIN_DIR")) {
        char *directory = g_build_filename(SDL_GetBasePath(), "libdecor", NULL);
        char *plugin = g_build_filename(directory, "libdecor-cairo.so", NULL);
        if (g_file_test(plugin, G_FILE_TEST_EXISTS))
          g_setenv("LIBDECOR_PLUGIN_DIR", directory, FALSE);
        g_free(plugin);
        g_free(directory);
      }
      const char *directory = g_getenv("LIBDECOR_PLUGIN_DIR");
      char *gtk_plugin =
          directory ? g_build_filename(directory, "libdecor-gtk.so", NULL)
                    : NULL;
      int conflict = gtk_plugin && g_file_test(gtk_plugin, G_FILE_TEST_EXISTS);
      g_free(gtk_plugin);
      if (!directory || conflict) {
        g_strlcpy(startup_error,
                  "Wayland requires LIBDECOR_PLUGIN_DIR containing only the "
                  "Cairo plugin; GTK3 libdecor cannot coexist with GTK4",
                  2048);
        return NULL;
      }
    }
    SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "1");
#endif
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
      error(NULL);
      return NULL;
    }
    windows = g_ptr_array_new();
  }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,
                      SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  GuWindow *w = g_new0(GuWindow, 1);
  w->surface = surface;
  gu_surface_ref(surface);
  w->window = SDL_CreateWindow(title, width, height,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                   SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (w->window)
    w->context = SDL_GL_CreateContext(w->window);
  if (!w->context) {
    error(NULL);
    gu_window_free(w);
    return NULL;
  }
  w->id = SDL_GetWindowID(w->window);
  SDL_GL_SetSwapInterval(0);
  if (!SDL_GL_MakeCurrent(w->window, NULL)) {
    error(NULL);
    gu_window_free(w);
    return NULL;
  }
  g_ptr_array_add(windows, w);
  return w;
}
void gu_window_close(GuWindow *w) {
  gu_thread();
  if (!w || !w->window)
    return;
  SDL_SetWindowRelativeMouseMode(w->window, false);
  if (w->context) {
    int live = SDL_GL_MakeCurrent(w->window, w->context);
    gu_surface_reset(w->surface, live);
    SDL_GL_MakeCurrent(w->window, NULL);
    SDL_GL_DestroyContext(w->context);
    w->context = NULL;
  }
  SDL_DestroyWindow(w->window);
  w->window = NULL;
  gu_surface_input(w->surface, 5, 0, 0, 0);
}
void gu_window_free(GuWindow *w) {
  gu_thread();
  if (!w)
    return;
  gu_window_close(w);
  if (windows)
    g_ptr_array_remove(windows, w);
  gu_surface_free(w->surface);
  g_free(w);
  if (windows && windows->len == 0) {
    g_ptr_array_free(windows, TRUE);
    windows = NULL;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
  }
}
int gu_window_capture(GuWindow *w, int enabled) {
  gu_thread();
  w->error[0] = 0;
  if (!w->window) {
    g_strlcpy(w->error, "Window is closed", sizeof(w->error));
    return 0;
  }
  if (!SDL_SetWindowRelativeMouseMode(w->window, enabled != 0))
    return error(w);
  return 1;
}
void gu_window_poll(void) {
  gu_thread();
  if (!windows)
    return;
  SDL_Event e;
  for (int n = 0; n < 64 && SDL_PollEvent(&e); n++) {
    for (guint i = 0; i < windows->len; i++) {
      GuWindow *w = g_ptr_array_index(windows, i);
      if (!w->window)
        continue;
      if (e.type == SDL_EVENT_QUIT) {
        gu_surface_input(w->surface, 7, 0, 0, 0);
        gu_window_close(w);
        continue;
      }
      Uint32 id = 0;
      if (e.type >= SDL_EVENT_WINDOW_FIRST && e.type <= SDL_EVENT_WINDOW_LAST)
        id = e.window.windowID;
      else if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP)
        id = e.key.windowID;
      else if (e.type == SDL_EVENT_MOUSE_MOTION)
        id = e.motion.windowID;
      else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
               e.type == SDL_EVENT_MOUSE_BUTTON_UP)
        id = e.button.windowID;
      else if (e.type == SDL_EVENT_MOUSE_WHEEL)
        id = e.wheel.windowID;
      if (id != w->id)
        continue;
      switch (e.type) {
      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        gu_surface_input(w->surface, 7, 0, 0, 0);
        gu_window_close(w);
        break;
      case SDL_EVENT_WINDOW_FOCUS_LOST:
        gu_window_capture(w, 0);
        gu_surface_input(w->surface, 5, 0, 0, 0);
        break;
      case SDL_EVENT_WINDOW_FOCUS_GAINED:
        gu_surface_input(w->surface, 5, 1, 0, 0);
        break;
      case SDL_EVENT_KEY_DOWN:
      case SDL_EVENT_KEY_UP:
        if (e.key.key == SDLK_ESCAPE && e.key.down)
          gu_window_capture(w, 0);
        gu_surface_input(w->surface, 3, e.key.key, e.key.down, 0);
        break;
      case SDL_EVENT_MOUSE_MOTION:
        gu_surface_input(
            w->surface, SDL_GetWindowRelativeMouseMode(w->window) ? 6 : 1, 0,
            SDL_GetWindowRelativeMouseMode(w->window) ? e.motion.xrel
                                                      : e.motion.x,
            SDL_GetWindowRelativeMouseMode(w->window) ? e.motion.yrel
                                                      : e.motion.y);
        break;
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
      case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.down && e.button.button == 1)
          gu_window_capture(w, 1);
        gu_surface_input(w->surface, 2,
                         e.button.down ? e.button.button
                                       : -(int)e.button.button,
                         e.button.x, e.button.y);
        break;
      case SDL_EVENT_MOUSE_WHEEL:
        gu_surface_input(w->surface, 4, 0, e.wheel.x, e.wheel.y);
        break;
      default:
        break;
      }
    }
  }
}
int gu_window_render(GuWindow *w) {
  gu_thread();
  if (!w->window)
    return 1;
  if (SDL_GetWindowFlags(w->window) &
      (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED))
    return 1;
  int width, height;
  if (!SDL_GetWindowSizeInPixels(w->window, &width, &height))
    return error(w);
  if (width <= 0 || height <= 0)
    return 1;
  if (!SDL_GL_MakeCurrent(w->window, w->context))
    return error(w);
  int ok = gu_surface_render(w->surface, width, height);
  if (ok && !SDL_GL_SwapWindow(w->window))
    ok = error(w);
  if (!SDL_GL_MakeCurrent(w->window, NULL))
    ok = error(w);
  return ok;
}
int gu_window_open(GuWindow *w) {
  gu_thread();
  return w && w->window;
}
const char *gu_window_error(GuWindow *w) {
  gu_thread();
  return w ? w->error : startup_error;
}
int gu_window_resize(GuWindow *w, int width, int height) {
  gu_thread();
  if (!w->window || width <= 0 || height <= 0) {
    g_strlcpy(w->error, "Invalid resize or closed window", sizeof(w->error));
    return 0;
  }
  return SDL_SetWindowSize(w->window, width, height) ? 1 : error(w);
}
void gu_window_delay(int ms) {
  gu_thread();
  if (ms > 0)
    SDL_Delay(MIN(ms, 100));
}
double gu_window_time(void) {
  gu_thread();
  return (double)SDL_GetTicksNS() / 1000000000.0;
}

int gu_window_captured(GuWindow *w) {
  gu_thread();
  return w->window && SDL_GetWindowRelativeMouseMode(w->window);
}
