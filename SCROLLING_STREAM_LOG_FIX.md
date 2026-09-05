# Scrolling Stream Status correction

The Stream Status panel is a live scrolling telemetry log again.

- A new line is appended approximately every 280 ms.
- Lines rotate through live CAN, pressure, temperature, lambda, power, fuel, boost-control, and decoder values.
- New entries appear at the bottom and the panel automatically scrolls to them.
- The buffer retains the latest 36 entries and removes older entries.
- Connection and disconnection transitions are written into the log.
- When ECU data is unavailable, the panel continues printing a waiting-for-stream status.

The existing Linebeam font and the scene-motion behaviour (active when RPM is greater than 500) remain unchanged.
