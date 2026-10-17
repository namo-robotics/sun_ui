# sun_plot roadmap

## UI integration and layout

- Move the GTK drawing and librsvg ownership wrappers into sun_gtk, then consume
  those checked APIs from the sun_ui adapter.
- Support drawing surfaces alongside controls in sun_ui's reconciled views.
- Measure fonts when laying out labels, ticks, and legends; handle long labels
  and dense ticks without overlap.
- Recompute layout for available space when desired, alongside fixed-size figure
  scaling for publication previews.
- Add render-error reporting and enforce native thread affinity in the bindings.

## Scientific plotting

- Add logarithmic scales, minor ticks, configurable tick formatting, and axis
  offset notation for large values with small differences.
- Add error bars, confidence bands, and heatmaps.
- Add configurable typography, axis styling, and legend placement.
- Add PDF and PNG export with explicit physical dimensions and resolution.

## Interaction and scale

- Add pan, zoom, cursor coordinates, and selection through sun_ui messages.
- Reuse rendering storage and reduce dense datasets without hiding extrema.
- Add visual regression fixtures across fonts, export formats, and native views.

## Independent distribution

- Publish a documented sun_ui dependency version and a standalone build/test
  entry point before moving sun_plot into its own repository.
