# Responsive RPM and red metric-panel update

## Why the previous styled version was slower

The 80-bar RPM revision used 80 independently styled SVG rectangles. Although JavaScript only changed bars near the current RPM boundary, the browser still had to maintain, clip, style, and repaint a larger SVG tree.

The metric panels also placed multiple gradients, repeating patterns, clipped backgrounds, inset shadows, and pseudo-elements on the same HTML elements whose text and progress values changed. On lower-power Android dashboard browsers, a child update can cause the entire clipped panel surface to be repainted.

## Changes in this build

- The RPM layer still displays 80 separated vertical bars.
- The 80 divisions are now produced by one static SVG pattern.
- RPM progress is one gradient-filled rectangle clipped to the original silhouette.
- A 10 RPM change updates one `scaleX()` transform.
- RPM colour zones and the original silhouette are preserved.
- Telemetry messages are coalesced through `requestAnimationFrame()` so only the newest pending frame is rendered.
- The stream log uses a fixed nine-line ring buffer and no longer reads `scrollHeight`, avoiding forced synchronous layout.
- Metric-panel chrome is baked into the separately cached dashboard background SVG.
- Live metric HTML is transparent and only changes text plus one transform per progress bar.
- Every panel now uses the dashboard red theme while readout values and units remain yellow.
- Metric panel positions are aligned to the original vector frame coordinates.
- The voltage panel has a large lower-right cut matching the road/scene boundary.
- The air-temperature panel has the mirrored lower-left cut.

## Validation

- Embedded JavaScript syntax check passed.
- Dashboard and asset C++ raw-string syntax checks passed.
- Browser rendering produced no runtime errors.
- Linebeam loaded successfully.
- The lower metric-panel clip paths matched the intended scene cuts.
- A synthetic forced-layout update test was approximately 40% faster than the prior cyberpunk build in the test browser. This is a relative browser test rather than a guarantee of the exact improvement on every dashboard device.
