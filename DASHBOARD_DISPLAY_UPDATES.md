# Dashboard display updates

- Restored the supplied Linebeam typeface as an embedded WOFF2 subset. It is
  contained inside the dashboard HTML, so no LittleFS/SPIFFS font upload is
  required.
- Converted the Stream Status body into a fixed decorative terminal panel.
  Only the CONNECTED/DISCONNECTED indicator remains live.
- Scene motion now starts whenever RPM is greater than 500, regardless of gear.
- Scene motion steps every 75 ms and illuminates three spaced road groups and
  two spaced tyre-tread groups to make the motion denser.
