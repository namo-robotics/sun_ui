# sun_plot

Scientific line and scatter plots for Sun programs, built on sun_ui's vector
drawing API. The library produces labelled linear axes, automatic or explicit
limits, readable decimal ticks, grid lines, multiple series, and legends. Figures
export as SVG with selectable text and display in a resizable GTK window.

## Build and run

From the repository root:

```sh
./build.sh
./build/plot-export                 # writes build/scientific-plot.svg
vcs import --skip-existing --input deps.repos .
bash src/sun_gobject/build.sh
./build.sh --plot-gtk               # builds and tests the GTK vector viewer
./build/plot-viewer
```

In the viewer, click **Export SVG…** to choose a filename and destination in the
native Save dialog. Export saves the current figure at its original logical
dimensions, with vector paths and selectable text. The toolbar reports success
or failure; canceling the dialog leaves the figure unchanged.

The default build and plotting tests need only Sun. The viewer uses the checked
GTK bindings in sun_gobject, built with the same Sun compiler. On Debian/Ubuntu,
it also needs `libgtk-4-dev`, `librsvg2-2`, and a font such as `fonts-dejavu-core`.
Its acceptance test uses Xvfb, xauth, and dbus-run-session. The development image
includes these dependencies. This initial native build targets Linux.

## Create a figure

```sun
var figure = plot.Figure(String("Experiment"), String("Time (s)"), String("Distance (m)"));
var series = plot.Series(String("Measurements"), String("#2364aa"), false);
series.add(0.0, 0.0);
series.add(1.0, 2.0);
series.add(2.0, 3.0);
figure.add(series);
var drawing = try figure.draw(800.0, 600.0);
var path = String("build/experiment.svg");
try drawing.save(path);
```

Import `stdlib.moon`, `sun_ui.moon`, and `sun_plot.moon` in the program manifest.
The final `Series` constructor argument selects scatter (`true`) or lines
(`false`). `line_width` and `marker_radius` control appearance; colors are SVG/CSS
color strings. An empty series label omits its legend entry. Set `figure.grid`
to `false` to hide grid lines.

Override either axis independently:

```sun
figure.x_limits = Option.Some(plot.Limits(0.0, 10.0));
figure.y_limits = Option.Some(plot.Limits(-2.0, 8.0));
```

For a window, also import `sun_ui_gtk_drawing.moon`, `sun_gobject.moon`, and
`sun_gtk.moon`, and link their native dependencies
as shown in [the build tool](../../tools/build.sun):

```sun
var title = String("Experiment");
var window = try ui_gtk_drawing.Window(title, drawing);
while (window.running()) {
  window.poll(true);
}
```

After changing the figure, call `figure.draw(...)` and `window.update(...)` again.
The window owns a parsed copy, so the original drawing can be released after
creation or update. Polling and all window operations must occur on the GTK
thread. Window destruction releases the drawing and native resources.

See [the export example](../../examples/plot/export.sun) and
[the viewer](../../examples/plot/viewer.sun), which share a four-panel damped
oscillator study in [data.sun](../../examples/plot/data.sun): transient response,
phase portrait, energy dissipation, and resonance spectrum. Teal and orange
identify light and strong damping throughout. The time-domain curves use a
small-step symplectic Euler simulation; the spectrum uses the analytical power
response. All data is synthetic and deterministic.

## Compose subplots

`plot.Grid` takes rendered panels in row-major order. Each panel retains its own
axes, clipping, labels, and legend:

```sun
var grid = plot.Grid(2, 2, String("Experiment"), String("Four complementary views"));
grid.add(try first.draw(684.0, 471.0));
grid.add(try second.draw(684.0, 471.0));
grid.add(try third.draw(684.0, 471.0));
grid.add(try fourth.draw(684.0, 471.0));
var drawing = try grid.draw(1440.0, 1100.0);
```

Supply exactly one panel per cell. The grid reserves space for its heading and
gutters; each cell must be at least 400×300 logical pixels. Resizing fits existing
panels into the cells while preserving their aspect ratios. To change panel data,
render a replacement drawing and assign it to the corresponding `grid.panels`
entry before drawing the grid again.

## Ownership and architecture

`Figure` owns its series and labels; each `Series` owns its samples. Adding a
series transfers ownership. Rendering borrows the figure and returns an owned
`ui_drawing.Drawing`. No native handle appears in the plotting API.

The dependency direction is `sun_plot → sun_ui`. The core UI has no dependency
on plotting. Plot sources, tests, and examples have dedicated directories, and
`sun_plot.moon` is built separately so they can move into a separate repository.

The drawing module lives in `src/drawing.sun`; its SVG primitives support lines,
circles, text, and clipping. SVG export and the GTK viewer use the same document.
GTK uses librsvg to render vectors at the current allocation while preserving
aspect ratio. Resize scales the figure; it does not recompute axes layout.

The standalone drawing window is separate from sun_ui's button reconciliation
API. `src/adapters/gtk/drawing.sun` composes safe `gtk.Label`, `gtk.DrawingArea`,
`gtk.SaveDialog`, and existing widget wrappers. Native declarations, references,
callbacks, and thread checks live in sun_gobject's `sun_gtk` package; SVG ownership
lives in its `svg` module. The adapter owns toolbar layout, export messages,
filesystem writes, and polling policy. Composing drawings with reconciled controls
remains on [the roadmap](ROADMAP.md).

## Numerical and layout limits

- Axes are linear. Non-finite paired samples are excluded from automatic limits;
  they break lines and are skipped in scatter plots.
- Empty axes default to `[0, 1]`. Constant data receives a nonzero interval.
- Explicit limits must be finite and ascending, with a finite span of at least
  `1e-280`. Unsupported extreme offsets and overflowing ranges return errors.
- Plot geometry is clipped to the axes rectangle. Markers on the boundary may be
  partially clipped. A line series needs two adjacent finite points to draw a
  segment.
- Figure dimensions range from 400×300 through 16384×16384 logical pixels. The
  figure must be tall enough for its legend. Invalid sizes and nonpositive or
  non-finite stroke widths and marker radii return errors.
- Margins and text sizes are currently fixed. Long labels, dense tick labels,
  or large legends may require a larger figure. Automatic font-measured layout
  is not implemented yet; SVG text appearance also depends on installed fonts.
- Each render allocates an SVG document proportional to the number of samples.
  Large-data reduction and interactive pan/zoom are not implemented.
