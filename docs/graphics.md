# OpenGL viewports and separate windows

The optional graphics bundles let one Sun application render its own scene inside
GTK and in a separate SDL3 window. GTK remains the sun_ui widget renderer. The
3D scene uses its own owned frames rather than a tree of UI elements.

## Build and run

Install SDL3 and libepoxy development packages in addition to the existing GTK
build dependencies. On Linux, install `libdecor-0-plugin-1-cairo` for Wayland. The
development image includes these packages. On macOS, use Homebrew's `sdl3` and
`libepoxy` alongside the project's existing GTK dependencies.

Build the updated sun_gobject checkout first: the GTK adapter now requires its
checked `GLArea` and widget-reference APIs. Then, from the sun_ui workspace root:

```sh
./build.sh --graphics
./build/graphics
```

The example displays a cube and ground grid in both hosts. GTK buttons change the
shared color and pause rotation. Drag inside the embedded viewport to look around.
Click inside the SDL window to capture the mouse, use WASD to move, and press
Escape to release capture. Focus loss releases capture and clears held keys.
Closing either window leaves the other usable.

Core-only and ordinary GTK builds do not require SDL. The graphics build produces:

- `sun_ui_graphics.moon`: owned assets, frames, surface status, and input.
- `sun_ui_graphics_gtk.moon`: checked GTK viewport integration.
- `sun_ui_sdl.moon`: separate SDL3 window hosting without a GTK dependency.
- `graphics-native/libsun_ui_graphics.a` and `graphics-native/libsun_ui_sdl.a`:
  native execution and host interoperability.

Consumers link the native archives and the system libraries selected by
`pkg-config --libs epoxy glib-2.0 sdl3`; GTK consumers also use the normal GTK and
sun_gobject linker dependencies. The SDL archive is needed only by SDL consumers.
CI uploads the optional graphics bundles and native archives separately. All Sun
bundles must use compatible compiler and dependency versions.

## Ownership and drawing API

`gpu.Mesh` copies packed XYZ positions from `Vec<f64>` and optional indices from
`Vec<i64>`. Position data is converted to OpenGL floats after validation. Attribute
location zero contains position triples. `gpu.Shader` copies GLSL source; each
surface compiles it in its own context. The baseline is desktop OpenGL 3.3 core
and GLSL 330. This initial API does not provide textures or a complete scene engine.

`gpu.Frame` owns its clear color and draw commands. `draw` retains immutable mesh
and shader assets, and selects triangles or lines, depth testing, and blending.
`uniform` copies a scalar, vector, or column-major matrix onto the most recent
draw. Missing or optimized-out shader uniforms are ignored by OpenGL; invalid
value shapes and nonfinite values are rejected. No attributes, native handles, or
GPU ownership enter ordinary sun_ui component descriptions.

Submitting a frame snapshots its commands and uniform values. Later edits to the
builder do not change the submitted packet. The most recent packet replaces any
pending packet and remains available for exposure redraws. Local frame, mesh, and
shader variables may be released after submission. CPU assets are retained;
programs and buffers are cached separately for each context and unused entries
are evicted after a successful frame. Context recreation rebuilds GPU resources
from the retained descriptions. Context errors and shader diagnostics are exposed
through host `error()` methods; SDL render errors also return `SystemResult.Error`.

## Register an embedded viewport

```sun
var viewport = try gpu_gtk.Viewport();
try renderer.register_viewport(1, viewport.widget());

// In the view function:
var element = ui.Element(ui.Key(42), ui.Kind.Viewport);
element.viewport_id = 1;
element.expand_x = true;
element.expand_y = true;
view.append(-1, element);

// At the application's frame boundary:
viewport.submit(frame);
```

The XML equivalent is `<Viewport key="scene" viewportId="1" expandX="true"/>`.
A viewport is a leaf. Its identifier must be positive, registered before mounting,
and unique within a view. The GTK adapter rejects registering the same surface
under multiple identifiers and mounting an already-mounted registration.

Keyed property updates and sibling reorders preserve the surface. Changing the
identifier replaces the surface association. `renderer.unregister_viewport(id)`
requires the viewport to be unmounted first. `renderer.viewport(id)` returns an
explicitly retained checked GTK area for redraw requests and context diagnostics.
The application keeps its `gpu_gtk.Viewport` to submit frames and inspect graphics
status. `viewport.surface()` returns an explicitly retained handle to that same
stable graphics state, not a replacement surface.

## Run both event sources

Keep both hosts on the main thread. Pump GTK without blocking, dispatch semantic
UI messages, poll SDL, update the scene, submit frames, and render the SDL window.
Use a short timed wait between iterations. Do not enter two blocking application
loops or run either host's GUI operations on a worker thread.

GTK and SDL cache their current contexts. The GTK adapter clears GDK's context at
the end of `poll` and `wait_events`; the SDL host releases its context after each
render. Call `gtk.clear_gl_context()` after initial GTK window setup and before
opening an SDL window, or after any manually driven GTK iteration. The demo shows
the complete loop. GTK executes its packet inside the GLArea render callback;
SDL executes the same packet format before presenting its window.

`surface().next()` returns a dedicated graphics event, independent of sun_ui's
semantic queue. It contains a kind, code, and two numeric fields:

| Kind | Code | x / y |
| --- | --- | --- |
| Move | unused | Absolute logical position |
| Relative | unused | Relative movement, SDL capture only |
| Button | positive press, negative release | Pointer position |
| Key | Unicode/key code; Escape is 27 | x is pressed state |
| Scroll | unused | Scroll deltas |
| Focus | 1 gained, 0 lost | unused |
| Close | unused | unused |

Queue overflow drops old events and inserts a focus-loss reset so applications
can clear held-input state. FPS capture is supplied by the SDL host. GTK input is
ordinary focused keyboard and pointer input, including drag-to-look; it does not
provide unrestricted mouse capture. Input injection tests verify dispatcher
behavior, not physical device behavior on every compositor.

## Wayland decoration compatibility

The GTK libdecor plugin links GTK 3 and must not be loaded into this GTK 4 process.
Disabling decorations through SDL's hint was insufficient in the tested SDL build.
The graphics build creates `build/libdecor/libdecor-cairo.so` as a link to the
installed Cairo plugin when found. The SDL host selects this directory next to
the executable before SDL initialization. An application installed elsewhere can
supply its own isolated directory explicitly:

```sh
LIBDECOR_PLUGIN_DIR=/absolute/path/to/cairo-only-plugins ./build/graphics
```

The directory must contain only a compatible Cairo plugin, not `libdecor-gtk.so`.
Missing or conflicting configuration produces an error instead of silently
switching display backends. A custom plugin installation can be selected when
configuring the native build with `-DGU_CAIRO_PLUGIN=/path/to/libdecor-cairo.so`.
Do not ship a development-directory link as a portable application dependency;
create an appropriate installation-time link or set the runtime directory.

The Linux SDL host uses EGL for OpenGL context creation; the tested Xvfb GLX path
could not find a matching visual. This does not force an X11 session. The Linux
automated acceptance command explicitly selects X11/Xvfb with software OpenGL.
Run the actual `build/graphics-tests` binary in a Wayland session to test Wayland.

## Validation and dependency rollout

The graphics acceptance program uses the real public APIs. It checks retained
frame/asset lifetimes, pending-frame replacement, shader errors and recovery,
scalar/vector/matrix uniforms, pixel readback, viewport registration, keyed
reordering, identifier replacement, unmount/remount, control events, native SDL
input, capture release, and continued rendering after either host closes.
Core tests cover platform-independent viewport validation; XML tests cover the
new element and identifier property. Production rendering does not perform pixel
readback unless explicitly enabled for tests.

The temporary native coexistence probe is superseded by these tests. macOS
compatibility is checked by the main CI graphics job, not locally in the Linux
workspace. Physical capture behavior and mixed-DPI displays still need interactive
checks on the target desktop. Test failures on macOS must be reported and fixed;
the feature does not silently remove that platform or move rendering to another
process.

The checked GTK binding changes belong to the independent sun_gobject checkout.
They must be available on that repository's tracked branch before parent CI can
build this version of the GTK adapter. Toolkit core tests remain independent of
that dependency and of the graphics system libraries.
