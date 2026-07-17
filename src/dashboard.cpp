#include "dashboard.h"

String dashboardHtml() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Link ECU Dashboard</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">

  <style>
    :root {
      --bg: #0f1115;
      --panel: #1c2028;
      --panel2: #252b36;
      --text: #f2f4f8;
      --muted: #9da7b4;
      --warn: #6b3e00;
      --danger: #6b1515;
      --ok: #163f2a;
      --accent: #4da3ff;
      --cardash-voltage-label-height: 30px;
      --gauge-glow-blue: rgba(77, 163, 255, 0.5);
      --gauge-glow-cyan: rgba(40, 215, 255, 0.55);
      --gauge-glow-orange: rgba(255, 157, 77, 0.5);
    }

    body {
      margin: 0;
      font-family: Arial, Helvetica, sans-serif;
      background: var(--bg);
      color: var(--text);
    }

    header {
      padding: 12px 16px;
      background: #171a21;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      border-bottom: 1px solid #303642;
    }

    .title {
      font-size: 20px;
      font-weight: bold;
    }

    .status {
      font-size: 13px;
      color: var(--muted);
      text-align: right;
    }

    nav {
      display: grid;
      grid-template-columns: repeat(3, minmax(84px, 1fr));
      gap: 8px;
      padding: 10px;
      background: #14171d;
      position: sticky;
      top: 0;
      z-index: 5;
    }

    button, select, input[type="color"], input[type="range"], input[type="number"] {
      width: 100%;
      box-sizing: border-box;
    }
    
    input[type="number"] {
      border: 1px solid #303642;
      border-radius: 10px;
      padding: 12px;
      background: #111722;
      color: var(--text);
      font-size: 16px;
    }

    button {
      border: 0;
      border-radius: 10px;
      padding: 12px 8px;
      background: var(--panel);
      color: var(--text);
      font-size: 15px;
      font-weight: bold;
    }

    button.active {
      background: var(--accent);
      color: #07111d;
    }

    .dashboard-tab {
      border: 1px solid currentColor;
      background: #000;
      color: #808080;
      display: grid;
      place-items: center;
      padding: 10px 8px;
    }

    .dashboard-tab.active {
      background: #000;
      color: #ff220d;
      box-shadow:
        0 0 2px rgba(255, 34, 13, 0.58),
        0 0 7px rgba(255, 34, 13, 0.22);
    }

    .dashboard-tab .tab-icon {
      width: 24px;
      height: 24px;
      display: block;
    }

    .dashboard-tab .tab-icon-fill {
      fill: currentColor;
      stroke: none;
    }

    .dashboard-tab .tab-icon-stroke {
      fill: none;
      stroke: currentColor;
      stroke-width: 0.48;
      stroke-linecap: round;
      stroke-linejoin: round;
    }

    .sr-only {
      position: absolute;
      width: 1px;
      height: 1px;
      padding: 0;
      margin: -1px;
      overflow: hidden;
      clip: rect(0, 0, 0, 0);
      white-space: nowrap;
      border: 0;
    }

    select {
      border: 1px solid #303642;
      border-radius: 10px;
      padding: 12px;
      background: #111722;
      color: var(--text);
      font-size: 16px;
    }

    input[type="color"] {
      height: 52px;
      border: 0;
      border-radius: 10px;
      background: var(--panel2);
      padding: 4px;
    }

    input[type="range"] {
      margin-top: 12px;
    }

    .page {
      display: none;
      padding: 12px;
    }

    .page.active {
      display: block;
    }

    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(145px, 1fr));
      gap: 12px;
    }

    .card {
      background: var(--panel);
      border-radius: 14px;
      padding: 14px;
      text-align: center;
      border: 1px solid #303642;
      min-height: 92px;
    }

    .wide {
      grid-column: span 2;
    }

    .label {
      font-size: 13px;
      color: var(--muted);
      margin-bottom: 8px;
      white-space: nowrap;
    }

    .value {
      font-size: 34px;
      font-weight: bold;
      line-height: 1.05;
    }

    .smallvalue {
      font-size: 24px;
      font-weight: bold;
      line-height: 1.1;
    }

    .unit {
      font-size: 12px;
      color: var(--muted);
      margin-top: 5px;
    }

    .ok {
      background: var(--ok);
    }

    .warn {
      background: var(--warn);
    }

    .danger {
      background: var(--danger);
    }

    .summary {
      background: var(--panel2);
      border-radius: 14px;
      padding: 14px;
      margin-bottom: 12px;
      border: 1px solid #303642;
      font-size: 15px;
      color: var(--muted);
    }

    .alert {
      background: var(--danger);
      color: white;
      padding: 14px;
      margin: 12px;
      border-radius: 14px;
      text-align: center;
      font-weight: bold;
      font-size: 20px;
      display: none;
    }

    .lighting-row {
      display: flex;
      gap: 10px;
    }

    .cardash-wrap {
      display: grid;
      grid-template-columns: 1.3fr 1fr;
      gap: 12px;
    }

    .cardash-hero {
      background: radial-gradient(circle at top, #2b3545, #111722);
      border: 1px solid #303642;
      border-radius: 18px;
      padding: 18px;
      text-align: center;
    }

    .cardash-label {
      font-size: 13px;
      color: var(--muted);
      margin-bottom: 6px;
    }

    .cardash-rpm {
      font-size: 58px;
      font-weight: bold;
      line-height: 1;
    }

    .cardash-speed {
      font-size: 46px;
      font-weight: bold;
      line-height: 1;
    }

    .cardash-sub {
      color: var(--muted);
      font-size: 13px;
      margin-top: 6px;
    }

    .cardash-bar-bg {
      height: 18px;
      border-radius: 999px;
      background: #0b0d11;
      overflow: hidden;
      border: 1px solid #303642;
      margin-top: 12px;
    }

    .cardash-bar-fill {
      height: 100%;
      width: 0%;
      background: linear-gradient(90deg, #4da3ff, #a855f7, #f97316);
      border-radius: 999px;
    }

    .cardash-mini-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(130px, 1fr));
      gap: 12px;
      margin-top: 12px;
    }

    .cardash-mini {
      background: var(--panel);
      border: 1px solid #303642;
      border-radius: 14px;
      padding: 12px;
      text-align: center;
    }

    .cardash-mini .big {
      font-size: 28px;
      font-weight: bold;
    }

    .cardash-mini.warn {
      background: var(--warn);
    }

    .cardash-mini.danger {
      background: var(--danger);
    }

    @media (max-width: 720px) {
      .cardash-wrap {
        grid-template-columns: 1fr;
      }

      .cardash-rpm {
        font-size: 44px;
      }

      .cardash-speed {
        font-size: 38px;
      }
    }

    

    .cluster-wrap {
      padding: 10px;
    }

    .cluster-main {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 16px;
      align-items: stretch;
    }

    .cluster-footer {
      display: grid;
      grid-template-columns: minmax(0, 1.35fr) minmax(260px, 0.85fr);
      gap: 16px;
      margin-top: 16px;
    }

    .cluster-temps {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 16px;
    }

    .cluster-volts {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 12px;
    }

    .gauge-card {
      position: relative;
      overflow: hidden;
      background:
        radial-gradient(circle at 50% 16%, rgba(139, 205, 255, 0.22), transparent 34%),
        linear-gradient(180deg, #364251 0%, #161d28 10%, #0b0f15 100%);
      border: 1px solid #465364;
      border-radius: 26px;
      padding: 18px 16px 14px;
      text-align: center;
      box-shadow:
        inset 0 1px 0 rgba(255, 255, 255, 0.08),
        inset 0 -18px 30px rgba(0, 0, 0, 0.35),
        0 18px 28px rgba(0, 0, 0, 0.22);
    }

    .gauge-card::after {
      content: '';
      position: absolute;
      inset: 0;
      background: linear-gradient(180deg, rgba(255,255,255,0.08), transparent 26%, rgba(0,0,0,0.16));
      pointer-events: none;
    }

    .gauge-card::before {
      content: '';
      position: absolute;
      inset: 10px;
      border-radius: 22px;
      border: 1px solid rgba(124, 144, 165, 0.14);
      box-shadow: inset 0 0 35px rgba(0, 0, 0, 0.38);
      pointer-events: none;
    }

    .gauge-card.secondary {
      padding-top: 14px;
    }

    .gauge-title {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      min-width: 108px;
      padding: 6px 12px;
      border-radius: 999px;
      border: 1px solid rgba(111, 129, 149, 0.45);
      background: linear-gradient(180deg, rgba(29, 39, 52, 0.95), rgba(10, 14, 20, 0.95));
      box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.06);
      color: #ced6df;
      font-size: 11px;
      font-weight: bold;
      letter-spacing: 0.18em;
      margin-bottom: 8px;
      position: relative;
      z-index: 1;
    }

    .gauge-svg {
      width: 100%;
      max-width: 360px;
      height: auto;
      margin-top: 2px;
      position: relative;
      z-index: 1;
    }

    .gauge-card.secondary .gauge-svg {
      max-width: 250px;
    }

    .gauge-value {
      font-size: 40px;
      font-weight: bold;
      line-height: 1;
      position: relative;
      z-index: 1;
      letter-spacing: 0.08em;
      text-shadow: 0 0 14px rgba(255, 255, 255, 0.12);
    }

    .gauge-card.secondary .gauge-value {
      font-size: 30px;
    }

    .gauge-unit {
      color: #97a6b7;
      font-size: 11px;
      margin-top: 4px;
      position: relative;
      z-index: 1;
      text-transform: uppercase;
      letter-spacing: 0.2em;
    }

    .gauge-readout {
      width: min(150px, 72%);
      margin: -14px auto 0;
      padding: 10px 12px 11px;
      border-radius: 16px;
      border: 1px solid #45556a;
      background:
        linear-gradient(180deg, rgba(33, 46, 61, 0.98), rgba(8, 12, 18, 0.98)),
        linear-gradient(90deg, rgba(77, 163, 255, 0.18), transparent 55%);
      box-shadow:
        inset 0 1px 0 rgba(255, 255, 255, 0.06),
        inset 0 0 26px rgba(0, 0, 0, 0.4),
        0 10px 18px rgba(0, 0, 0, 0.22);
      position: relative;
      z-index: 1;
    }

    .gauge-card.secondary .gauge-readout {
      width: min(128px, 76%);
      margin-top: -10px;
      padding-top: 9px;
      padding-bottom: 9px;
    }

    .gauge-caption {
      color: #677789;
      font-size: 10px;
      letter-spacing: 0.22em;
      text-transform: uppercase;
      margin-top: 9px;
      position: relative;
      z-index: 1;
    }

    .gauge-bezel-outer {
      fill: #45505d;
      stroke: #718095;
      stroke-width: 2;
    }

    .gauge-bezel-inner {
      fill: #1c242f;
      stroke: #0d1117;
      stroke-width: 6;
    }

    .gauge-face {
      fill: #0c121a;
      stroke: #2e3948;
      stroke-width: 2;
    }

    .gauge-face-ring {
      fill: none;
      stroke: rgba(159, 181, 204, 0.08);
      stroke-width: 1.5;
    }

    .gauge-glare {
      fill: rgba(159, 209, 255, 0.06);
    }

    .gauge-scale {
      fill: #93a1b1;
      font-size: 10px;
      font-weight: bold;
      letter-spacing: 0.05em;
    }

    .gauge-scale-alert {
      fill: #ffb38c;
    }

    .gauge-center-label {
      fill: #6c7b8b;
      font-size: 9px;
      font-weight: bold;
      letter-spacing: 0.18em;
    }

    .gauge-status-line {
      stroke: rgba(115, 134, 156, 0.32);
      stroke-width: 1;
    }

    .gauge-tick {
      stroke: #8f9eb0;
      stroke-width: 2.2;
    }

    .gauge-tick.minor {
      stroke-width: 1.3;
      opacity: 0.55;
    }

    .gauge-arc-bg {
      fill: none;
      stroke: #28323e;
      stroke-width: 10;
      stroke-linecap: round;
    }

    .gauge-arc-active {
      fill: none;
      stroke: #4da3ff;
      stroke-width: 10;
      stroke-linecap: round;
      filter: drop-shadow(0 0 7px var(--gauge-glow-blue));
    }

    .gauge-zone {
      fill: none;
      stroke-linecap: round;
      opacity: 0.92;
    }

    .gauge-zone-warn {
      stroke: #ffb14d;
      stroke-width: 8;
    }

    .gauge-zone-danger {
      stroke: #ff5a5a;
      stroke-width: 8;
    }

    .boost-arc {
      stroke: #28d7ff;
    }

    .temp-arc {
      stroke: #ff9d4d;
    }

    .needle-pack {
      transform-origin: 100px 100px;
      transition: transform 80ms linear;
    }

    .gauge-needle-shadow {
      stroke: rgba(0, 0, 0, 0.42);
      stroke-width: 7;
      stroke-linecap: round;
    }

    .gauge-needle {
      stroke: #f2f4f8;
      stroke-width: 4.4;
      stroke-linecap: round;
    }

    .gauge-needle-tail {
      stroke: rgba(242, 244, 248, 0.4);
      stroke-width: 3;
      stroke-linecap: round;
    }

    .boost-needle {
      stroke: #8ef2ff;
    }

    .temp-needle {
      stroke: #ffd7a8;
      stroke-width: 3.5;
    }

    .gauge-hub {
      fill: #f2f4f8;
    }

    .gauge-hub-ring {
      fill: none;
      stroke: rgba(242, 244, 248, 0.4);
      stroke-width: 2.5;
    }

    .gauge-redline {
      fill: none;
      stroke: #ff4d4d;
      stroke-width: 7;
      stroke-linecap: round;
    }

    .voltage-card {
      background: linear-gradient(180deg, #1b2230, #10151d);
      border: 1px solid #303642;
      border-radius: 18px;
      padding: 14px 12px;
      text-align: center;
    }

    .voltage-card.warn {
      background: linear-gradient(180deg, #5f4311, #31210a);
    }

    .voltage-card.danger {
      background: linear-gradient(180deg, #5d1b1b, #2d0d0d);
    }

    .voltage-label {
      color: var(--muted);
      font-size: 11px;
      font-weight: bold;
      letter-spacing: 0.08em;
      margin-top: 8px;
      min-height: var(--cardash-voltage-label-height);
    }

    .voltage-value {
      font-size: 28px;
      font-weight: bold;
      line-height: 1.1;
      margin-top: 6px;
    }

    .voltage-icon {
      width: 56px;
      height: 56px;
      margin: 0 auto;
      border-radius: 16px;
      display: flex;
      align-items: center;
      justify-content: center;
      background: rgba(7, 11, 17, 0.9);
      border: 1px solid #303642;
      color: #8bd5ff;
      box-shadow: inset 0 0 18px rgba(0, 0, 0, 0.45);
    }

    .voltage-icon svg {
      width: 30px;
      height: 30px;
      fill: none;
      stroke: currentColor;
      stroke-width: 1.8;
      stroke-linecap: round;
      stroke-linejoin: round;
    }

    @media (max-width: 960px) {
      .cluster-footer {
        grid-template-columns: 1fr;
      }
    }

    @media (max-width: 720px) {
      .cluster-main,
      .cluster-temps {
        grid-template-columns: 1fr;
      }

      .cluster-volts {
        grid-template-columns: repeat(3, minmax(0, 1fr));
      }

      .gauge-value {
        font-size: 34px;
      }
    }

    @media (max-width: 520px) {
      .cluster-volts {
        grid-template-columns: 1fr;
      }
    }


    .lighting-row button {
      flex: 1;
    }

    .zone-grid {
      display: flex;
      flex-direction: column;
      gap: 8px;
      margin-top: 4px;
    }

    .zone-row {
      display: flex;
      align-items: center;
      gap: 8px;
      font-size: 13px;
      color: var(--text);
      cursor: pointer;
    }

    .zone-row input[type="checkbox"] {
      width: 18px;
      height: 18px;
      flex-shrink: 0;
      cursor: pointer;
    }

    .zone-row span {
      flex: 0 0 110px;
      color: var(--muted);
    }

    .zone-row input[type="text"] {
      flex: 1;
      border: 1px solid #303642;
      border-radius: 10px;
      padding: 8px 10px;
      background: #111722;
      color: var(--text);
      font-size: 14px;
      width: auto;
    }
    @media (max-width: 480px) {
      .value {
        font-size: 28px;
      }

      .wide {
        grid-column: span 1;
      }

      header {
        display: block;
      }

      .status {
        text-align: left;
        margin-top: 4px;
      }
        
      .fullscreen_btn {
         width: auto;
         padding: 8px 12px;
         font-size: 13px;
      }       
    }

    /* ==================================================================
       CarDash: Example-style SVG gauge cluster
       ================================================================== */
    #page_cardash {
      --dash-bg: #000;
      --ring: #9a9a9a;
      --dash-text: #b9b9b9;
      --blue: #00aeef;
      --cyan: #8fefff;
      --teal: #008e95;
      --red: #e20b0b;
      --yellow: #e7d000;
      --dial-rgb: 0, 174, 239;
      background: #000;
      padding: 0;
      min-height: calc(100vh - 120px);
    }

    #page_cardash .dash {
      min-height: calc(100vh - 190px);
      display: flex;
      align-items: center;
      justify-content: center;
      gap: clamp(22px, 4vw, 52px);
      padding: 24px 24px 8px;
      box-sizing: border-box;
    }

    #page_cardash svg.gauge {
      width: min(47vw, 520px);
      height: auto;
      overflow: visible;
      /* removed expensive SVG filter causing massive layout lag */
    }

    #page_cardash .outer-ring {
      fill: none;
      stroke: var(--ring);
      stroke-width: 4;
      opacity: .95;
    }

    #page_cardash .inner-dial {
      fill: url(#dialGradientBoost);
    }

    #page_cardash .black-mask {
      fill: #020202;
    }

    #page_cardash .blue-track {
      fill: none;
      stroke: var(--blue);
      stroke-width: 24;
      stroke-linecap: butt;
    }

    #page_cardash .teal-track {
      fill: none;
      stroke: var(--teal);
      stroke-width: 24;
      stroke-linecap: butt;
      opacity: .96;
    }

    #page_cardash .red-track {
      fill: none;
      stroke: var(--red);
      stroke-width: 18;
      stroke-linecap: butt;
    }

    #page_cardash .white-track {
      fill: none;
      stroke: #f4f4f4;
      stroke-width: 18;
      stroke-linecap: butt;
    }

    #page_cardash .small-blue-track {
      fill: none;
      stroke: var(--blue);
      stroke-width: 18;
      stroke-linecap: butt;
    }

    #page_cardash .tick-major,
    #page_cardash .tick-minor,
    #page_cardash .tick-thin {
      stroke: #fff;
      stroke-linecap: square;
      opacity: .92;
    }

    #page_cardash .tick-major { stroke-width: 3; }
    #page_cardash .tick-minor { stroke-width: 1.2; opacity: .82; }
    #page_cardash .tick-thin  { stroke-width: .7; opacity: .38; }

    #page_cardash .blue-tick { stroke: #67dcff; opacity: .45; }
    #page_cardash .teal-tick { stroke: #7bf4f4; opacity: .35; }
    #page_cardash .red-tick { stroke: var(--red); }
    #page_cardash .yellow-tick { stroke: var(--yellow); }

    #page_cardash .label {
      fill: var(--dash-text);
      font-weight: 700;
      font-size: 27px;
      text-anchor: middle;
      dominant-baseline: middle;
    }

    #page_cardash .small-label {
      fill: var(--dash-text);
      font-weight: 700;
      font-size: 15px;
      text-anchor: middle;
      dominant-baseline: middle;
    }

    #page_cardash .center-value {
      fill: #f7f7f7;
      font-weight: 800;
      font-size: 30px;
      text-anchor: middle;
      dominant-baseline: middle;
    }

    #page_cardash .unit {
      fill: var(--dash-text);
      color: var(--dash-text);
      font-weight: 800;
      font-size: 27px;
      text-anchor: middle;
      dominant-baseline: middle;
      margin-top: 0;
    }

    #page_cardash .subtitle {
      fill: var(--dash-text);
      font-size: 18px;
      font-weight: 800;
      text-anchor: middle;
      dominant-baseline: middle;
    }

    #page_cardash .inner-ring {
      fill: none;
      stroke: var(--blue);
      stroke-width: 2.5;
    }

    #page_cardash .needle-blue,
    #page_cardash .needle-red {
      stroke-linecap: round;
      /* offloaded layout rotation to GPU CSS transformations */
      transform-origin: 260px 260px;
      will-change: transform;
      transition: transform 30ms linear;
    }

    #page_cardash .needle-blue {
      stroke: var(--cyan);
      stroke-width: 3.2;
    }

    #page_cardash .needle-red {
      stroke: var(--red);
      stroke-width: 3;
    }

    #page_cardash .warning-light {
      fill: url(#redGlow);
      opacity: .25;
      will-change: opacity;
    }

    #page_cardash .warning-light.active {
      opacity: 1;
    }

    #page_cardash .dash-aux {
      width: min(880px, calc(100% - 32px));
      margin: 0 auto 18px;
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 12px;
    }

    #page_cardash .dash-pill {
      border: 1px solid rgba(154, 154, 154, .42);
      border-radius: 999px;
      padding: 10px 14px;
      color: var(--dash-text);
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      background: rgba(255, 255, 255, .035);
      box-shadow: inset 0 0 18px rgba(0,174,239,.07);
      letter-spacing: .08em;
    }

    #page_cardash .dash-pill span {
      font-size: 11px;
      font-weight: 800;
    }

    #page_cardash .dash-pill strong {
      color: #fff;
      font-size: 20px;
      letter-spacing: .02em;
    }

    #page_cardash .dash-pill.warn {
      border-color: var(--yellow);
      background: rgba(231, 208, 0, .08);
    }

    #page_cardash .dash-pill.danger {
      border-color: var(--red);
      background: rgba(226, 11, 11, .12);
    }

    @media (max-width: 760px) {
      #page_cardash .dash {
        flex-direction: column;
        min-height: auto;
      }

      #page_cardash svg.gauge {
        width: min(92vw, 520px);
      }

      #page_cardash .dash-aux {
        grid-template-columns: 1fr;
      }
    }


    #page_cardash .temp-track {
      fill: none;
      stroke: var(--blue);
      stroke-width: 12;
      stroke-linecap: butt;
      opacity: 0.92;
    }

    #page_cardash .temp-track-warn {
      fill: none;
      stroke: var(--yellow);
      stroke-width: 12;
      stroke-linecap: butt;
      opacity: 0.92;
    }

    #page_cardash .temp-track-danger {
      fill: none;
      stroke: var(--red);
      stroke-width: 12;
      stroke-linecap: butt;
      opacity: 0.94;
    }

    #page_cardash .temp-sub-needle {
      stroke-width: 3;
      transform-origin: 260px 260px;
      will-change: transform;
      transition: transform 30ms linear;
    }

    #page_cardash .temp-readout-label {
      font-size: 14px;
      opacity: 0.9;
    }

    #page_cardash .temp-readout-value {
      fill: #f7f7f7;
      font-size: 19px;
      font-weight: 900;
    }

    #page_cardash .temp-readout-unit {
      font-size: 11px;
      opacity: 0.82;
    }

    #page_cardash .temp-scale-label {
      fill: var(--dash-text);
      font-size: 13px;
      font-weight: 800;
      text-anchor: middle;
      dominant-baseline: middle;
      opacity: 0.9;
    }


    #page_cardash .progress-arc {
      fill: none;
      stroke: var(--blue);
      stroke-width: 14;
      stroke-linecap: butt;
      opacity: 0.96;
      will-change: stroke-dashoffset;
      transition: stroke-dashoffset 30ms linear;
    }

    #page_cardash .boost-progress-arc {
      stroke: var(--teal);
    }


    /* Battery charge-level indicator inside CarDash battery pill */
    #page_cardash .battery-pill {
      align-items: center;
    }

    #page_cardash .battery-pill-main {
      min-width: 126px;
      display: grid;
      grid-template-columns: auto 62px;
      align-items: center;
      gap: 10px;
    }

    #page_cardash .battery-level {
      position: relative;
      width: 62px;
      height: 18px;
      border: 1px solid rgba(185, 185, 185, .82);
      border-radius: 4px;
      padding: 2px;
      box-sizing: border-box;
      background: rgba(0, 0, 0, .65);
      box-shadow: inset 0 0 8px rgba(0, 0, 0, .65);
    }

    #page_cardash .battery-level::after {
      content: '';
      position: absolute;
      top: 5px;
      right: -6px;
      width: 5px;
      height: 8px;
      border-radius: 0 2px 2px 0;
      background: rgba(185, 185, 185, .82);
    }

    #page_cardash .battery-level-fill {
      display: block;
      width: 0%;
      height: 100%;
      border-radius: 2px;
      background: var(--blue);
      box-shadow: 0 0 8px rgba(0, 174, 239, .55);
      transition: width 120ms linear, background-color 120ms linear;
    }

    #page_cardash .battery-level-fill.warn {
      background: var(--yellow);
      box-shadow: 0 0 8px rgba(231, 208, 0, .5);
    }

    #page_cardash .battery-level-fill.danger {
      background: var(--red);
      box-shadow: 0 0 8px rgba(226, 11, 11, .65);
    }

    @media (max-width: 760px) {
      #page_cardash .battery-pill-main {
        grid-template-columns: auto 72px;
      }

      #page_cardash .battery-level {
        width: 72px;
      }
    }

  </style>
</head>

<body>
  <header>
    <div class="title">Link ECU Dashboard</div>
    <div style="display:flex;gap:8px;align-items:center;">
      <button id="fullscreen_btn" onclick="goFullscreen()">Fullscreen</button>
      <div class="status" id="status">Waiting for data...</div>
    </div>
  </header>

  <div class="alert" id="main_alert"></div>

  <nav>
    <button id="btn_drive" class="dashboard-tab active" onclick="showPage('drive')" aria-label="Drive Dashboard">
      <svg class="tab-icon tab-icon-drive" viewBox="282.500275 -226.499176 10.999451 10.998367" aria-hidden="true" focusable="false">
        <path class="tab-icon-fill" transform="matrix(1,0,0,-1,0,0)" d="M287.711304,226.499176C286.253387,226.410049,284.906616,225.770874,283.940735,224.709641C283.139069,223.828827,282.656464,222.753387,282.5224,221.548965C282.473572,221.110352,282.503723,220.436874,282.592865,219.975662C282.885895,218.459274,283.797424,217.133957,285.103455,216.325455C285.700653,215.95575,286.412537,215.686966,287.070313,215.582855C287.632629,215.493835,287.813049,215.484268,288.31897,215.516647C288.87323,215.552109,289.353638,215.648956,289.850647,215.825409C290.064453,215.901321,290.105042,215.918518,290.397034,216.056961C291.34552,216.506683,292.176086,217.266602,292.719269,218.181671C293.242767,219.063568,293.500183,219.998505,293.499725,221.016205C293.498535,223.689835,291.567444,225.976151,288.928558,226.428253C288.555878,226.492111,288.064209,226.520752,287.711304,226.499176zM285.1745,220.326294C285.455688,220.266693,285.665802,220.042435,285.720367,219.743683C285.765076,219.499069,285.725922,218.821762,285.643829,218.419022C285.620483,218.304565,285.480774,217.707718,285.414764,217.595108C285.302521,217.403564,285.064819,217.332565,284.858063,217.428818C284.651367,217.525024,284.203613,218.035141,283.915802,218.502258C283.732849,218.799194,283.521698,219.258224,283.464722,219.48288C283.400269,219.73703,283.525421,220.000931,283.765533,220.117172C283.803528,220.135574,284.641418,220.305481,284.839386,220.33873C284.929108,220.353806,285.069244,220.348602,285.1745,220.326294zM291.332367,220.330368C291.437683,220.3116,292.125549,220.240128,292.243195,220.1828C292.485443,220.064743,292.602631,219.782379,292.523163,219.508179C292.429016,219.183212,292.211884,218.733688,291.977661,218.378799C291.734497,218.010361,291.328339,217.551743,291.167877,217.464417C290.93924,217.339966,290.664063,217.432083,290.546722,217.672302C290.522797,217.721298,290.475739,217.847549,290.442139,217.95285C290.408569,218.058151,290.207642,218.927933,290.209808,219.459564C290.210785,219.700073,290.2164,219.770737,290.23822,219.817505C290.253174,219.849548,290.281128,219.918488,290.300323,219.970703C290.349823,220.105286,290.45871,220.219864,290.605286,220.291687C290.807556,220.390762,290.950745,220.398392,291.332367,220.330368zM288.912231,218.492111C289.021667,218.447128,289.122955,218.365479,289.184448,218.272675C289.235901,218.194992,289.308472,217.991714,289.372955,217.744751C289.403137,217.62912,289.567993,216.846161,289.491211,216.694382C289.43399,216.581329,289.295776,216.44873,289.179749,216.395615C288.892242,216.263992,288.036377,216.192032,287.486542,216.253281C287.205414,216.284592,286.89801,216.347183,286.795959,216.393906C286.625458,216.47197,286.496399,216.630737,286.444244,216.826584C286.417664,216.926392,286.671967,218.150116,286.747131,218.26506C286.835205,218.399704,286.975006,218.490921,287.136902,218.519363C287.178131,218.526596,287.575195,218.531326,288.019287,218.529877C288.7995,218.527328,288.829651,218.526047,288.912231,218.492111zM288.532043,225.751541C288.803436,225.720398,289.082031,225.663818,289.355988,225.584198C289.904114,225.424911,290.4646,225.135773,290.949463,224.762177C291.14389,224.612366,291.586761,224.171036,291.73468,223.979675C292.079102,223.534149,292.373199,222.980865,292.535614,222.472961C292.646057,222.127579,292.657379,221.900101,292.571442,221.753433C292.537506,221.695541,292.50705,221.669312,292.435852,221.636719C292.353333,221.598938,292.331238,221.595825,292.218231,221.606079C292.149048,221.612335,291.923279,221.658234,291.716553,221.708054C291.509827,221.757874,290.774078,221.991592,290.7211,222.006805C290.668152,222.022034,290.422516,222.100952,290.175293,222.182205C289.604279,222.369843,289.43512,222.417801,289.126434,222.479584C288.743927,222.556137,288.458984,222.58345,288.04425,222.583359C287.266144,222.583176,286.885742,222.510406,285.796692,222.15332C285.205231,221.959396,284.424561,221.666382,284.19223,221.606995C283.921112,221.537689,283.736664,221.499023,283.677216,221.499023C283.419952,221.499023,283.29541,221.692062,283.338318,222.024338C283.364319,222.225876,283.457611,222.527298,283.593079,222.847565C283.857971,223.473846,284.146942,223.903961,284.622528,224.379822C284.995087,224.75264,285.395813,225.041611,285.864838,225.275711C286.377258,225.531479,286.984833,225.707413,287.536469,225.759766C287.786499,225.783478,288.289948,225.779327,288.532043,225.751541z"></path>
      </svg>
      <span class="sr-only">Drive Dashboard</span>
    </button>
    <button id="btn_gear" class="dashboard-tab" onclick="showPage('gear')" aria-label="Debug Settings">
      <svg class="tab-icon tab-icon-gear" viewBox="305.002014 -226.818893 10.999939 10.988190" aria-hidden="true" focusable="false">
        <path class="tab-icon-fill" transform="matrix(1,0,0,-1,0,0)" d="M308.444336,226.545044C308.069702,226.387024,307.725281,226.24498,307.678925,226.229401C307.570923,226.193085,307.461945,226.063751,307.461945,225.971893C307.461945,225.932602,307.573883,225.629944,307.710693,225.299347C307.847504,224.96875,307.959503,224.685425,307.959564,224.669739C307.959717,224.63768,307.206512,223.873062,307.174774,223.873062C307.163605,223.873062,307.008514,223.933136,306.830109,224.006577C306.079987,224.315399,305.932617,224.370789,305.861298,224.370789C305.742981,224.370789,305.647614,224.254425,305.528564,223.964752C305.46933,223.820663,305.330841,223.485321,305.220795,223.219543C305.110748,222.953751,305.01535,222.700302,305.00885,222.656311C305.001282,222.605438,305.015137,222.547714,305.046875,222.497742C305.08905,222.43129,305.189423,222.38118,305.696472,222.17337C306.026337,222.038193,306.312836,221.912109,306.33313,221.893188C306.384918,221.844955,306.382568,220.816498,306.330566,220.768082C306.311676,220.750473,306.026062,220.625534,305.695923,220.490448C305.183594,220.280823,305.088593,220.233307,305.047638,220.166245C304.990143,220.072037,304.987335,220.005707,305.035828,219.887833C305.388855,219.029724,305.640991,218.429184,305.663116,218.393738C305.698242,218.337463,305.794373,218.293182,305.881348,218.293182C305.918365,218.293182,306.074036,218.34552,306.227234,218.409485C307.025696,218.742889,307.144196,218.790924,307.168152,218.790924C307.20462,218.790924,307.959717,218.03215,307.959625,217.99559C307.959595,217.979156,307.847595,217.696625,307.710754,217.367706C307.573914,217.038803,307.461945,216.735428,307.461945,216.693558C307.461945,216.592102,307.571991,216.480179,307.73703,216.413788C307.809052,216.384796,308.153778,216.241745,308.503052,216.095886C308.852356,215.950043,309.164734,215.83075,309.197266,215.830795C309.229797,215.830856,309.292694,215.853027,309.337067,215.880066C309.422241,215.932007,309.416443,215.919922,309.745972,216.734482C309.833405,216.950607,309.919373,217.14296,309.937012,217.161926C309.983398,217.211807,311.018555,217.211807,311.063507,217.161926C311.094177,217.127869,311.12851,217.049286,311.39325,216.407028C311.467499,216.226929,311.546204,216.040741,311.568176,215.993286C311.609009,215.904968,311.717072,215.830704,311.804688,215.830704C311.831696,215.830704,312.012054,215.89621,312.205505,215.976273C312.398926,216.056351,312.745819,216.198502,312.976349,216.292175C313.432343,216.477463,313.539551,216.554428,313.539551,216.696411C313.539551,216.739853,313.427582,217.047638,313.29071,217.380386C313.153809,217.713135,313.04184,217.993805,313.04184,218.004074C313.04184,218.014359,313.214844,218.196579,313.42627,218.409012C313.727478,218.711639,313.822479,218.792557,313.865082,218.78273C313.894958,218.775848,314.17453,218.662872,314.486359,218.531693C314.798187,218.400513,315.089752,218.293182,315.134308,218.293182C315.2547,218.293182,315.358978,218.406021,315.449066,218.633743C315.491821,218.741806,315.593201,218.989365,315.674316,219.183868C315.966766,219.88504,316.00705,220.000336,315.990479,220.088715C315.981873,220.134598,315.954407,220.190201,315.929474,220.21228C315.904541,220.234344,315.614319,220.364929,315.284515,220.502441C314.954742,220.639969,314.673462,220.763931,314.659485,220.777908C314.620728,220.816666,314.634003,221.861465,314.673706,221.898468C314.691101,221.914658,314.975433,222.038437,315.305603,222.173523C315.817932,222.383148,315.912933,222.430664,315.953857,222.497742C315.980255,222.540955,316.001892,222.601395,316.001953,222.632034C316.001984,222.662674,315.900421,222.933807,315.776245,223.234558C315.652039,223.535309,315.511139,223.877869,315.463135,223.995819C315.415131,224.11377,315.361176,224.233765,315.343262,224.262466C315.303864,224.325531,315.211151,224.370789,315.121338,224.370789C315.084961,224.370789,314.784668,224.258347,314.45401,224.120926C314.053467,223.954422,313.839935,223.878281,313.814087,223.892746C313.75531,223.925644,313.04184,224.631287,313.04184,224.65654C313.04184,224.668655,313.082001,224.777298,313.131104,224.89798C313.180237,225.018661,313.292206,225.293457,313.379974,225.508636C313.545563,225.914581,313.559326,225.975311,313.510284,226.082977C313.472839,226.165176,313.456421,226.173157,312.583374,226.533615C311.807404,226.853989,311.755157,226.866425,311.630371,226.760391C311.601227,226.735641,311.469177,226.45015,311.336945,226.125961C311.204681,225.801788,311.081543,225.520508,311.063324,225.500916C311.037201,225.472855,310.916901,225.466156,310.497009,225.469406C310.203766,225.47168,309.954498,225.482895,309.943054,225.494354C309.93158,225.505798,309.810242,225.785217,309.67337,226.11528C309.536469,226.445343,309.403137,226.736908,309.377045,226.763214C309.267212,226.873917,309.170197,226.851227,308.444336,226.545044zM311.111053,223.495697C311.754486,223.30455,312.228363,222.908569,312.527405,222.312134C312.692871,221.982147,312.739532,221.765259,312.738251,221.331985C312.737274,220.995926,312.729004,220.925568,312.666565,220.721695C312.425903,219.93576,311.89444,219.404526,311.111053,219.166809C310.910522,219.105972,310.833008,219.096649,310.513855,219.095078C310.080688,219.092926,309.871521,219.140793,309.492188,219.328918C309.055542,219.545456,308.717255,219.883957,308.49826,220.323425C308.314392,220.692413,308.263336,220.911545,308.263336,221.331985C308.263336,221.751953,308.314209,221.970718,308.497833,222.340561C308.809845,222.968933,309.350128,223.379425,310.084564,223.546097C310.344391,223.605072,310.821777,223.581635,311.111053,223.495697z"></path>
      </svg>
      <span class="sr-only">Debug Settings</span>
    </button>
    <button id="btn_light" class="dashboard-tab" onclick="showPage('light')" aria-label="Lighting Controls">
      <svg class="tab-icon tab-icon-light" viewBox="327.422363 -226.618851 9.154205 11.000015" aria-hidden="true" focusable="false">
        <path class="tab-icon-stroke" transform="matrix(1,0,0,-1,0,0)" d="M328.040802,219.740845L329.653137,220.713913"></path>
        <path class="tab-icon-stroke" transform="matrix(1,0,0,-1,0,0)" d="M335.936218,219.708725L334.336975,220.706116"></path>
        <path class="tab-icon-stroke" transform="matrix(1,0,0,-1,0,0)" d="M336.576569,222.037354L334.684479,221.995667"></path>
        <path class="tab-icon-stroke" transform="matrix(1,0,0,-1,0,0)" d="M335.909485,224.335693L334.321777,223.354614"></path>
        <path class="tab-icon-stroke" transform="matrix(1,0,0,-1,0,0)" d="M334.240601,226.012909L333.317963,224.340012"></path>
        <path class="tab-icon-stroke" transform="matrix(1,0,0,-1,0,0)" d="M331.997803,226.618851L331.997803,224.723236"></path>
        <path class="tab-icon-stroke" transform="matrix(1,0,0,-1,0,0)" d="M329.666382,225.950058L330.682434,224.345184"></path>
        <path class="tab-icon-stroke" transform="matrix(1,0,0,-1,0,0)" d="M328.049652,224.307022L329.680176,223.366608"></path>
        <path class="tab-icon-stroke" transform="matrix(1,0,0,-1,0,0)" d="M329.285614,222.000244L327.422363,222.026306"></path>
        <path class="tab-icon-fill" transform="matrix(1,0,0,-1,0,0)" d="M333.106018,216.62117C333.106018,215.683426,333.137787,215.560837,333.361084,215.63681C333.443817,215.664963,333.455963,215.787598,333.455963,216.594513L333.455963,217.519928L333.281006,217.519928L333.106018,217.519928L333.106018,216.62117z"></path>
        <path class="tab-icon-fill" transform="matrix(1,0,0,-1,0,0)" d="M330.539642,216.62117C330.539642,215.683426,330.571411,215.560837,330.794708,215.63681C330.877441,215.664963,330.889587,215.787598,330.889587,216.594513L330.889587,217.519928L330.71463,217.519928L330.539642,217.519928L330.539642,216.62117z"></path>
        <path class="tab-icon-fill" transform="matrix(1,0,0,-1,0,0)" d="M329.606415,218.161514L329.606415,217.869888L331.997803,217.869888L334.389191,217.869888L334.389191,218.161514L334.389191,218.453156L331.997803,218.453156L329.606415,218.453156L329.606415,218.161514z"></path>
        <path class="tab-icon-fill" transform="matrix(1,0,0,-1,0,0)" d="M331.554932,223.853806C331.034973,223.727585,330.564758,223.34407,330.296753,222.827637C330.163574,222.57106,330.160126,222.523605,330.143616,220.713303L330.126709,218.861435L330.653961,218.861435L331.181244,218.861435L331.181244,219.304657C331.181244,219.578903,331.214691,219.817932,331.268982,219.931656C331.547668,220.51532,332.446167,220.523773,332.720673,219.945328C332.781189,219.817825,332.814362,219.590851,332.814362,219.304657L332.814362,218.861435L333.341644,218.861435L333.868896,218.861435L333.85199,220.713303C333.835632,222.504227,333.830719,222.57402,333.702454,222.834366C333.541962,223.16011,333.16925,223.55159,332.880615,223.697556C332.665833,223.806198,332.132935,223.939972,331.951202,223.930878C331.896637,223.928146,331.718323,223.893463,331.554932,223.853806z"></path>
      </svg>
      <span class="sr-only">Lighting Controls</span>
    </button>
  </nav>

  
  <section id="page_cardash" class="page active">
    <main class="dash">
      <svg id="boostGauge" class="gauge" viewBox="0 0 520 520" role="img" aria-label="Boost gauge">
        <defs>
          <radialGradient id="dialGradientBoost" cx="50%" cy="50%" r="55%">
            <stop offset="0%" stop-color="#000" />
            <stop offset="54%" stop-color="rgb(var(--dial-rgb))" stop-opacity="0.22" />
            <stop offset="100%" stop-color="rgb(var(--dial-rgb))" stop-opacity="0.48" />
          </radialGradient>
        </defs>

        <circle class="outer-ring" cx="260" cy="260" r="252" />
        <circle class="inner-dial" cx="260" cy="260" r="205" style="fill:url(#dialGradientBoost)" />
        <circle class="black-mask" cx="260" cy="260" r="116" />
        <circle class="inner-ring" cx="260" cy="260" r="119" />
        <circle class="inner-ring" cx="260" cy="260" r="109" />

        <g id="boost-blue-arc"></g>
        <g id="boost-red-arc"></g>
        <g id="boost-ticks"></g>
        <g id="boost-labels"></g>
        <g id="iat-temp-arc"></g>
        <g id="iat-temp-ticks"></g>
        <g id="iat-temp-labels"></g>

        <line id="boostNeedle" class="needle-blue" x1="202.3" y1="149.0" x2="166.0" y2="79.0" />
        <line id="iatNeedle" class="needle-red temp-sub-needle" x1="260" y1="385" x2="260" y2="455" />

        <text class="center-value" id="cardash_mgp" x="260" y="252">0</text>
        <text class="unit" x="260" y="289">KPA</text>
        <text class="subtitle" x="260" y="330">MGP / BOOST</text>

        <text class="small-label temp-readout-label" x="260" y="410">IAT</text>
        <text class="small-label temp-readout-value" id="cardash_iat" x="260" y="435">0</text>
        <text class="small-label temp-readout-unit" x="260" y="458">°C</text>
      </svg>

      <svg id="rpmGauge" class="gauge" viewBox="0 0 520 520" role="img" aria-label="RPM gauge">
        <defs>
          <radialGradient id="dialGradientRpm" cx="50%" cy="50%" r="55%">
            <stop offset="0%" stop-color="#000" />
            <stop offset="55%" stop-color="rgb(var(--dial-rgb))" stop-opacity="0.22" />
            <stop offset="100%" stop-color="rgb(var(--dial-rgb))" stop-opacity="0.48" />
          </radialGradient>
          <radialGradient id="redGlow" cx="35%" cy="30%" r="72%">
            <stop offset="0%" stop-color="#ff2a2a" />
            <stop offset="55%" stop-color="#d00000" />
            <stop offset="100%" stop-color="#700000" />
          </radialGradient>
        </defs>

        <circle class="outer-ring" cx="260" cy="260" r="252" />
        <circle class="inner-dial" cx="260" cy="260" r="205" style="fill:url(#dialGradientRpm)" />
        <circle class="black-mask" cx="260" cy="260" r="116" />
        <circle class="inner-ring" cx="260" cy="260" r="119" />
        <circle class="inner-ring" cx="260" cy="260" r="109" />

        <g id="rpm-left-arc"></g>
        <g id="rpm-top-track"></g>
        <g id="rpm-red-arc"></g>
        <g id="rpm-ticks"></g>
        <g id="rpm-labels"></g>
        <g id="ect-temp-arc"></g>
        <g id="ect-temp-ticks"></g>
        <g id="ect-temp-labels"></g>

        <line id="rpmNeedle" class="needle-blue" x1="202.3" y1="149.0" x2="166.0" y2="79.0" />
        <line id="ectNeedle" class="needle-red temp-sub-needle" x1="260" y1="385" x2="260" y2="455" />

        <text class="center-value" id="cardash_rpm" x="260" y="252">0</text>
        <text class="unit" x="260" y="289">RPM</text>
        <text class="subtitle" x="260" y="116">RPM x 1000</text>

        <circle class="warning-light" id="rpmWarningLight" cx="212" cy="419" r="19" />

        <text class="small-label temp-readout-label" x="260" y="410">ECT</text>
        <text class="small-label temp-readout-value" id="cardash_ect" x="260" y="435">0</text>
        <text class="small-label temp-readout-unit" x="260" y="458">°C</text>
      </svg>
    </main>

    <div class="dash-aux">
      <div class="dash-pill" id="cardash_3v3_panel">
        <span>3.3V INTERNAL</span>
        <strong id="cardash_3v3">0.00</strong>
      </div>
      <div class="dash-pill" id="cardash_12v_panel">
        <span>12V INTERNAL</span>
        <strong id="cardash_12v">0.0</strong>
      </div>
      <div class="dash-pill battery-pill" id="cardash_batt_panel">
        <span>BATTERY</span>
        <div class="battery-pill-main">
          <strong id="cardash_batt_aux">0.0</strong>
          <div class="battery-level" aria-label="Battery level indicator">
            <div class="battery-level-fill" id="batteryLevelFill" style="width:0%"></div>
          </div>
        </div>
      </div>
    </div>
  </section>

<section id="page_driving" class="page">
    <div class="summary" id="driving_summary">Main live driving values.</div>

    <div class="grid">
      <div class="card wide" id="card_rpm">
        <div class="label">Engine Speed</div>
        <div class="value" id="rpm">0</div>
        <div class="unit">rpm</div>
      </div>

      <div class="card" id="card_mgp">
        <div class="label">MGP / Boost</div>
        <div class="value" id="mgp">0</div>
        <div class="unit">kPa gauge</div>
      </div>

      <div class="card" id="card_map">
        <div class="label">MAP</div>
        <div class="value" id="map">0</div>
        <div class="unit">kPa absolute</div>
      </div>

      <div class="card" id="card_lambda">
        <div class="label">Lambda</div>
        <div class="value" id="lambda1">0.00</div>
        <div class="unit">actual</div>
      </div>

      <div class="card" id="card_lambda_target">
        <div class="label">Lambda Target</div>
        <div class="value" id="lambda_target">0.00</div>
        <div class="unit">target</div>
      </div>

      <div class="card" id="card_ect">
        <div class="label">Coolant</div>
        <div class="value" id="ect">0</div>
        <div class="unit">°C</div>
      </div>

      <div class="card" id="card_oil_pressure">
        <div class="label">Oil Pressure</div>
        <div class="value" id="oil_pressure">0</div>
        <div class="unit">kPa</div>
      </div>

      <div class="card" id="card_battery">
        <div class="label">Battery</div>
        <div class="value" id="battery_v">0.0</div>
        <div class="unit">V</div>
      </div>

      <div class="card" id="card_tps">
        <div class="label">TPS Main</div>
        <div class="value" id="tps">0</div>
        <div class="unit">%</div>
      </div>
    </div>
  </section>

  <section id="page_health" class="page">
    <div class="summary">Engine health, sensor sanity, voltage rails, and trigger/lambda status.</div>

    <div class="grid">
      <div class="card" id="card_health_ect">
        <div class="label">Coolant</div>
        <div class="value" id="health_ect">0</div>
        <div class="unit">°C</div>
      </div>

      <div class="card" id="card_iat">
        <div class="label">Intake Air Temp</div>
        <div class="value" id="iat">0</div>
        <div class="unit">°C</div>
      </div>

      <div class="card" id="card_oil_temp">
        <div class="label">Oil Temp</div>
        <div class="value" id="oil_temp">0</div>
        <div class="unit">°C</div>
      </div>

      <div class="card" id="card_health_oil_pressure">
        <div class="label">Oil Pressure</div>
        <div class="value" id="health_oil_pressure">0</div>
        <div class="unit">kPa</div>
      </div>

      <div class="card" id="card_fuel_pressure">
        <div class="label">Fuel Pressure</div>
        <div class="value" id="fuel_pressure">0</div>
        <div class="unit">kPa</div>
      </div>

      <div class="card" id="card_health_battery">
        <div class="label">Battery</div>
        <div class="value" id="health_battery_v">0.0</div>
        <div class="unit">V</div>
      </div>

      <div class="card" id="card_3v3">
        <div class="label">3.3V Internal</div>
        <div class="value" id="internal_3v3">0.00</div>
        <div class="unit">V</div>
      </div>

      <div class="card" id="card_12v">
        <div class="label">12V Internal</div>
        <div class="value" id="internal_12v">0.0</div>
        <div class="unit">V</div>
      </div>

      <div class="card" id="card_trig">
        <div class="label">Trig1 Err Counter</div>
        <div class="value" id="trig1_err">0</div>
        <div class="unit">count</div>
      </div>

      <div class="card" id="card_lambda_status">
        <div class="label">Lambda Status</div>
        <div class="value" id="lambda_status">0</div>
        <div class="unit">status code</div>
      </div>

      <div class="card" id="card_lambda_temp">
        <div class="label">Lambda Temp</div>
        <div class="value" id="lambda_temp">0</div>
        <div class="unit">°C</div>
      </div>
    </div>
  </section>

  <section id="page_gear" class="page">
    <div class="summary">Tuning and control-loop debug values.</div>

    <div class="grid">
      <div class="card">
        <div class="label">Ignition Angle</div>
        <div class="value" id="ignition_angle">0.0</div>
        <div class="unit">deg</div>
      </div>

      <div class="card">
        <div class="label">Injection Actual PW</div>
        <div class="value" id="injection_actual_pw">0.0</div>
        <div class="unit">ms</div>
      </div>

      <div class="card">
        <div class="label">Injection Effective PW</div>
        <div class="value" id="injection_effective_pw">0.0</div>
        <div class="unit">ms</div>
      </div>

      <div class="card" id="card_lambda_error">
        <div class="label">Lambda Error</div>
        <div class="value" id="lambda_error">0.00</div>
        <div class="unit">λ</div>
      </div>

      <div class="card">
        <div class="label">Boost Target</div>
        <div class="value" id="boost_target">0</div>
        <div class="unit">kPa</div>
      </div>

      <div class="card" id="card_boost_error">
        <div class="label">Boost Target Error</div>
        <div class="value" id="boost_error">0</div>
        <div class="unit">kPa</div>
      </div>

      <div class="card">
        <div class="label">Boost P</div>
        <div class="smallvalue" id="boost_p">0.0</div>
        <div class="unit">proportional</div>
      </div>

      <div class="card">
        <div class="label">Boost I</div>
        <div class="smallvalue" id="boost_i">0.0</div>
        <div class="unit">integral</div>
      </div>

      <div class="card">
        <div class="label">Boost D</div>
        <div class="smallvalue" id="boost_d">0.0</div>
        <div class="unit">derivative</div>
      </div>

      <div class="card">
        <div class="label">Boost Duty</div>
        <div class="value" id="boost_duty">0</div>
        <div class="unit">%</div>
      </div>

      <div class="card">
        <div class="label">APS Main</div>
        <div class="value" id="aps_main">0</div>
        <div class="unit">%</div>
      </div>

      <div class="card">
        <div class="label">E-Throttle Target</div>
        <div class="value" id="throttle_target">0</div>
        <div class="unit">%</div>
      </div>

      <div class="card">
        <div class="label">VVT Inlet Target</div>
        <div class="value" id="vvt_in_target">0</div>
        <div class="unit">deg</div>
      </div>

      <div class="card">
        <div class="label">VVT Inlet Position</div>
        <div class="value" id="vvt_in_pos">0</div>
        <div class="unit">deg</div>
      </div>
    </div>
  </section>

  <section id="page_light" class="page">
    <div class="summary">Cabin RGBW lighting controls.</div>

    <div class="grid">
      <div class="card wide">
        <div class="label">Lighting Enabled</div>
        <button id="lighting_enabled_btn" class="active" onclick="toggleLightingEnabled()">On</button>
      </div>

      <div class="card wide">
        <div class="label">Auto-off Timer</div>
        <input
          type="number"
          id="lighting_auto_off_minutes"
          min="0"
          max="1440"
          step="1"
          value="0"
          onchange="applyLighting()"
        >
        <div class="unit">Minutes after last ECU packet before lights turn off. Use 0 to disable.</div>
      </div>

      <div class="card wide">
        <div class="label">Mode</div>
        <select id="lighting_mode" onchange="applyLighting(); updateLightingCardVisibility()">
          <option value="static">Static Colour</option>
          <option value="pattern">Pattern / Theme</option>
        </select>
      </div>

      <div class="card wide" id="card_static_colour">
        <div class="label">Static Colour</div>
        <input type="color" id="static_color" value="#0050ff" onchange="applyLighting()">
      </div>

      <div class="card wide" id="card_pattern_theme" style="display:none">
        <div class="label">Pattern / Theme</div>
        <select id="lighting_pattern" onchange="applyLighting(); updateLightingCardVisibility()">
          <option value="engine_plasma">Engine Plasma</option>
          <option value="breathing">Breathing</option>
          <option value="rainbow">Rainbow</option>
          <option value="color_chase">Color Chase</option>
          <option value="lightning">Lightning</option>
          <option value="off">Off</option>
        </select>
      </div>

      <div class="card wide" id="card_params_engine_plasma" style="display:none">
        <div class="label">Engine Plasma Settings</div>
        <div class="unit">RPM range (brightness mapping)</div>
        <div style="display:flex;gap:16px;flex-wrap:wrap;margin:6px 0 10px;">
          <label style="display:flex;flex-direction:column;gap:4px;font-size:0.85em;">Min RPM
            <input type="number" id="plasma_rpm_min" min="0" max="9000" step="100" value="1100" onchange="applyLighting()" style="width:90px;">
          </label>
          <label style="display:flex;flex-direction:column;gap:4px;font-size:0.85em;">Max RPM
            <input type="number" id="plasma_rpm_max" min="0" max="9000" step="100" value="4200" onchange="applyLighting()" style="width:90px;">
          </label>
        </div>
        <div class="unit">MAP range (colour mapping, kPa)</div>
        <div style="display:flex;gap:16px;flex-wrap:wrap;margin:6px 0;">
          <label style="display:flex;flex-direction:column;gap:4px;font-size:0.85em;">Min MAP
            <input type="number" id="plasma_map_min" min="0" max="300" step="5" value="30" onchange="applyLighting()" style="width:90px;">
          </label>
          <label style="display:flex;flex-direction:column;gap:4px;font-size:0.85em;">Max MAP
            <input type="number" id="plasma_map_max" min="0" max="300" step="5" value="70" onchange="applyLighting()" style="width:90px;">
          </label>
        </div>
      </div>

      <div class="card wide" id="card_params_breathing" style="display:none">
        <div class="label">Breathing Settings</div>
        <div class="unit">Speed (use Static Colour below to set the colour)</div>
        <input type="range" id="breathing_speed" min="1" max="100" value="10" oninput="applyLighting()">
        <div class="unit"><span id="breathing_speed_label">1.0</span>×</div>
      </div>

      <div class="card wide" id="card_params_rainbow" style="display:none">
        <div class="label">Rainbow Settings</div>
        <div class="unit">Speed</div>
        <input type="range" id="rainbow_speed" min="1" max="100" value="10" oninput="applyLighting()">
        <div class="unit"><span id="rainbow_speed_label">1.0</span>×</div>
      </div>

      <div class="card wide" id="card_params_color_chase" style="display:none">
        <div class="label">Color Chase Settings</div>
        <div class="unit">Chase colours &amp; widths (pixels)</div>
        <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:10px;margin:8px 0 10px;">
          <label style="display:flex;flex-direction:column;align-items:center;gap:4px;font-size:0.85em;">Color 1
            <input type="color" id="chase_c1" value="#ff0000" onchange="applyLighting()">
            <input type="number" id="chase_w1" min="1" max="500" value="50" style="width:72px;text-align:center;" oninput="applyLighting()">
          </label>
          <label style="display:flex;flex-direction:column;align-items:center;gap:4px;font-size:0.85em;">Color 2
            <input type="color" id="chase_c2" value="#00ff00" onchange="applyLighting()">
            <input type="number" id="chase_w2" min="1" max="500" value="50" style="width:72px;text-align:center;" oninput="applyLighting()">
          </label>
          <label style="display:flex;flex-direction:column;align-items:center;gap:4px;font-size:0.85em;">Color 3
            <input type="color" id="chase_c3" value="#0000ff" onchange="applyLighting()">
            <input type="number" id="chase_w3" min="1" max="500" value="50" style="width:72px;text-align:center;" oninput="applyLighting()">
          </label>
          <label style="display:flex;flex-direction:column;align-items:center;gap:4px;font-size:0.85em;">Color 4
            <input type="color" id="chase_c4" value="#ff8000" onchange="applyLighting()">
            <input type="number" id="chase_w4" min="1" max="500" value="50" style="width:72px;text-align:center;" oninput="applyLighting()">
          </label>
        </div>
        <div class="unit">Speed</div>
        <input type="range" id="chase_speed" min="1" max="100" value="10" oninput="applyLighting()">
        <div class="unit"><span id="chase_speed_label">1.0</span>×</div>
      </div>

      <div class="card wide" id="card_params_lightning" style="display:none">
        <div class="label">Lightning Settings</div>
        <div class="unit">Flash colours (randomly selected per strike)</div>
        <div style="display:flex;gap:14px;flex-wrap:wrap;margin:6px 0 10px;">
          <label style="display:flex;flex-direction:column;align-items:center;gap:4px;font-size:0.85em;">Color 1
            <input type="color" id="lightning_c1" value="#c8c8ff" onchange="applyLighting()">
          </label>
          <label style="display:flex;flex-direction:column;align-items:center;gap:4px;font-size:0.85em;">Color 2
            <input type="color" id="lightning_c2" value="#c8c8ff" onchange="applyLighting()">
          </label>
          <label style="display:flex;flex-direction:column;align-items:center;gap:4px;font-size:0.85em;">Color 3
            <input type="color" id="lightning_c3" value="#c8c8ff" onchange="applyLighting()">
          </label>
        </div>
        <div class="unit">Frequency (flashes / sec)</div>
        <input type="range" id="lightning_freq" min="1" max="200" value="20" oninput="applyLighting()">
        <div class="unit"><span id="lightning_freq_label">2.0</span> Hz</div>
      </div>

      <div class="card wide" id="card_live_lighting" style="display:none">
        <div class="label">Live Lighting Output</div>
        <div id="lighting_preview"
            style="height:80px;border-radius:14px;border:1px solid #555;background:#000;margin-bottom:10px;">
        </div>
        <div class="unit" id="lighting_preview_text">RGBW: 0, 0, 0, 0</div>
        <div class="unit" id="lighting_preview_mode">Mode: --</div>
      </div>

      <div class="card wide">
        <div class="label">Max Brightness</div>
        <input type="range" id="lighting_brightness" min="0" max="100" value="100" oninput="applyLighting()">
        <div class="unit"><span id="brightness_label">100</span>%</div>
      </div>

      <div class="card wide">
        <div class="label">Outside Zones (D13 – Exterior)</div>
        <div class="zone-grid">
          <label class="zone-row"><input type="checkbox" id="ext_z1_en" onchange="applyLighting()"><span>Outside Zone 1</span><input type="text" id="ext_z1_range" placeholder="e.g. 0-49" onchange="applyLighting()"></label>
          <label class="zone-row"><input type="checkbox" id="ext_z2_en" onchange="applyLighting()"><span>Outside Zone 2</span><input type="text" id="ext_z2_range" placeholder="e.g. 50-99" onchange="applyLighting()"></label>
          <label class="zone-row"><input type="checkbox" id="ext_z3_en" onchange="applyLighting()"><span>Outside Zone 3</span><input type="text" id="ext_z3_range" placeholder="e.g. 100-149" onchange="applyLighting()"></label>
          <label class="zone-row"><input type="checkbox" id="ext_z4_en" onchange="applyLighting()"><span>Outside Zone 4</span><input type="text" id="ext_z4_range" placeholder="e.g. 150-199" onchange="applyLighting()"></label>
        </div>
      </div>

      <div class="card wide">
        <div class="label">Interior Zones (D12 – Interior)</div>
        <div class="zone-grid">
          <label class="zone-row"><input type="checkbox" id="int_z1_en" onchange="applyLighting()"><span>Interior Zone 1</span><input type="text" id="int_z1_range" placeholder="e.g. 0-49" onchange="applyLighting()"></label>
          <label class="zone-row"><input type="checkbox" id="int_z2_en" onchange="applyLighting()"><span>Interior Zone 2</span><input type="text" id="int_z2_range" placeholder="e.g. 50-99" onchange="applyLighting()"></label>
          <label class="zone-row"><input type="checkbox" id="int_z3_en" onchange="applyLighting()"><span>Interior Zone 3</span><input type="text" id="int_z3_range" placeholder="e.g. 100-149" onchange="applyLighting()"></label>
          <label class="zone-row"><input type="checkbox" id="int_z4_en" onchange="applyLighting()"><span>Interior Zone 4</span><input type="text" id="int_z4_range" placeholder="e.g. 150-199" onchange="applyLighting()"></label>
        </div>
      </div>
    </div>
  </section>

<script>

const DASH_SVG_NS = "http://www.w3.org/2000/svg";
const DASH_CX = 260;
const DASH_CY = 260;
const DASH_MAIN_NEEDLE_INNER_R = 125;
const DASH_MAIN_NEEDLE_OUTER_R = 204;
const DASH_MAIN_NEEDLE_BASE_ANGLE = 235;
const DASH_SUB_NEEDLE_INNER_R = 126;
const DASH_SUB_NEEDLE_OUTER_R = 208;
const DASH_SUB_NEEDLE_BASE_ANGLE = 202;
const DASH_PROGRESS_ARC_R = 225;
const DASH_PROGRESS_START_ANGLE = 235;
const DASH_PROGRESS_END_ANGLE = 485;

function dashPolar(cx, cy, r, angleDeg) {
  const a = (angleDeg - 90) * Math.PI / 180;
  return { x: cx + r * Math.cos(a), y: cy + r * Math.sin(a) };
}

function dashDescribeArc(cx, cy, r, startAngle, endAngle) {
  // Swapped the points! The arc now natively draws left-to-right (from startAngle to endAngle).
  // This causes stroke-dashoffset to correctly reveal the path from the starting edge.
  const start = dashPolar(cx, cy, r, startAngle);
  const end = dashPolar(cx, cy, r, endAngle);
  const largeArc = Math.abs(endAngle - startAngle) <= 180 ? "0" : "1";
  const sweep = endAngle >= startAngle ? "1" : "0";
  return `M ${start.x} ${start.y} A ${r} ${r} 0 ${largeArc} ${sweep} ${end.x} ${end.y}`;
}

function dashAddPath(group, d, className, extra = {}) {
  if (!group) return null;
  const p = document.createElementNS(DASH_SVG_NS, "path");
  p.setAttribute("d", d);
  p.setAttribute("class", className);
  Object.entries(extra).forEach(([k, v]) => p.setAttribute(k, v));
  group.appendChild(p);
  return p;
}

function dashAddLine(group, p1, p2, className) {
  if (!group) return null;
  const line = document.createElementNS(DASH_SVG_NS, "line");
  line.setAttribute("x1", p1.x);
  line.setAttribute("y1", p1.y);
  line.setAttribute("x2", p2.x);
  line.setAttribute("y2", p2.y);
  line.setAttribute("class", className);
  group.appendChild(line);
  return line;
}

function dashAddText(group, x, y, text, className) {
  if (!group) return null;
  const t = document.createElementNS(DASH_SVG_NS, "text");
  t.setAttribute("x", x);
  t.setAttribute("y", y);
  t.setAttribute("class", className);
  t.textContent = text;
  group.appendChild(t);
  return t;
}

function dashTick(group, angle, outerR, innerR, className) {
  dashAddLine(group, dashPolar(DASH_CX, DASH_CY, outerR, angle), dashPolar(DASH_CX, DASH_CY, innerR, angle), className);
}

function buildBoostGauge() {
  if (document.getElementById("boost-blue-arc-built")) return;

  const arcGroup = document.getElementById("boost-blue-arc");
  const redGroup = document.getElementById("boost-red-arc");
  const tickGroup = document.getElementById("boost-ticks");
  const labelGroup = document.getElementById("boost-labels");

  // Dynamic arc starts empty and is updated from live MGP.
  dashAddPath(arcGroup, "", "progress-arc boost-progress-arc", { id: "boostProgressArc" });
  dashAddPath(arcGroup, "", "tick-thin", { id: "boost-blue-arc-built", opacity: "0" });

  // Keep only the red/danger static arc.
  dashAddPath(redGroup, dashDescribeArc(DASH_CX, DASH_CY, 225, 92, 128), "red-track");

  const startAngle = 235;
  const endAngle = 485;
  const min = -100;
  const max = 250;
  const values = [-100, -50, 0, 50, 100, 150, 200, 250];

  values.forEach((v) => {
    const angle = startAngle + (endAngle - startAngle) * ((v - min) / (max - min));
    dashTick(tickGroup, angle, 240, 219, "tick-major");
    const pos = dashPolar(DASH_CX, DASH_CY, 199, angle);
    dashAddText(labelGroup, pos.x, pos.y, String(v), v >= 200 ? "label red-tick" : "label");
  });

  for (let v = min; v <= max; v += 10) {
    if (values.includes(v)) continue;
    const angle = startAngle + (endAngle - startAngle) * ((v - min) / (max - min));
    let cls = "tick-minor";
    if (v >= 200) cls = "tick-minor red-tick";
    dashTick(tickGroup, angle, 230, 218, cls);
  }
}

function buildRpmGauge() {
  if (document.getElementById("rpm-left-arc-built")) return;

  const leftArc = document.getElementById("rpm-left-arc");
  const redArc = document.getElementById("rpm-red-arc");
  const tickGroup = document.getElementById("rpm-ticks");
  const labelGroup = document.getElementById("rpm-labels");

  // Dynamic arc starts empty and is updated from live RPM.
  dashAddPath(leftArc, "", "progress-arc", { id: "rpmProgressArc" });
  dashAddPath(leftArc, "", "tick-thin", { id: "rpm-left-arc-built", opacity: "0" });

  // Keep only the red/danger static arc.
  dashAddPath(redArc, dashDescribeArc(DASH_CX, DASH_CY, 225, 80, 128), "red-track");

  const startAngle = 235;
  const endAngle = 485;
  const max = 8;

  for (let v = 0; v <= max; v++) {
    const angle = startAngle + (endAngle - startAngle) * (v / max);
    dashTick(tickGroup, angle, 240, 219, "tick-major");
    const pos = dashPolar(DASH_CX, DASH_CY, 201, angle);
    dashAddText(labelGroup, pos.x, pos.y, String(v), v >= 7 ? "label red-tick" : "label");
  }

  for (let v = 0; v <= max; v += 0.2) {
    const rounded = Math.round(v * 10) / 10;
    if (Math.abs(rounded - Math.round(rounded)) < 0.001) continue;
    const angle = startAngle + (endAngle - startAngle) * (v / max);
    let cls = "tick-minor";
    if (v >= 6.5 && v < 7.2) cls = "tick-minor yellow-tick";
    if (v >= 7.2) cls = "tick-minor red-tick";
    dashTick(tickGroup, angle, 230, 218, cls);
  }
}

function buildTempSubGauge(prefix) {
  if (document.getElementById(prefix + "-temp-built")) return;

  const arcGroup = document.getElementById(prefix + "-temp-arc");
  const tickGroup = document.getElementById(prefix + "-temp-ticks");
  const labelGroup = document.getElementById(prefix + "-temp-labels");

  // Lower centre arch, separate from the main gauge sweep.
  // Min is lower-left and max is lower-right.
  const startAngle = 202;
  const endAngle = 158;
  const isIat = prefix === "iat";

  const min = 0;
  const max = isIat ? 60 : 140;
  const majorValues = isIat ? [0, 20, 40, 60] : [0, 40, 80, 110, 140];

  // Hidden marker so the gauge only builds once.
  dashAddPath(arcGroup, "", "tick-thin", { id: prefix + "-temp-built", opacity: "0" });

  // IAT intentionally has no red arc. ECT red arc covers 110-140 only.
  if (!isIat) {
    const redStartValue = 110;
    const redStartAngle = startAngle + (endAngle - startAngle) * ((redStartValue - min) / (max - min));
    dashAddPath(arcGroup, dashDescribeArc(DASH_CX, DASH_CY, 225, redStartAngle, endAngle), "temp-track-danger");
  }

  majorValues.forEach((v) => {
    const angle = startAngle + (endAngle - startAngle) * ((v - min) / (max - min));
    const danger = !isIat && v >= 110;
    dashTick(tickGroup, angle, 236, 217, danger ? "tick-major red-tick" : "tick-major");
    const pos = dashPolar(DASH_CX, DASH_CY, 199, angle);
    dashAddText(labelGroup, pos.x, pos.y, String(v), danger ? "temp-scale-label red-tick" : "temp-scale-label");
  });

  const minorStep = isIat ? 5 : 10;
  for (let v = min; v <= max; v += minorStep) {
    if (majorValues.includes(v)) continue;
    const angle = startAngle + (endAngle - startAngle) * ((v - min) / (max - min));
    const danger = !isIat && v >= 110;
    dashTick(tickGroup, angle, 232, 222, danger ? "tick-minor red-tick" : "tick-minor");
  }
}

function buildCarDashGauges() {
  buildBoostGauge();
  buildRpmGauge();
  buildTempSubGauge("iat");
  buildTempSubGauge("ect");
  initDashNeedle("boostNeedle", DASH_MAIN_NEEDLE_INNER_R, DASH_MAIN_NEEDLE_OUTER_R, DASH_MAIN_NEEDLE_BASE_ANGLE);
  initDashNeedle("rpmNeedle", DASH_MAIN_NEEDLE_INNER_R, DASH_MAIN_NEEDLE_OUTER_R, DASH_MAIN_NEEDLE_BASE_ANGLE);
  initDashNeedle("iatNeedle", DASH_SUB_NEEDLE_INNER_R, DASH_SUB_NEEDLE_OUTER_R, DASH_SUB_NEEDLE_BASE_ANGLE);
  initDashNeedle("ectNeedle", DASH_SUB_NEEDLE_INNER_R, DASH_SUB_NEEDLE_OUTER_R, DASH_SUB_NEEDLE_BASE_ANGLE);
  initProgressArc("boostProgressArc", DASH_PROGRESS_START_ANGLE, DASH_PROGRESS_END_ANGLE);
  initProgressArc("rpmProgressArc", DASH_PROGRESS_START_ANGLE, DASH_PROGRESS_END_ANGLE);
}

function initDashNeedle(id, innerRadius, outerRadius, baseAngle) {
  const needle = document.getElementById(id);
  if (!needle || needle.dataset.baseAngle) return;

  const inner = dashPolar(DASH_CX, DASH_CY, innerRadius, baseAngle);
  const outer = dashPolar(DASH_CX, DASH_CY, outerRadius, baseAngle);

  needle.setAttribute("x1", inner.x);
  needle.setAttribute("y1", inner.y);
  needle.setAttribute("x2", outer.x);
  needle.setAttribute("y2", outer.y);
  needle.dataset.baseAngle = String(baseAngle);
}

function initProgressArc(id, startAngle, endAngle) {
  const arc = document.getElementById(id);
  if (!arc || arc.dataset.arcLength) return;

  arc.setAttribute("d", dashDescribeArc(DASH_CX, DASH_CY, DASH_PROGRESS_ARC_R, startAngle, endAngle));
  const length = arc.getTotalLength();
  arc.style.strokeDasharray = String(length);
  arc.style.strokeDashoffset = String(length);
  arc.dataset.arcLength = String(length);
}

function setDashNeedle(id, value, min, max) {
  const needle = document.getElementById(id);
  if (!needle) return;

  const pct = clamp((value - min) / (max - min), 0, 1);
  const angle = DASH_MAIN_NEEDLE_BASE_ANGLE + (DASH_PROGRESS_END_ANGLE - DASH_PROGRESS_START_ANGLE) * pct;
  const baseAngle = Number(needle.dataset.baseAngle || DASH_MAIN_NEEDLE_BASE_ANGLE);
  const rotation = (angle - baseAngle).toFixed(2);
  
  if (needle.dataset.rotation === rotation) return;
  needle.style.transform = `rotate(${rotation}deg)`;
  needle.dataset.rotation = rotation;
}

function setLowerArcNeedle(id, value, min, max) {
  const needle = document.getElementById(id);
  if (!needle) return;

  const pct = clamp((value - min) / (max - min), 0, 1);
  const angle = DASH_SUB_NEEDLE_BASE_ANGLE + (158 - DASH_SUB_NEEDLE_BASE_ANGLE) * pct;
  const baseAngle = Number(needle.dataset.baseAngle || DASH_SUB_NEEDLE_BASE_ANGLE);
  const rotation = (angle - baseAngle).toFixed(2);
  
  if (needle.dataset.rotation === rotation) return;
  needle.style.transform = `rotate(${rotation}deg)`;
  needle.dataset.rotation = rotation;
}

function setProgressArc(id, value, min, max) {
  const arc = document.getElementById(id);
  if (!arc) return;

  const pct = clamp((value - min) / (max - min), 0, 1);
  const length = Number(arc.dataset.arcLength || 0);
  if (!length) return;
  const offset = (length * (1 - pct)).toFixed(2);
  if (arc.dataset.offset === offset) return;
  
  arc.style.strokeDashoffset = offset;
  arc.dataset.offset = offset;
}

function setSvgText(id, value, decimals = 0) {
  const el = document.getElementById(id);
  if (!el) return;
  
  // Checking before assignment prevents destructive DOM layout thrashing
  const strVal = Number(value).toFixed(decimals);
  if (el.textContent !== strVal) {
    el.textContent = strVal;
  }
}

function setText(id, value, decimals = 0) {
  const el = document.getElementById(id);
  if (!el) return;

  const n = Number(value);
  const strVal = Number.isFinite(n) ? n.toFixed(decimals) : '--';
  
  if (el.textContent !== strVal) {
    el.textContent = strVal;
  }
}

function updateBatteryLevel(voltage) {
  const fill = document.getElementById('batteryLevelFill');
  if (!fill) return;

  // UI indicator only:
  // 11.5V = empty/critical, 12.7V = full resting battery.
  // Alternator/charging voltage is capped at full.
  const pct = clamp((voltage - 11.5) / (12.7 - 11.5), 0, 1) * 100;

  fill.style.width = pct.toFixed(0) + '%';

  const isDanger = voltage > 0 && voltage < 11.5;
  const isWarn = voltage >= 11.5 && voltage < 12.2;

  fill.classList.toggle('danger', isDanger);
  fill.classList.toggle('warn', !isDanger && isWarn);
}


function hexToRgb(hex) {
  const clean = hex.replace('#', '');
  return {
    r: parseInt(clean.substring(0, 2), 16),
    g: parseInt(clean.substring(2, 4), 16),
    b: parseInt(clean.substring(4, 6), 16)
  };
}

async function goFullscreen() {
  const el = document.documentElement;

  try {
    if (el.requestFullscreen) {
      await el.requestFullscreen();
    } else if (el.webkitRequestFullscreen) {
      await el.webkitRequestFullscreen();
    }
  } catch (err) {
    console.log('Fullscreen request failed', err);
  }
}

async function applyLighting() {
  const mode = document.getElementById('lighting_mode').value;
  const pattern = document.getElementById('lighting_pattern').value;
  const color = hexToRgb(document.getElementById('static_color').value);
  const brightnessPct = Number(document.getElementById('lighting_brightness').value);
  const brightness = brightnessPct / 100.0;
  const autoOffMinutes = Number(document.getElementById('lighting_auto_off_minutes').value) || 0;

  document.getElementById('brightness_label').textContent = brightnessPct;

  const zoneIds = [
    { rangeId: 'ext_z1_range', enId: 'ext_z1_en', rangeParam: 'ext_z1_range', enParam: 'ext_z1_en' },
    { rangeId: 'ext_z2_range', enId: 'ext_z2_en', rangeParam: 'ext_z2_range', enParam: 'ext_z2_en' },
    { rangeId: 'ext_z3_range', enId: 'ext_z3_en', rangeParam: 'ext_z3_range', enParam: 'ext_z3_en' },
    { rangeId: 'ext_z4_range', enId: 'ext_z4_en', rangeParam: 'ext_z4_range', enParam: 'ext_z4_en' },
    { rangeId: 'int_z1_range', enId: 'int_z1_en', rangeParam: 'int_z1_range', enParam: 'int_z1_en' },
    { rangeId: 'int_z2_range', enId: 'int_z2_en', rangeParam: 'int_z2_range', enParam: 'int_z2_en' },
    { rangeId: 'int_z3_range', enId: 'int_z3_en', rangeParam: 'int_z3_range', enParam: 'int_z3_en' },
    { rangeId: 'int_z4_range', enId: 'int_z4_en', rangeParam: 'int_z4_range', enParam: 'int_z4_en' },
  ];

  let zonePart = '';
  for (const z of zoneIds) {
    const rangeEl = document.getElementById(z.rangeId);
    const enEl    = document.getElementById(z.enId);
    if (rangeEl) zonePart += '&' + z.rangeParam + '=' + encodeURIComponent(rangeEl.value);
    if (enEl)    zonePart += '&' + z.enParam    + '=' + (enEl.checked ? '1' : '0');
  }

  // Per-pattern parameters.
  let patternParams = '';

  // Engine Plasma.
  const pRpmMin = document.getElementById('plasma_rpm_min');
  const pRpmMax = document.getElementById('plasma_rpm_max');
  const pMapMin = document.getElementById('plasma_map_min');
  const pMapMax = document.getElementById('plasma_map_max');
  if (pRpmMin) patternParams += '&plasma_rpm_min=' + pRpmMin.value;
  if (pRpmMax) patternParams += '&plasma_rpm_max=' + pRpmMax.value;
  if (pMapMin) patternParams += '&plasma_map_min=' + pMapMin.value;
  if (pMapMax) patternParams += '&plasma_map_max=' + pMapMax.value;

  // Breathing speed.
  const breathSpeedEl = document.getElementById('breathing_speed');
  if (breathSpeedEl) {
    const spd = (Number(breathSpeedEl.value) / 10.0).toFixed(1);
    const lbl = document.getElementById('breathing_speed_label');
    if (lbl) lbl.textContent = spd;
    patternParams += '&breathing_speed=' + spd;
  }

  // Rainbow speed.
  const rainbowSpeedEl = document.getElementById('rainbow_speed');
  if (rainbowSpeedEl) {
    const spd = (Number(rainbowSpeedEl.value) / 10.0).toFixed(1);
    const lbl = document.getElementById('rainbow_speed_label');
    if (lbl) lbl.textContent = spd;
    patternParams += '&rainbow_speed=' + spd;
  }

  // Color Chase.
  const c1El = document.getElementById('chase_c1');
  const c2El = document.getElementById('chase_c2');
  const c3El = document.getElementById('chase_c3');
  const c4El = document.getElementById('chase_c4');
  const cSpeedEl = document.getElementById('chase_speed');
  if (c1El && c2El && c3El && c4El && cSpeedEl) {
    const c1 = hexToRgb(c1El.value);
    const c2 = hexToRgb(c2El.value);
    const c3 = hexToRgb(c3El.value);
    const c4 = hexToRgb(c4El.value);
    const spd = (Number(cSpeedEl.value) / 10.0).toFixed(1);
    const lbl = document.getElementById('chase_speed_label');
    if (lbl) lbl.textContent = spd;
    patternParams += '&chase_c1_r=' + c1.r + '&chase_c1_g=' + c1.g + '&chase_c1_b=' + c1.b;
    patternParams += '&chase_c2_r=' + c2.r + '&chase_c2_g=' + c2.g + '&chase_c2_b=' + c2.b;
    patternParams += '&chase_c3_r=' + c3.r + '&chase_c3_g=' + c3.g + '&chase_c3_b=' + c3.b;
    patternParams += '&chase_c4_r=' + c4.r + '&chase_c4_g=' + c4.g + '&chase_c4_b=' + c4.b;
    const w1El = document.getElementById('chase_w1');
    const w2El = document.getElementById('chase_w2');
    const w3El = document.getElementById('chase_w3');
    const w4El = document.getElementById('chase_w4');
    if (w1El) patternParams += '&chase_w1=' + Math.max(1, parseInt(w1El.value) || 50);
    if (w2El) patternParams += '&chase_w2=' + Math.max(1, parseInt(w2El.value) || 50);
    if (w3El) patternParams += '&chase_w3=' + Math.max(1, parseInt(w3El.value) || 50);
    if (w4El) patternParams += '&chase_w4=' + Math.max(1, parseInt(w4El.value) || 50);
    patternParams += '&chase_speed=' + spd;
  }

  // Lightning.
  const ltC1El = document.getElementById('lightning_c1');
  const ltC2El = document.getElementById('lightning_c2');
  const ltC3El = document.getElementById('lightning_c3');
  const ltFreqEl  = document.getElementById('lightning_freq');
  if (ltC1El && ltFreqEl) {
    const lc1  = hexToRgb(ltC1El.value);
    const freq = (Number(ltFreqEl.value) / 10.0).toFixed(1);
    const lbl  = document.getElementById('lightning_freq_label');
    if (lbl) lbl.textContent = freq;
    patternParams += '&lightning_r=' + lc1.r + '&lightning_g=' + lc1.g + '&lightning_b=' + lc1.b;
    if (ltC2El) {
      const lc2 = hexToRgb(ltC2El.value);
      patternParams += '&lightning_c2_r=' + lc2.r + '&lightning_c2_g=' + lc2.g + '&lightning_c2_b=' + lc2.b;
    }
    if (ltC3El) {
      const lc3 = hexToRgb(ltC3El.value);
      patternParams += '&lightning_c3_r=' + lc3.r + '&lightning_c3_g=' + lc3.g + '&lightning_c3_b=' + lc3.b;
    }
    patternParams += '&lightning_freq=' + freq;
  }

  const url =
    '/setLighting?' +
    'mode=' + encodeURIComponent(mode) +
    '&pattern=' + encodeURIComponent(pattern) +
    '&r=' + color.r +
    '&g=' + color.g +
    '&b=' + color.b +
    '&w=0' +
    '&brightness=' + brightness +
    '&auto_off_minutes=' + autoOffMinutes +
    zonePart +
    patternParams;

  await fetch(url);
}

async function toggleLightingEnabled() {
  const btn = document.getElementById('lighting_enabled_btn');
  const isOn = btn && btn.classList.contains('active');
  const newState = !isOn;
  await fetch('/setLighting?enabled=' + (newState ? '1' : '0'));
  updateLightingEnabledButton(newState);
}

function updateLightingEnabledButton(enabled) {
  const btn = document.getElementById('lighting_enabled_btn');
  if (!btn) return;
  btn.textContent = enabled ? 'On' : 'Off';
  btn.classList.toggle('active', enabled);
}

function updateLightingCardVisibility() {
  const modeEl = document.getElementById('lighting_mode');
  if (!modeEl) return;
  const mode = modeEl.value;

  const patternEl = document.getElementById('lighting_pattern');
  const pattern = patternEl ? patternEl.value : '';

  const staticCard  = document.getElementById('card_static_colour');
  const patternCard = document.getElementById('card_pattern_theme');
  const liveCard    = document.getElementById('card_live_lighting');
  if (staticCard)  staticCard.style.display  = (mode === 'static')  ? '' : 'none';
  if (patternCard) patternCard.style.display = (mode === 'pattern') ? '' : 'none';
  if (liveCard)    liveCard.style.display    = (mode === 'pattern') ? '' : 'none';

  // Per-pattern parameter cards.
  const paramCards = [
    'card_params_engine_plasma',
    'card_params_breathing',
    'card_params_rainbow',
    'card_params_color_chase',
    'card_params_lightning',
  ];
  for (const id of paramCards) {
    const el = document.getElementById(id);
    if (el) el.style.display = 'none';
  }
  if (mode === 'pattern' && pattern) {
    const active = document.getElementById('card_params_' + pattern);
    if (active) active.style.display = '';
  }
}

function rgbToHex(r, g, b) {
  return '#' + [r, g, b]
    .map((value) => Number(value).toString(16).padStart(2, '0'))
    .join('');
}

const PAGE_TABS = [
  { tab: 'drive', page: 'cardash' },
  { tab: 'gear', page: 'gear' },
  { tab: 'light', page: 'light' }
];

function showPage(name) {
  for (const p of PAGE_TABS) {
    document.getElementById('page_' + p.page).classList.remove('active');
    document.getElementById('btn_' + p.tab).classList.remove('active');
  }

  const target = PAGE_TABS.find((t) => t.tab === name);
  if (!target) return;

  document.getElementById('page_' + target.page).classList.add('active');
  document.getElementById('btn_' + target.tab).classList.add('active');
}

function setCardState(id, state) {
  const el = document.getElementById(id);
  if (!el) return;

  el.classList.remove('ok', 'warn', 'danger');

  if (state) {
    el.classList.add(state);
  }
}

const refreshState = {
  dataInFlight: false,
  lightingInFlight: false,
  dataPending: false,
  lightingPending: false
};

function updateWarnings(d) {
  const stale = d.age_ms > 1500;

  const oilPressureDanger = d.rpm > 1500 && d.oil_pressure < 150;
  const ectWarn = d.ect >= 100 && d.ect < 110;
  const ectDanger = d.ect >= 110;
  const battWarn = d.battery_v > 0 && d.battery_v < 12.2;
  const lambdaDanger = d.map > 120 && d.lambda1 > d.lambda_target + 0.08;
  const fuelPressureDanger = d.rpm > 1500 && d.fuel_pressure > 0 && d.fuel_pressure < 250;
  const trigDanger = d.trig1_err > 0;

  setCardState('card_ect', ectDanger ? 'danger' : ectWarn ? 'warn' : null);
  setCardState('card_health_ect', ectDanger ? 'danger' : ectWarn ? 'warn' : null);

  setCardState('card_oil_pressure', oilPressureDanger ? 'danger' : null);
  setCardState('card_health_oil_pressure', oilPressureDanger ? 'danger' : null);

  setCardState('card_battery', battWarn ? 'warn' : null);
  setCardState('card_health_battery', battWarn ? 'warn' : null);

  setCardState('card_lambda', lambdaDanger ? 'danger' : null);
  setCardState('card_fuel_pressure', fuelPressureDanger ? 'danger' : null);
  setCardState('card_trig', trigDanger ? 'danger' : null);

  setCardState('card_lambda_error', Math.abs(d.lambda_error) > 0.08 ? 'warn' : null);
  setCardState('card_boost_error', Math.abs(d.boost_error) > 20 ? 'warn' : null);

  const alertBox = document.getElementById('main_alert');

  let alerts = [];

  if (stale) alerts.push('DATA STALE');
  if (oilPressureDanger) alerts.push('LOW OIL PRESSURE');
  if (ectDanger) alerts.push('HIGH COOLANT TEMP');
  if (lambdaDanger) alerts.push('LEAN ON BOOST');
  if (fuelPressureDanger) alerts.push('LOW FUEL PRESSURE');
  if (trigDanger) alerts.push('TRIGGER ERROR');

  if (alerts.length > 0) {
    alertBox.textContent = alerts.join(' | ');
    alertBox.style.display = 'block';
  } else {
    alertBox.style.display = 'none';
  }
}

function clamp(value, min, max) {
  return Math.min(Math.max(value, min), max);
}

function valueToNeedleAngle(value, min, max) {
  const pct = clamp((value - min) / (max - min), 0, 1);

  // Gauge sweep: -130° left to +130° right.
  return -130 + pct * 260;
}

function setGaugeArc(id, value, min, max) {
  const arc = document.getElementById(id);
  if (!arc) return;

  const pct = clamp((value - min) / (max - min), 0, 1);
  const length = arc.getTotalLength();
  arc.style.strokeDasharray = length;
  arc.style.strokeDashoffset = length * (1 - pct);
}

function setGaugeNeedle(id, value, min, max) {
  const needle = document.getElementById(id);
  if (!needle) return;

  const angle = valueToNeedleAngle(value, min, max);
  needle.style.transform = 'rotate(' + angle + 'deg)';
}

function setMiniPanelState(id, state) {
  const el = document.getElementById(id);
  if (!el) return;

  el.classList.remove('warn', 'danger');
  if (state) el.classList.add(state);
}

function updateCarDash(d) {
  const rpm = Number(d.rpm) || 0;
  const mgp = Number(d.mgp) || 0;
  const map = Number(d.map) || 0;
  const ect = Number(d.ect) || 0;
  const iat = Number(d.iat) || 0;
  const battery = Number(d.battery_v) || 0;
  const internal3v3 = Number(d.internal_3v3) || 0;
  const internal12v = Number(d.internal_12v) || 0;

  setSvgText('cardash_rpm', Math.round(rpm), 0);
  setSvgText('cardash_mgp', Math.round(mgp), 0);
  setSvgText('cardash_map', Math.round(map), 0);
  setSvgText('cardash_batt', battery, 1);
  setSvgText('cardash_batt_aux', battery, 1);
  setSvgText('cardash_ect', Math.round(ect), 0);
  setSvgText('cardash_iat', Math.round(iat), 0);
  setSvgText('cardash_3v3', internal3v3, 2);
  setSvgText('cardash_12v', internal12v, 1);
  updateBatteryLevel(battery);

  setDashNeedle('rpmNeedle', rpm, 0, 8000);
  setDashNeedle('boostNeedle', mgp, -100, 250);
  setProgressArc('rpmProgressArc', rpm, 0, 8000);
  setProgressArc('boostProgressArc', mgp, 0, 250);
  setLowerArcNeedle('iatNeedle', iat, 0, 60);
  setLowerArcNeedle('ectNeedle', ect, 0, 140);

  const rpmWarning = document.getElementById('rpmWarningLight');
  if (rpmWarning) rpmWarning.classList.toggle('active', rpm >= 6500);

  const battWarn = battery > 0 && battery < 12.2;
  const battDanger = battery > 0 && battery < 11.5;
  const rail3v3Warn = internal3v3 > 0 && (internal3v3 < 3.15 || internal3v3 > 3.45);
  const rail3v3Danger = internal3v3 > 0 && (internal3v3 < 3.0 || internal3v3 > 3.6);
  const rail12vWarn = internal12v > 0 && (internal12v < 11.5 || internal12v > 13.5);
  const rail12vDanger = internal12v > 0 && (internal12v < 10.5 || internal12v > 14.5);

  setMiniPanelState('cardash_batt_panel', battDanger ? 'danger' : battWarn ? 'warn' : null);
  setMiniPanelState('cardash_3v3_panel', rail3v3Danger ? 'danger' : rail3v3Warn ? 'warn' : null);
  setMiniPanelState('cardash_12v_panel', rail12vDanger ? 'danger' : rail12vWarn ? 'warn' : null);
}

async function refreshData() {
  if (refreshState.dataInFlight) {
    refreshState.dataPending = true;
    return;
  }

  refreshState.dataInFlight = true;
  do {
    refreshState.dataPending = false;

    try {
      const res = await fetch('/data?_=' + Date.now(), { cache: 'no-store' });
      const d = await res.json();

      // Apply dialed lighting state synchronously with data update
      const carDashPage = document.getElementById('page_cardash');
      if (carDashPage) {
        carDashPage.style.setProperty(
          '--dial-rgb',
          (d.led_r || 0) + ', ' + (d.led_g || 0) + ', ' + (d.led_b || 0)
        );
      }

      updateCarDash(d);

      setText('rpm', d.rpm, 0);
      setText('mgp', d.mgp, 0);
      setText('map', d.map, 0);
      setText('lambda1', d.lambda1, 2);
      setText('lambda_target', d.lambda_target, 2);
      setText('ect', d.ect, 0);
      setText('oil_pressure', d.oil_pressure, 0);
      setText('battery_v', d.battery_v, 1);
      setText('tps', d.tps, 0);

      setText('health_ect', d.ect, 0);
      setText('iat', d.iat, 0);
      setText('oil_temp', d.oil_temp, 0);
      setText('health_oil_pressure', d.oil_pressure, 0);
      setText('fuel_pressure', d.fuel_pressure, 0);
      setText('health_battery_v', d.battery_v, 1);
      setText('internal_3v3', d.internal_3v3, 2);
      setText('internal_12v', d.internal_12v, 1);
      setText('trig1_err', d.trig1_err, 0);
      setText('lambda_status', d.lambda_status, 0);
      setText('lambda_temp', d.lambda_temp, 0);

      setText('ignition_angle', d.ignition_angle, 1);
      setText('injection_actual_pw', d.injection_actual_pw, 1);
      setText('injection_effective_pw', d.injection_effective_pw, 1);
      setText('lambda_error', d.lambda_error, 2);
      setText('boost_target', d.boost_target, 0);
      setText('boost_error', d.boost_error, 0);
      setText('boost_p', d.boost_p, 1);
      setText('boost_i', d.boost_i, 1);
      setText('boost_d', d.boost_d, 1);
      setText('boost_duty', d.boost_duty, 0);
      setText('aps_main', d.aps_main, 0);
      setText('throttle_target', d.throttle_target, 0);
      setText('vvt_in_target', d.vvt_in_target, 0);
      setText('vvt_in_pos', d.vvt_in_pos, 0);

      const status = document.getElementById('status');

      if (d.age_ms > 1500) {
        status.textContent = 'Data stale — last packet ' + d.age_ms + ' ms ago';
        status.style.color = '#ff7777';
      } else {
        status.textContent = 'Live — last packet ' + d.age_ms + ' ms ago';
        status.style.color = '#9da7b4';
      }

      const summary = document.getElementById('driving_summary');
      const summaryText =
        'RPM ' + Math.round(d.rpm) +
        ' | MAP ' + Math.round(d.map) + ' kPa' +
        ' | Lambda ' + Number(d.lambda1).toFixed(2) +
        ' / target ' + Number(d.lambda_target).toFixed(2) +
        ' | Oil ' + Math.round(d.oil_pressure) + ' kPa';
        
      if (summary.textContent !== summaryText) {
          summary.textContent = summaryText;
      }

      updateWarnings(d);

    } catch (err) {
      const status = document.getElementById('status');
      status.textContent = 'Dashboard fetch error';
      status.style.color = '#ff7777';
    }
  } while (refreshState.dataPending);

  refreshState.dataInFlight = false;
}

// Lighting endpoint is now pulled much less frequently just for the UI form data
// since LED data is merged directly into the fast data payload above.
async function refreshLightingState() {
  if (refreshState.lightingInFlight) {
    refreshState.lightingPending = true;
    return;
  }

  refreshState.lightingInFlight = true;

  do {
    refreshState.lightingPending = false;

    try {
      const res = await fetch('/lightingState?_=' + Date.now(), { cache: 'no-store' });
      const s = await res.json();

      const preview = document.getElementById('lighting_preview');
      const text = document.getElementById('lighting_preview_text');
      const mode = document.getElementById('lighting_preview_mode');

      if (preview && text && mode) {
        preview.style.backgroundColor =
          'rgb(' + s.preview_r + ',' + s.preview_g + ',' + s.preview_b + ')';

        const previewTextStr = 'RGBW: ' + s.r + ', ' + s.g + ', ' + s.b + ', ' + s.w;
        if (text.textContent !== previewTextStr) text.textContent = previewTextStr;

        const autoOffText =
          Number(s.auto_off_minutes) > 0
            ? ' / Auto-off: ' + s.auto_off_minutes + ' min' + (s.auto_off_expired ? ' active' : '')
            : ' / Auto-off: disabled';

        const modeTextStr = 'Mode: ' + s.mode + ' / Pattern: ' + s.pattern +
                            ' / Brightness: ' + Math.round(s.max_brightness * 100) + '%' +
                            autoOffText;
        if (mode.textContent !== modeTextStr) mode.textContent = modeTextStr;

        const modeSelect = document.getElementById('lighting_mode');
        if (modeSelect) {
          modeSelect.value = s.mode;
          updateLightingCardVisibility();
        }

        const patternSelect = document.getElementById('lighting_pattern');
        if (patternSelect) {
          patternSelect.value = s.pattern;
          updateLightingCardVisibility();
        }

        const staticColor = document.getElementById('static_color');
        if (staticColor) {
          staticColor.value = rgbToHex(s.static_r, s.static_g, s.static_b);
        }

        const brightness = document.getElementById('lighting_brightness');
        const brightnessLabel = document.getElementById('brightness_label');
        const brightnessPct = Math.round((Number(s.max_brightness) || 0) * 100);
        if (brightness) brightness.value = String(brightnessPct);
        if (brightnessLabel && brightnessLabel.textContent !== String(brightnessPct)) {
          brightnessLabel.textContent = brightnessPct;
        }
        
        const autoOffInput = document.getElementById('lighting_auto_off_minutes');
        if (autoOffInput && document.activeElement !== autoOffInput) {
          autoOffInput.value = String(s.auto_off_minutes || 0);
        }

        updateLightingEnabledButton(s.enabled);

        // Populate zone inputs.
        const zoneMap = [
          { zones: s.exterior_zones, rangePrefix: 'ext_z', enPrefix: 'ext_z' },
          { zones: s.interior_zones, rangePrefix: 'int_z', enPrefix: 'int_z' },
        ];
        for (const group of zoneMap) {
          if (!Array.isArray(group.zones)) continue;
          for (let i = 0; i < group.zones.length && i < 4; i++) {
            const z = group.zones[i];
            const idx = i + 1;
            const rangeEl = document.getElementById(group.rangePrefix + idx + '_range');
            const enEl    = document.getElementById(group.enPrefix   + idx + '_en');
            if (rangeEl && document.activeElement !== rangeEl) {
              rangeEl.value = z.start + '-' + z.end;
            }
            if (enEl) enEl.checked = z.enabled;
          }
        }

        // Populate per-pattern parameter inputs.
        function setInput(id, value) {
          const el = document.getElementById(id);
          if (el && document.activeElement !== el) el.value = String(value);
        }
        function setLabel(id, value) {
          const el = document.getElementById(id);
          if (el) el.textContent = String(value);
        }
        function setColorInput(id, r, g, b) {
          const el = document.getElementById(id);
          if (el) el.value = rgbToHex(r, g, b);
        }

        if (s.plasma_rpm_min !== undefined) setInput('plasma_rpm_min', Math.round(s.plasma_rpm_min));
        if (s.plasma_rpm_max !== undefined) setInput('plasma_rpm_max', Math.round(s.plasma_rpm_max));
        if (s.plasma_map_min !== undefined) setInput('plasma_map_min', Math.round(s.plasma_map_min));
        if (s.plasma_map_max !== undefined) setInput('plasma_map_max', Math.round(s.plasma_map_max));

        if (s.breathing_speed !== undefined) {
          const sliderVal = Math.round(Number(s.breathing_speed) * 10);
          setInput('breathing_speed', sliderVal);
          setLabel('breathing_speed_label', Number(s.breathing_speed).toFixed(1));
        }

        if (s.rainbow_speed !== undefined) {
          const sliderVal = Math.round(Number(s.rainbow_speed) * 10);
          setInput('rainbow_speed', sliderVal);
          setLabel('rainbow_speed_label', Number(s.rainbow_speed).toFixed(1));
        }

        if (s.chase_c1_r !== undefined) setColorInput('chase_c1', s.chase_c1_r, s.chase_c1_g, s.chase_c1_b);
        if (s.chase_c2_r !== undefined) setColorInput('chase_c2', s.chase_c2_r, s.chase_c2_g, s.chase_c2_b);
        if (s.chase_c3_r !== undefined) setColorInput('chase_c3', s.chase_c3_r, s.chase_c3_g, s.chase_c3_b);
        if (s.chase_c4_r !== undefined) setColorInput('chase_c4', s.chase_c4_r, s.chase_c4_g, s.chase_c4_b);
        if (s.chase_w1 !== undefined) setInput('chase_w1', s.chase_w1);
        if (s.chase_w2 !== undefined) setInput('chase_w2', s.chase_w2);
        if (s.chase_w3 !== undefined) setInput('chase_w3', s.chase_w3);
        if (s.chase_w4 !== undefined) setInput('chase_w4', s.chase_w4);
        if (s.chase_speed !== undefined) {
          const sliderVal = Math.round(Number(s.chase_speed) * 10);
          setInput('chase_speed', sliderVal);
          setLabel('chase_speed_label', Number(s.chase_speed).toFixed(1));
        }

        if (s.lightning_r !== undefined) setColorInput('lightning_c1', s.lightning_r, s.lightning_g, s.lightning_b);
        if (s.lightning_c2_r !== undefined) setColorInput('lightning_c2', s.lightning_c2_r, s.lightning_c2_g, s.lightning_c2_b);
        if (s.lightning_c3_r !== undefined) setColorInput('lightning_c3', s.lightning_c3_r, s.lightning_c3_g, s.lightning_c3_b);
        if (s.lightning_freq !== undefined) {
          const sliderVal = Math.round(Number(s.lightning_freq) * 10);
          setInput('lightning_freq', sliderVal);
          setLabel('lightning_freq_label', Number(s.lightning_freq).toFixed(1));
        }
      }
    } catch (err) {
      console.log('Lighting state refresh failed', err);
    }
  } while (refreshState.lightingPending);

  refreshState.lightingInFlight = false;
}

buildCarDashGauges();
updateLightingCardVisibility();
setInterval(refreshData, 50);
refreshData();

setInterval(refreshLightingState, 2000); // Backed off 800% to unchoke the ESP32 network stack
refreshLightingState();
</script>

</body>
</html>
)rawliteral";
}
