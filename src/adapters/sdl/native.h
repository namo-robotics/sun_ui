/** Main-thread SDL3 window hosting for the shared graphics executor. */
#pragma once
#include "../../graphics/native/graphics.h"
/** Owns an SDL window, context, and retained graphics surface. */
typedef struct GuWindow GuWindow;
/** Opens a separate window; errors are available through gu_window_error with
 * null. */
GuWindow *gu_window_new(GuSurface *surface, const char *title, int width,
                        int height);
/** Releases native resources and the retained surface exactly once. */
void gu_window_free(GuWindow *window);
/** Closes a window while leaving its wrapper and other hosts usable. */
void gu_window_close(GuWindow *window);
/** Processes a bounded batch of events for every SDL window. */
void gu_window_poll(void);
/** Draws and presents the latest frame, releasing SDL's current context
 * afterwards. */
int gu_window_render(GuWindow *window);
/** Reports whether the window remains open. */
int gu_window_open(GuWindow *window);
/** Copies no data; the diagnostic remains valid until the next host operation.
 */
const char *gu_window_error(GuWindow *window);
/** Enables or releases relative pointer capture, reporting platform failure. */
int gu_window_capture(GuWindow *window, int enabled);
/** Reports the currently requested relative pointer mode. */
int gu_window_captured(GuWindow *window);
/** Requests a logical window size for applications and tests. */
int gu_window_resize(GuWindow *window, int width, int height);
/** Sleeps for a bounded frame interval without pumping either host recursively.
 */
void gu_window_delay(int milliseconds);
/** Returns a monotonic time for frame scheduling. */
double gu_window_time(void);
