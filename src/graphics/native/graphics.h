/** Owned CPU frames and context-local OpenGL execution, independent of window
 * hosts. */
#pragma once
#include <stdint.h>
/** Immutable CPU geometry with packed position triples and optional indices. */
typedef struct GuMesh GuMesh;
/** Immutable vertex and fragment shader source. */
typedef struct GuShader GuShader;
/** An owned drawing packet retaining its geometry, shaders, and uniform values.
 */
typedef struct GuFrame GuFrame;
/** Context-local GPU cache, submitted frame, input queue, and diagnostics. */
typedef struct GuSurface GuSurface;
/** Validates length-delimited text before crossing the C string boundary. */
int gu_text_valid(const char *text, uint64_t length);
/** Checks that graphics operations run on the process main thread. */
void gu_thread(void);
/** Allocates geometry storage; invalid sizes return null. */
GuMesh *gu_mesh_new(int vertices, int indices);
/** Initializes one position component before publishing the immutable mesh. */
void gu_mesh_vertex(GuMesh *mesh, int index, float value);
/** Initializes one index before publishing the immutable mesh. */
void gu_mesh_index(GuMesh *mesh, int index, unsigned value);
/** Releases a CPU geometry reference. */
void gu_mesh_free(GuMesh *mesh);
/** Copies source text for deferred compilation in each context. */
GuShader *gu_shader_new(const char *vertex, const char *fragment);
/** Releases one shader source reference. */
void gu_shader_free(GuShader *shader);
/** Allocates an owned frame with its clear color. */
GuFrame *gu_frame_new(float r, float g, float b, float a);
/** Releases a frame and all retained draw resources. */
void gu_frame_free(GuFrame *frame);
/** Appends a validated draw; mode is zero for triangles or one for lines. */
int gu_frame_draw(GuFrame *frame, GuMesh *mesh, GuShader *shader, int mode,
                  int depth, int blend);
/** Copies a scalar, vector, or matrix uniform onto the most recent draw. */
int gu_frame_uniform(GuFrame *frame, const char *name, int count,
                     const float *values);
/** Copies a single component into a temporary uniform builder. */
void gu_uniform_value(int index, float value);
/** Copies the main-thread temporary builder into the most recent draw. */
int gu_frame_uniform_built(GuFrame *frame, const char *name, int count);
/** Allocates a surface with no GPU resources until its first render. */
GuSurface *gu_surface_new(void);
/** Retains a surface for a native host callback. */
void gu_surface_ref(void *surface);
/** Drops a surface reference; hosts must reset its cache before destroying the
 * context. */
void gu_surface_free(void *surface);
/** Replaces a pending frame by retaining an immutable snapshot of its commands.
 */
void gu_surface_submit(GuSurface *surface, GuFrame *frame);
/** Executes the latest frame in the current host framebuffer; returns success.
 */
int gu_surface_render(void *surface, int width, int height);
/** Clears context-local objects; live indicates whether GL calls are still
 * valid. */
void gu_surface_reset(void *surface, int live);
/** Returns the latest stable diagnostic string, empty on success. */
const char *gu_surface_error(GuSurface *surface);
/** Returns completed render count. */
int gu_surface_frames(GuSurface *surface);
/** Returns the last framebuffer dimensions. */
int gu_surface_extent(GuSurface *surface, int horizontal);
/** Returns one channel of the last center pixel for acceptance checks. */
int gu_surface_pixel(GuSurface *surface, int channel);
/** Enables deterministic readback for tests, disabled for normal rendering. */
void gu_surface_readback(GuSurface *surface, int enabled);
/** Queues a host input event; kinds are move, button, key, scroll, focus,
 * relative, close. */
void gu_surface_input(void *surface, int kind, int code, double x, double y);
/** Pops one input record into a stable read slot. */
int gu_surface_next(GuSurface *surface);
/** Reads the last popped event's integer or coordinate fields. */
double gu_surface_event(GuSurface *surface, int field);
