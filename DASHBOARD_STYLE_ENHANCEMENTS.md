# Dashboard style enhancements

## 80-bar RPM meter

The original 15 polygon RPM segments have been replaced by 80 narrow vertical
SVG bars. The original uploaded RPM silhouette is retained as an SVG clip path,
so the sloped left edge, top profile, lower edge, and overall proportions remain
unchanged.

The 0–8000 RPM range gives each physical bar a 100 RPM span. The leading bar is
rendered in ten opacity steps, giving visible 10 RPM response without adding 800
DOM elements. Only bars whose state changes receive DOM updates.

The original green, yellow, orange, and red colour zones are retained according
to their positions in the uploaded silhouette.

## Cyberpunk metric panels

The five metric panels now include:

- clipped/notched panel corners;
- per-channel accent colours;
- inset grid and scan-line details;
- coded sensor headers and live-feed identifiers;
- segmented progress tracks;
- highlighted value/unit hierarchy; and
- static decorative instrumentation details.

The treatment uses static gradients, borders, and pseudo-elements rather than
blur filters or continuously animated effects. Progress remains transform-based,
so the styling does not undo the previous performance optimization work.
