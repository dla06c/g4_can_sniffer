# LinkDash performance optimization

This build restructures the dashboard so the browser redraws only the small parts that actually change, while the ESP32 spends less time servicing HTTP, CAN frames, and unchanged LED output.

## Dashboard rendering

- The static dashboard drawing is restored as a separate cached `/dashboard-bg.svg` image.
- Linebeam is served as a separate cached `/linebeam.woff2` font.
- Static artwork is no longer part of the inline dynamic SVG tree.
- Animated road and tyre paths are grouped into complete frames, so animation changes a few parent groups instead of many individual paths.
- Expensive blur/drop-shadow filters and transitions are removed from frequently changing SVG elements.
- Progress bars use `transform: scaleX()` instead of changing element width.
- RPM segments update only when the active segment count changes.
- Only the currently visible dashboard tab receives DOM updates.
- Scene motion and telemetry rendering pause while the browser page is hidden.
- The scrolling stream panel uses a fixed ring buffer and one `<pre>` text node instead of continually creating and deleting nested DOM elements.

## Telemetry transport

- Live telemetry is pushed at 20 Hz over a persistent WebSocket on port 81.
- `/data` remains available as a compatibility and diagnostic fallback.
- If WebSocket connection is unavailable, the page falls back to `/data` at 10 Hz.
- Telemetry JSON is created in a fixed character buffer rather than through repeated Arduino `String` concatenation.

## CAN processing

- The TWAI hardware filter accepts the configured Link ECU standard CAN ID `0x3E8` rather than queueing every vehicle CAN frame.
- A maximum of 12 queued CAN frames is processed per main-loop pass, preventing CAN traffic from starving web and lighting services.
- Serial CAN logging is rate-limited to once every two seconds.

## Addressable-lighting processing

- Static and off output is sent only when the output or configuration changes.
- Engine-plasma output is skipped when the calculated RGBW value has not changed.
- Animated patterns are capped at 20 frames per second.
- Settings changes explicitly mark the LED output dirty so zones and colours still update immediately.

The two strips still use the existing Adafruit NeoPixel driver and are transmitted sequentially. This avoids changing the tested RGBW driver and pin behaviour. The major blocking reduction comes from no longer retransmitting unchanged strips and from limiting animated output to 20 FPS.

## Browser endpoints

- Dashboard: `http://192.168.4.1/`
- Health information: `http://192.168.4.1/health`
- Telemetry fallback: `http://192.168.4.1/data`
- WebSocket telemetry: `ws://192.168.4.1:81/`

## First build

`platformio.ini` includes:

```ini
links2004/WebSockets @ 2.7.2
```

PlatformIO will download this dependency during the first build. After flashing, perform a hard refresh so the browser discards the previous embedded page while retaining the versioned background and font assets.
