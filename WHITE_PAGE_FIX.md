# ESP32 blank-page fix

The merged dashboard is approximately 116 KB. The previous implementation returned the complete page as an Arduino `String`, which required a large contiguous heap allocation before the response could be sent. On the ESP32 this could fail and result in an empty HTTP response (a plain white browser page).

The dashboard is now declared as a `PROGMEM` byte string and served with `WebServer::send_P()`, so it remains in flash and is transmitted in small chunks.

## Diagnostics

After flashing, open the serial monitor at 115200 baud and load the dashboard. A request to `/` should print a line similar to:

```
Serving dashboard from flash, bytes: 115939 | free heap: 123456
```

A lightweight test endpoint is also available:

```
http://<esp32-address>/health
```

It should return:

```
OK
html_bytes=115939
free_heap=<current free heap>
```

Use `http://`, not `https://`.
