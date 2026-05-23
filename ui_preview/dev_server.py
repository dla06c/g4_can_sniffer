from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import json
import math
import time

lighting = {
    "enabled": True,
    "mode": "pattern",
    "pattern": "engine_plasma",
    "max_brightness": 1.0,
    "r": 0,
    "g": 80,
    "b": 255,
    "w": 0,
}

start = time.time()

def clamp(v, lo, hi):
    return max(lo, min(hi, v))

def mock_data():
    t = time.time() - start

    rpm = 1800 + 2200 * ((math.sin(t * 0.9) + 1) / 2)
    map_kpa = 45 + 35 * ((math.sin(t * 0.55) + 1) / 2)
    mgp = map_kpa - 100
    battery = 13.8 + 0.15 * math.sin(t * 0.2)

    return {
        "rpm": rpm,
        "ect": 86 + 2 * math.sin(t * 0.1),
        "iat": 32 + 3 * math.sin(t * 0.15),
        "mgp": mgp,
        "map": map_kpa,
        "tps": 25 + 20 * ((math.sin(t * 0.75) + 1) / 2),

        "ignition_angle": 22.5,
        "injection_actual_pw": 4.1,
        "injection_effective_pw": 3.9,

        "lambda1": 0.98 + 0.03 * math.sin(t * 0.7),
        "lambda_target": 0.98,
        "lambda_error": 0.02 * math.sin(t * 0.7),
        "lambda_status": 1,
        "lambda_temp": 740,

        "oil_temp": 78,
        "battery_v": battery,
        "fuel_pressure": 370,
        "oil_pressure": 420 + 120 * ((math.sin(t * 0.9) + 1) / 2),

        "boost_target": 150,
        "boost_error": 5 * math.sin(t),
        "boost_p": 1.2,
        "boost_i": 0.8,
        "boost_d": 0.1,
        "boost_duty": 42,

        "trig1_err": 0,
        "internal_3v3": 3.31,
        "internal_12v": 12.1,

        "aps_main": 22,
        "throttle_target": 24,
        "vvt_in_target": 15,
        "vvt_in_pos": 14.5,

        "age_ms": 25,
        "can_frames": int(t * 50),
        "can_decoded_frames": int(t * 50),
        "can_last_frame_age_ms": 10,
        "can_last_decoded_age_ms": 10,
    }

def preview_rgbw():
    d = mock_data()

    if not lighting["enabled"]:
        r = g = b = w = 0
    elif lighting["mode"] == "static":
        scale = lighting["max_brightness"]
        r = int(lighting["r"] * scale)
        g = int(lighting["g"] * scale)
        b = int(lighting["b"] * scale)
        w = int(lighting["w"] * scale)
    else:
        # Simple approximation of your engine plasma mode for UI preview.
        load = clamp((d["map"] - 10) / (70 - 10), 0, 1)
        brightness = clamp((d["rpm"] - 1000) / (4800 - 1000), 0, 1)
        brightness = 0.5 + brightness * 0.5
        brightness *= lighting["max_brightness"]

        r = int((20 + 235 * load) * brightness)
        g = int((0 + 180 * max(0, load - 0.55)) * brightness)
        b = int((120 * (1 - load)) * brightness)
        w = int((40 * max(0, load - 0.75) / 0.25) * brightness)

    return {
        **lighting,
        "r": r,
        "g": g,
        "b": b,
        "w": w,
        "preview_r": min(255, r + w),
        "preview_g": min(255, g + w),
        "preview_b": min(255, b + w),
        "rpm": d["rpm"],
        "mgp": d["mgp"],
    }

class Handler(SimpleHTTPRequestHandler):
    def send_json(self, data):
        body = json.dumps(data).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        global lighting

        parsed = urlparse(self.path)

        if parsed.path == "/data":
            return self.send_json(mock_data())

        if parsed.path == "/lightingState":
            return self.send_json(preview_rgbw())

        if parsed.path == "/setLighting":
            qs = parse_qs(parsed.query)

            if "enabled" in qs:
                lighting["enabled"] = qs["enabled"][0] == "1"

            if "mode" in qs:
                lighting["mode"] = qs["mode"][0]

            if "pattern" in qs:
                lighting["pattern"] = qs["pattern"][0]

            for key in ["r", "g", "b", "w"]:
                if key in qs:
                    lighting[key] = int(qs[key][0])

            if "brightness" in qs:
                lighting["max_brightness"] = float(qs["brightness"][0])

            return self.send_json({"ok": True})

        if parsed.path == "/canStatus":
            return self.send_json({
                "started": True,
                "frames": int((time.time() - start) * 50),
                "decoded_frames": int((time.time() - start) * 50),
                "last_frame_age_ms": 10,
                "last_decoded_age_ms": 10,
                "can_state": "RUNNING",
                "tx_err": 0,
                "rx_err": 0,
                "rx_missed": 0,
                "bus_error": 0,
            })

        return super().do_GET()

if __name__ == "__main__":
    server = ThreadingHTTPServer(("0.0.0.0", 8080), Handler)
    print("LinkDash dev server running at http://localhost:8080", flush=True)
    server.serve_forever()