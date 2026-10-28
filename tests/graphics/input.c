/** Injects native SDL events to exercise the public host's dispatch and capture
 * policy. */
#include <SDL3/SDL.h>
/** Sends an event to the current test window, never directly to the graphics
 * queue. */
int graphics_test_event(int kind, int code, double x, double y) {
  int count = 0;
  SDL_Window **windows = SDL_GetWindows(&count);
  if (!windows || count != 1) {
    SDL_free(windows);
    return 0;
  }
  SDL_WindowID id = SDL_GetWindowID(windows[0]);
  SDL_free(windows);
  SDL_Event event = {0};
  if (kind == 1) {
    event.type = x != 0 ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.windowID = id;
    event.key.key = code;
    event.key.down = x != 0;
  } else if (kind == 2) {
    event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    event.window.windowID = id;
  } else if (kind == 3) {
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = id;
    event.motion.xrel = (float)x;
    event.motion.yrel = (float)y;
  } else if (kind == 4) {
    event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    event.window.windowID = id;
  } else {
    return 0;
  }
  return SDL_PushEvent(&event);
}
