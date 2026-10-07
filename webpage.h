// MEPA Hand Controller web app. Served from flash by the ESP32.
#pragma once
#include <pgmspace.h>

const char INDEX_HTML[] PROGMEM = R"MEPA(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#0a0a0a">
<title>MEPA Hand</title>
<style>
:root{--o:#FF6A00;--o2:#ff8a33;--bg:#0a0a0a;--card:#141414;--card2:#1c1c1c;--line:#2b2b2b;--tx:#f1f1f1;--dim:#9a9a9a;--bad:#ff5a4a;--ok:#35d07f}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;background:var(--bg);color:var(--tx);font-family:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;font-size:15px}
body{background:radial-gradient(900px 400px at 50% -100px,#2a1400 0%,var(--bg) 70%) no-repeat,var(--bg);min-height:100vh;padding-bottom:92px}
.mono{font-family:ui-monospace,Consolas,monospace}
svg.ic{width:20px;height:20px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round;flex:none}
svg.ic.lg{width:30px;height:30px;color:var(--o)}
header{position:sticky;top:0;z-index:5;display:flex;justify-content:space-between;align-items:center;gap:8px;padding:10px 16px;background:rgba(10,10,10,.92);backdrop-filter:blur(8px);border-bottom:1px solid var(--line)}
.brand{display:flex;align-items:center;gap:10px;line-height:1.1}
.brand b{color:var(--o);font-size:19px;letter-spacing:.5px}
.brand small{display:block;color:var(--dim);font-size:11px;letter-spacing:1.5px;text-transform:uppercase}
.chips{display:flex;gap:6px;flex-wrap:wrap;justify-content:flex-end}
.chip{display:inline-flex;align-items:center;gap:6px;border:1px solid var(--line);background:var(--card);border-radius:99px;padding:4px 10px;font-size:12px;color:var(--dim)}
.chip i{width:8px;height:8px;border-radius:50%;background:var(--bad)}
.chip.ok i{background:var(--ok)}.chip.ok{color:var(--tx)}
.chip.rec{color:#fff;border-color:var(--bad);background:#3a1410;display:none}
.chip.rec i{background:var(--bad);animation:blink 1s infinite}
.chip.warn{color:var(--o);border-color:#5a2c00}
@keyframes blink{50%{opacity:.2}}
main{max-width:980px;margin:0 auto;padding:12px 16px}
.page{display:none}.page.active{display:block}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(310px,1fr));gap:12px}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:14px;margin-bottom:12px}
.grid .card{margin-bottom:0}
.ch{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px}
h2{margin:0;font-size:15px;letter-spacing:.4px;display:flex;align-items:center;gap:8px;text-transform:uppercase}
h2 svg{color:var(--o)}
h3{margin:14px 0 8px;font-size:13px;color:var(--o);text-transform:uppercase;letter-spacing:1px}
.badge{font-size:12px;font-weight:700;border-radius:8px;padding:3px 9px;background:var(--card2);color:var(--dim);border:1px solid var(--line)}
.badge.on{background:var(--o);color:#111;border-color:var(--o)}
.row{display:flex;gap:10px;align-items:center;flex-wrap:wrap}
.btn{background:var(--card2);border:1px solid var(--line);color:var(--tx);padding:10px 14px;border-radius:12px;font:inherit;font-weight:600;display:inline-flex;gap:8px;align-items:center;justify-content:center;cursor:pointer;transition:.15s}
.btn:active{transform:scale(.97)}
.btn.sm{padding:7px 11px;font-size:13px}
.btn.big{flex:1;min-width:170px;padding:13px 16px}
.btn.pri,.btn.on{background:var(--o);color:#111;border-color:var(--o)}
.btn.danger{border-color:#7a2a22;color:var(--bad)}
.btn.danger.fill{background:var(--bad);color:#150403;border-color:var(--bad)}
.btn:disabled{opacity:.4;pointer-events:none}
.sw{display:inline-flex;align-items:center;gap:8px;cursor:pointer;user-select:none}
.sw input{display:none}
.sw .knob{width:44px;height:25px;border-radius:99px;background:#2a2a2a;position:relative;transition:.2s;border:1px solid var(--line)}
.sw .knob:after{content:"";position:absolute;top:2px;left:2px;width:19px;height:19px;border-radius:50%;background:#888;transition:.2s}
.sw input:checked+.knob{background:var(--o);border-color:var(--o)}
.sw input:checked+.knob:after{left:21px;background:#111}
.sw input:disabled+.knob{opacity:.4}
.sw em{font-style:normal;color:var(--dim);font-size:13px}
canvas.chart{width:100%;height:170px;display:block;margin-top:10px;background:#0f0f0f;border:1px solid var(--line);border-radius:10px}
.legend{display:flex;gap:12px;flex-wrap:wrap;color:var(--dim);font-size:12px;margin-top:8px}
.legend span{display:inline-flex;align-items:center;gap:5px}.legend i{width:14px;height:3px;border-radius:2px;display:inline-block}
.stats{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:10px}
.stat{background:var(--card2);border-radius:10px;padding:8px 10px}
.stat small{display:block;color:var(--dim);font-size:11px;text-transform:uppercase;letter-spacing:.8px}
.stat b{font-size:19px}
.hand-wrap{display:grid;grid-template-columns:minmax(150px,230px) 1fr;gap:12px;align-items:center}
@media(max-width:420px){.hand-wrap{grid-template-columns:1fr}}
#hand{width:100%;max-width:240px;height:auto;display:block;margin:0 auto}
#hand .fing{fill:#1d1d1d;stroke:var(--o);stroke-width:2}
#hand .crease{stroke:#4a2a10;stroke-width:2;stroke-linecap:round}
#hand .palm{fill:#191919;stroke:var(--o);stroke-width:2}
#hand .sens{fill:var(--o);stroke:#ffd0a8;stroke-width:1.5;transition:r .08s,fill-opacity .08s}
#hand .sens.off{fill:#333;stroke:#555}
#hand .rip{fill:none;stroke:var(--o);stroke-width:2.5;transform-box:fill-box;transform-origin:center;animation:rip 1s ease-out infinite}
@keyframes rip{from{transform:scale(1);opacity:.9}to{transform:scale(2.6);opacity:0}}
#hand text{fill:var(--dim);font-size:13px;font-weight:700;letter-spacing:2px;text-anchor:middle}
.fl{display:grid;gap:7px}
.fr{display:grid;grid-template-columns:62px 1fr 38px;gap:8px;align-items:center;font-size:13px}
.bar{height:9px;border-radius:9px;background:#252525;overflow:hidden}
.bar i{display:block;height:100%;width:0;background:linear-gradient(90deg,#b34700,var(--o),#ffb26b);border-radius:9px;transition:width .1s}
.fr.off{opacity:.35}
.frow{padding:10px 0;border-top:1px solid var(--line)}.frow:first-child{border-top:0;padding-top:0}
.fh{display:flex;gap:10px;align-items:center;margin-bottom:6px}.fh b{flex:1}
.tag{font-size:10px;font-weight:800;background:var(--bad);color:#150403;border-radius:6px;padding:2px 6px;letter-spacing:.5px}
.fb{display:flex;gap:8px;align-items:center;margin-top:8px}
.fold{flex:1;height:7px;border-radius:7px;background:#252525;overflow:hidden}.fold i{display:block;height:100%;width:0;background:#777;transition:width .15s}
input[type=range]{width:100%;accent-color:var(--o);height:26px}
.seg{display:inline-flex;border:1px solid var(--line);border-radius:12px;overflow:hidden}
.seg button{background:var(--card2);color:var(--dim);border:0;padding:8px 13px;font:inherit;font-size:13px;font-weight:600;cursor:pointer}
.seg button.on{background:var(--o);color:#111}
.kv{display:flex;justify-content:space-between;gap:10px;padding:8px 0;border-top:1px solid var(--line);font-size:14px}.kv:first-of-type{border-top:0}
.kv span:first-child{color:var(--dim)}
.meter{height:12px;border-radius:12px;background:#252525;overflow:hidden;margin:6px 0}.meter i{display:block;height:100%;width:0;background:var(--o);transition:width .3s}
.note{color:var(--dim);font-size:13px;line-height:1.5}
.form{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:10px}
label.f{display:flex;flex-direction:column;gap:4px;font-size:12px;color:var(--dim)}
input[type=number],select{background:#0f0f0f;color:var(--tx);border:1px solid var(--line);border-radius:10px;padding:9px 10px;font:inherit;width:100%}
input[type=number]:focus,select:focus{outline:2px solid var(--o);border-color:var(--o)}
table.t{width:100%;border-collapse:collapse;font-size:14px}.t th{color:var(--dim);font-weight:600;text-align:left;font-size:12px;padding:4px 6px}.t td{padding:5px 6px}
.chk{display:flex;flex-wrap:wrap;gap:8px}.chk label{display:flex;gap:6px;align-items:center;background:var(--card2);border:1px solid var(--line);border-radius:10px;padding:7px 10px;font-size:13px}
.chk input{accent-color:var(--o)}
nav{position:fixed;left:0;right:0;bottom:0;z-index:10;display:flex;justify-content:center;background:rgba(14,14,14,.96);backdrop-filter:blur(10px);border-top:1px solid var(--line);padding:6px 8px calc(6px + env(safe-area-inset-bottom))}
nav .in{display:flex;width:100%;max-width:520px;gap:4px}
nav button{flex:1;background:none;border:0;color:var(--dim);display:flex;flex-direction:column;align-items:center;gap:3px;padding:7px 0;border-radius:12px;font:inherit;font-size:11px;font-weight:600;cursor:pointer;letter-spacing:.4px}
nav button svg{width:24px;height:24px}
nav button.on{color:var(--o);background:#1f1208}
#toast{position:fixed;left:50%;bottom:98px;transform:translateX(-50%) translateY(20px);background:#222;border:1px solid var(--o);color:var(--tx);padding:9px 16px;border-radius:12px;opacity:0;pointer-events:none;transition:.25s;z-index:20;font-size:14px;max-width:90vw}
#toast.show{opacity:1;transform:translateX(-50%)}#toast.err{border-color:var(--bad)}
</style>
</head>
<body>
<svg width="0" height="0" style="position:absolute" aria-hidden="true">
<defs>
<symbol id="i-home" viewBox="0 0 24 24"><path d="M3 11l9-8 9 8"/><path d="M5 10v10h5v-6h4v6h5V10"/></symbol>
<symbol id="i-data" viewBox="0 0 24 24"><ellipse cx="12" cy="5" rx="8" ry="3"/><path d="M4 5v6c0 1.7 3.6 3 8 3s8-1.3 8-3V5"/><path d="M4 11v6c0 1.7 3.6 3 8 3s8-1.3 8-3v-6"/></symbol>
<symbol id="i-set" viewBox="0 0 24 24"><path d="M4 6h8M18 6h2M4 12h2M12 12h8M4 18h10M20 18h0"/><circle cx="15" cy="6" r="2.2"/><circle cx="9" cy="12" r="2.2"/><circle cx="17" cy="18" r="2.2"/></symbol>
<symbol id="i-sys" viewBox="0 0 24 24"><rect x="6" y="6" width="12" height="12" rx="2"/><path d="M9 2v4M15 2v4M9 18v4M15 18v4M2 9h4M2 15h4M18 9h4M18 15h4"/></symbol>
<symbol id="i-hand" viewBox="0 0 24 24"><path d="M8 13V5.5a1.5 1.5 0 013 0V11M11 11V3.5a1.5 1.5 0 013 0V11M14 11V5.5a1.5 1.5 0 013 0V13M17 9.5a1.5 1.5 0 013 0V14a7 7 0 01-7 7h-1a7 7 0 01-5.6-2.8L4 15.5a1.5 1.5 0 012.3-2L8 15"/></symbol>
<symbol id="i-pulse" viewBox="0 0 24 24"><path d="M3 12h4l2-6 4 12 2-6h6"/></symbol>
<symbol id="i-power" viewBox="0 0 24 24"><path d="M12 3v9"/><path d="M6.3 6.3a8 8 0 1011.4 0"/></symbol>
<symbol id="i-rec" viewBox="0 0 24 24"><circle cx="12" cy="12" r="8"/><circle cx="12" cy="12" r="3" fill="currentColor"/></symbol>
<symbol id="i-stop" viewBox="0 0 24 24"><path d="M8 3h8l5 5v8l-5 5H8l-5-5V8z"/><path d="M9 9l6 6M15 9l-6 6"/></symbol>
<symbol id="i-dl" viewBox="0 0 24 24"><path d="M12 3v12m0 0l-4-4m4 4l4-4"/><path d="M4 20h16"/></symbol>
<symbol id="i-trash" viewBox="0 0 24 24"><path d="M4 7h16M10 11v6M14 11v6M6 7l1 13h10l1-13M9 7V4h6v3"/></symbol>
<symbol id="i-flag" viewBox="0 0 24 24"><path d="M5 21V4m0 0h12l-2 4 2 4H5"/></symbol>
<symbol id="i-open" viewBox="0 0 24 24"><path d="M4 12h16M8 8l-4 4 4 4M16 8l4 4-4 4"/></symbol>
<symbol id="i-close" viewBox="0 0 24 24"><path d="M3 12h6M21 12h-6M6 9l3 3-3 3M18 9l-3 3 3 3"/></symbol>
<symbol id="i-hand2" viewBox="0 0 24 24"><path d="M5 20v-8l-1-1V8l2 1V4h2v5h1V3h2v6h1V4h2v6h1V7h2v9c0 3-2 5-5 5H9c-2 0-4-1-4-1z"/></symbol>
<symbol id="i-save" viewBox="0 0 24 24"><path d="M5 12l5 5 9-10"/></symbol>
<symbol id="i-reset" viewBox="0 0 24 24"><path d="M4 12a8 8 0 112.5 5.8M4 19v-5h5"/></symbol>
<symbol id="i-clock" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/></symbol>
<symbol id="i-wifi" viewBox="0 0 24 24"><path d="M2 9a15 15 0 0120 0M5 12.5a10.5 10.5 0 0114 0M8.5 16a5.5 5.5 0 017 0"/><circle cx="12" cy="19.5" r="1" fill="currentColor"/></symbol>
<symbol id="i-target" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><circle cx="12" cy="12" r="4"/><path d="M12 1v4M12 19v4M1 12h4M19 12h4"/></symbol>
<symbol id="i-gauge" viewBox="0 0 24 24"><path d="M4 18a9 9 0 1116 0"/><path d="M12 18l4-6"/></symbol>
</defs>
</svg>

<header>
  <div class="brand"><svg class="ic lg"><use href="#i-hand"/></svg><div><b>MEPA</b> Hand<small>Adaptive myoelectric</small></div></div>
  <div class="chips">
    <span class="chip" id="chNet"><i></i><span>Offline</span></span>
    <span class="chip rec" id="chRec"><i></i><span>REC</span></span>
    <span class="chip" id="chTime"><svg class="ic" style="width:14px;height:14px"><use href="#i-clock"/></svg><span>No time</span></span>
  </div>
</header>

<main>
<!-- ============ HOME ============ -->
<section class="page active" id="p-home">
  <div class="grid">
    <div class="card" style="grid-column:1/-1">
      <div class="ch"><h2><svg class="ic"><use href="#i-pulse"/></svg>EMG signal</h2><span class="badge" id="emgBadge">OFF</span></div>
      <div class="row">
        <button class="btn big" id="btnEmg"><svg class="ic"><use href="#i-power"/></svg><span>Activate EMG</span></button>
        <label class="sw"><input type="checkbox" id="swCtl" disabled><span class="knob"></span><em>EMG controls hand</em></label>
      </div>
      <canvas id="cvEmg" class="chart"></canvas>
      <div class="legend">
        <span><i style="background:#6b6b6b"></i>RMS</span>
        <span><i style="background:#FF6A00"></i>EKF filtered</span>
        <span><i style="background:#ff5a4a"></i>Close threshold</span>
        <span><i style="background:#35d07f"></i>Open threshold</span>
      </div>
      <div class="stats">
        <div class="stat"><small>RMS</small><b class="mono" id="stRms">0</b> <small style="display:inline">mV</small></div>
        <div class="stat"><small>Activation</small><b class="mono" id="stAct">0</b> <small style="display:inline">%</small></div>
        <div class="stat"><small>Hand state</small><b id="stState">OPEN</b></div>
      </div>
      <div class="row" style="margin-top:10px">
        <button class="btn sm" data-cal="rest"><svg class="ic"><use href="#i-target"/></svg>Calibrate rest</button>
        <button class="btn sm" data-cal="flex"><svg class="ic"><use href="#i-target"/></svg>Calibrate flex</button>
        <span class="note" id="calTxt1"></span>
      </div>
    </div>

    <div class="card">
      <div class="ch"><h2><svg class="ic"><use href="#i-hand2"/></svg>Hand and force</h2><span class="badge" id="handBadge">OPEN</span></div>
      <div class="hand-wrap">
        <svg id="hand" viewBox="0 0 280 330" role="img" aria-label="Hand with fingertip pressure sensors"></svg>
        <div class="fl" id="fsrList"></div>
      </div>
    </div>

    <div class="card">
      <div class="ch"><h2><svg class="ic"><use href="#i-open"/></svg>Fingers</h2>
        <button class="btn sm danger fill" id="btnStop"><svg class="ic"><use href="#i-stop"/></svg>E-STOP</button></div>
      <div id="fingers"></div>
      <div class="row" style="margin-top:12px">
        <button class="btn pri" data-all="open"><svg class="ic"><use href="#i-open"/></svg>Open all</button>
        <button class="btn pri" data-all="closed"><svg class="ic"><use href="#i-close"/></svg>Close all</button>
      </div>
      <div class="row" style="margin-top:12px"><span class="note">Speed</span>
        <div class="seg" id="speedSeg"><button data-ms="30">Slow</button><button data-ms="15">Medium</button><button data-ms="5">Fast</button></div></div>
    </div>

    <div class="card">
      <div class="ch"><h2><svg class="ic"><use href="#i-gauge"/></svg>Force graph</h2><span class="note">last 15 s, % of full scale</span></div>
      <canvas id="cvFsr" class="chart"></canvas>
      <div class="legend" id="fsrLegend"></div>
    </div>

    <div class="card">
      <div class="ch"><h2><svg class="ic"><use href="#i-rec"/></svg>Recording</h2><span class="badge" id="recBadge">STOPPED</span></div>
      <div class="row">
        <button class="btn big" id="btnRec"><svg class="ic"><use href="#i-rec"/></svg><span>Start recording</span></button>
        <button class="btn" id="btnMark"><svg class="ic"><use href="#i-flag"/></svg>Marker</button>
      </div>
      <div class="meter"><i id="homeMeter"></i></div>
      <div class="note" id="homeStore">Storage</div>
    </div>
  </div>
</section>

<!-- ============ DATA ============ -->
<section class="page" id="p-data">
  <div class="card">
    <div class="ch"><h2><svg class="ic"><use href="#i-data"/></svg>Stored data</h2><span class="badge" id="dataBadge">0 KB</span></div>
    <div class="meter"><i id="dataMeter"></i></div>
    <div class="kv"><span>Log size</span><span id="dKb">0</span></div>
    <div class="kv"><span>Limit before oldest data is replaced</span><span id="dMax">0</span></div>
    <div class="kv"><span>Flash free</span><span id="dFree">0</span></div>
    <div class="kv"><span>Files</span><span id="dSegs">0</span></div>
    <div class="kv"><span>Estimated time stored</span><span id="dTime">0</span></div>
    <div class="kv"><span>Clock</span><span id="dClock">Not synced</span></div>
    <div class="row" style="margin-top:12px">
      <a class="btn pri big" href="/export.csv" download="mepa_log.csv" style="text-decoration:none"><svg class="ic"><use href="#i-dl"/></svg>Export CSV (Excel)</a>
      <button class="btn danger" id="btnClear"><svg class="ic"><use href="#i-trash"/></svg>Clear data</button>
    </div>
    <p class="note">Each row has a timestamp, EMG RMS and activation, hand state, the three servo angles, the six force values and an event column (markers, grip limit, EMG open or close, calibration, emergency stop). When storage is full the oldest data is deleted automatically and the newest data is kept.</p>
  </div>
  <div class="card">
    <div class="ch"><h2><svg class="ic"><use href="#i-set"/></svg>Logging options</h2></div>
    <div class="form">
      <label class="f">Log rate
        <select id="lhz"><option value="1">1 per second</option><option value="2">2 per second</option><option value="5">5 per second</option><option value="10">10 per second</option><option value="20">20 per second</option></select></label>
      <label class="f">Storage limit (KB, 0 = automatic)<input type="number" id="lmax" min="0" max="16000" step="10"></label>
    </div>
    <div class="row" style="margin-top:12px"><button class="btn pri" data-save="1"><svg class="ic"><use href="#i-save"/></svg>Save</button></div>
  </div>
</section>

<!-- ============ SETTINGS ============ -->
<section class="page" id="p-set">
  <div class="card">
    <div class="ch"><h2><svg class="ic"><use href="#i-open"/></svg>Servo positions</h2></div>
    <table class="t"><thead><tr><th>Finger</th><th>Start</th><th>Open</th><th>Closed</th></tr></thead><tbody id="srvTable"></tbody></table>
    <p class="note">Angles are 0 to 180 degrees. Start is where the finger goes when the hand is powered on. Sliders on the Home page stay between Open and Closed.</p>
    <h3>Speed</h3>
    <div class="row"><input type="range" id="ms" min="1" max="60" style="flex:1"><b class="mono" id="msVal">15</b><span class="note">ms per degree</span></div>
    <div class="row" style="margin-top:12px">
      <button class="btn pri" data-save="1"><svg class="ic"><use href="#i-save"/></svg>Save</button>
      <button class="btn danger" id="btnReset"><svg class="ic"><use href="#i-reset"/></svg>Reset to defaults</button>
    </div>
  </div>

  <div class="card">
    <div class="ch"><h2><svg class="ic"><use href="#i-pulse"/></svg>EMG calibration</h2><span class="badge" id="calBadge">idle</span></div>
    <div class="kv"><span>Rest level</span><span class="mono" id="vRest">-</span></div>
    <div class="kv"><span>Flex level</span><span class="mono" id="vFlex">-</span></div>
    <div class="meter"><i id="calBar"></i></div>
    <div class="row">
      <button class="btn" data-cal="rest"><svg class="ic"><use href="#i-target"/></svg>Calibrate rest</button>
      <button class="btn" data-cal="flex"><svg class="ic"><use href="#i-target"/></svg>Calibrate flex</button>
    </div>
    <p class="note" id="calTxt2">Activate EMG first. For rest, relax the muscle. For flex, squeeze firmly and hold. Each step takes 4 seconds.</p>
    <h3>Decision</h3>
    <div class="form">
      <label class="f">Close above (%)<input type="number" id="cth" min="10" max="100"></label>
      <label class="f">Open below (%)<input type="number" id="oth" min="0" max="95"></label>
      <label class="f">Close hold (ms)<input type="number" id="chold" min="0" max="2000" step="10"></label>
      <label class="f">Open hold (ms)<input type="number" id="ohold" min="0" max="2000" step="10"></label>
      <label class="f">Minimum state time (ms)<input type="number" id="minst" min="0" max="5000" step="50"></label>
    </div>
    <div class="row" style="margin-top:12px"><button class="btn pri" data-save="1"><svg class="ic"><use href="#i-save"/></svg>Save</button></div>
  </div>

  <div class="card">
    <div class="ch"><h2><svg class="ic"><use href="#i-gauge"/></svg>Force sensors</h2><span class="badge" id="fcalBadge">idle</span></div>
    <div class="note" style="margin-bottom:8px">Connected sensors</div>
    <div class="chk" id="fsrChk"></div>
    <div class="row" style="margin-top:12px">
      <button class="btn" data-cal="fsrtare"><svg class="ic"><use href="#i-target"/></svg>Zero (no touch)</button>
      <button class="btn" data-cal="fsrmax"><svg class="ic"><use href="#i-target"/></svg>Capture maximum</button>
    </div>
    <p class="note">Zero: keep every sensor untouched for 2 seconds. Maximum: press each sensor as hard as you want to call 100 percent during the 4 second capture.</p>
    <table class="t"><thead><tr><th>Sensor</th><th>Zero mV</th><th>Full mV</th></tr></thead><tbody id="fsrCalTable"></tbody></table>
    <h3>Grip force limit</h3>
    <div class="row">
      <label class="sw"><input type="checkbox" id="gon"><span class="knob"></span><em>Stop closing at limit</em></label>
      <label class="f" style="width:130px">Limit (%)<input type="number" id="glim" min="5" max="100"></label>
    </div>
    <div class="row" style="margin-top:12px"><button class="btn pri" data-save="1"><svg class="ic"><use href="#i-save"/></svg>Save</button></div>
  </div>
</section>

<!-- ============ SYSTEM ============ -->
<section class="page" id="p-sys">
  <div class="card">
    <div class="ch"><h2><svg class="ic"><use href="#i-sys"/></svg>System</h2></div>
    <div class="kv"><span>Firmware</span><span id="sFw">-</span></div>
    <div class="kv"><span>Network</span><span id="sNet">-</span></div>
    <div class="kv"><span>Address</span><span id="sIp">-</span></div>
    <div class="kv"><span>Signal</span><span id="sRssi">-</span></div>
    <div class="kv"><span>Free memory</span><span id="sHeap">-</span></div>
    <div class="kv"><span>Storage size</span><span id="sFs">-</span></div>
    <div class="kv"><span>Uptime</span><span id="sUp">-</span></div>
    <div class="row" style="margin-top:12px">
      <button class="btn" id="btnTime"><svg class="ic"><use href="#i-clock"/></svg>Sync clock from this device</button>
      <button class="btn danger" id="btnReboot"><svg class="ic"><use href="#i-power"/></svg>Restart</button>
    </div>
  </div>
  <div class="card">
    <div class="ch"><h2><svg class="ic"><use href="#i-wifi"/></svg>Good to know</h2></div>
    <p class="note">Over the air update: in Arduino IDE choose the port named MEPA-Hand and upload as usual.</p>
    <p class="note">If the router is not found at power on, the hand creates its own WiFi called MEPA-Hand. Join it and open 192.168.4.1.</p>
    <p class="note">The clock is set from this device each time the page opens, so timestamps work without internet.</p>
    <p class="note">E-STOP freezes all fingers where they are and turns EMG control off.</p>
  </div>
</section>
</main>

<nav><div class="in">
  <button class="on" data-p="home"><svg class="ic"><use href="#i-home"/></svg>Home</button>
  <button data-p="data"><svg class="ic"><use href="#i-data"/></svg>Data</button>
  <button data-p="set"><svg class="ic"><use href="#i-set"/></svg>Settings</button>
  <button data-p="sys"><svg class="ic"><use href="#i-sys"/></svg>System</button>
</div></nav>
<div id="toast"></div>

<script>
const $=s=>document.querySelector(s),$$=s=>[...document.querySelectorAll(s)];
const FN=['Thumb','Index + Middle','Ring + Little'];
const SN=['Thumb','Index','Middle','Ring','Little'];
const FOLDOF=[0,1,1,2,2];
let cfg=null,st=null,fails=0,lastSeq=0,drag=-1,lastSend=0,curPage='home';
const emgR=[],emgK=[],EN=600;
const fsrH=[[],[],[],[],[]],FN2=150;
const FCOL=['#FF6A00','#ffc08a','#ffffff','#b34700','#ffe3cc','#8a8a8a'];

async function api(p){try{const r=await fetch(p,{cache:'no-store'});return await r.json()}catch(e){return null}}
let tt;function toast(m,err){const t=$('#toast');t.textContent=m;t.className='show'+(err?' err':'');clearTimeout(tt);tt=setTimeout(()=>t.className='',2200)}
async function cmd(p,msg){const r=await api(p);if(r&&r.ok===0)toast(r.msg,1);else if(msg)toast(msg);return r}
const clamp=(v,a,b)=>Math.min(b,Math.max(a,v));
const fmtDur=s=>{s=Math.round(s);const h=Math.floor(s/3600),m=Math.floor(s%3600/60);return h?h+' h '+m+' min':m?m+' min '+(s%60)+' s':s+' s'};

/* ---------- navigation ---------- */
$$('nav button').forEach(b=>b.onclick=()=>{curPage=b.dataset.p;$$('nav button').forEach(x=>x.classList.toggle('on',x===b));$$('.page').forEach(p=>p.classList.toggle('active',p.id==='p-'+curPage));window.scrollTo(0,0);if(curPage==='sys')loadSys();draw()});

/* ---------- hand SVG ---------- */
const NS='http://www.w3.org/2000/svg';
function el(t,a,p){const e=document.createElementNS(NS,t);for(const k in a)e.setAttribute(k,a[k]);if(p)p.appendChild(e);return e}
const hand=$('#hand'),H=[];
(function build(){
  const gF=el('g',{},hand);
  const defs=[{x:null,L:70,k:.32},{x:192,L:84,k:.5},{x:158,L:94,k:.5},{x:124,L:86,k:.5},{x:90,L:68,k:.5}];
  defs.forEach((d,i)=>{
    const g=el('g',{},gF);
    const body=el('rect',{x:-14,width:28,rx:14,class:'fing'},g);
    const c1=el('line',{x1:-9,x2:9,class:'crease'},g),c2=el('line',{x1:-9,x2:9,class:'crease'},g);
    H[i]={g,body,c1,c2,L:d.L,k:d.k,x:d.x};
  });
  el('rect',{x:78,y:172,width:148,height:122,rx:32,class:'palm'},hand);
  el('rect',{x:112,y:288,width:80,height:34,rx:10,class:'palm'},hand);
  // sensors drawn above the palm so they stay visible
  const gS=el('g',{},hand);
  H.forEach((h,i)=>{
    h.rip=el('circle',{r:8,class:'rip'},gS);h.sens=el('circle',{r:8,class:'sens'},gS);
  });
  H.forEach((h,i)=>{h.rip.style.display='none'});
  hand.appendChild(el('text',{x:152,y:22,id:'handTxt'}));
  $('#handTxt').textContent='OPEN';
})();
function pose(i,c){
  const h=H[i],len=h.L*(1-h.k*c);
  h.body.setAttribute('y',-len);h.body.setAttribute('height',len+16);
  h.c1.setAttribute('y1',-len*.38);h.c1.setAttribute('y2',-len*.38);h.c2.setAttribute('y1',-len*.7);h.c2.setAttribute('y2',-len*.7);
  const tx=i===0?`translate(220,262) rotate(${38-64*c})`:`translate(${h.x},176)`;
  h.g.setAttribute('transform',tx);
  // sensor position in hand coordinates
  let sx,sy;
  if(i===0){const a=(38-64*c)*Math.PI/180,d=len-18;sx=220+Math.sin(a)*d;sy=262-Math.cos(a)*d}
  else{sx=h.x;sy=176-len+18}
  for(const n of [h.sens,h.rip]){n.setAttribute('cx',sx);n.setAttribute('cy',sy)}
}
function press(i,f,on){
  const h=H[i],r=7+8*f/100;
  h.sens.setAttribute('r',r);h.sens.style.fillOpacity=.25+.75*f/100;h.sens.classList.toggle('off',!on);
  h.rip.setAttribute('r',r);
  const show=on&&f>4;h.rip.style.display=show?'':'none';
  if(show)h.rip.style.animationDuration=(1.4-1.0*f/100).toFixed(2)+'s';
}
for(let i=0;i<5;i++)pose(i,0);

/* ---------- build static UI ---------- */
$('#fsrList').innerHTML=SN.map((n,i)=>`<div class="fr" id="fr${i}"><span>${n}</span><div class="bar"><i></i></div><b class="mono">0</b></div>`).join('');
$('#fsrLegend').innerHTML=SN.map((n,i)=>`<span><i style="background:${FCOL[i]}"></i>${n}</span>`).join('');
$('#fingers').innerHTML=FN.map((n,i)=>`<div class="frow"><div class="fh"><b>${n}</b><span class="tag" id="hold${i}" hidden>GRIP LIMIT</span><span class="mono" id="ang${i}">--</span></div>
<input type="range" id="sl${i}" min="0" max="180" value="0">
<div class="fb"><button class="btn sm" data-mv="${i}:open"><svg class="ic"><use href="#i-open"/></svg>Open</button><button class="btn sm" data-mv="${i}:closed"><svg class="ic"><use href="#i-close"/></svg>Close</button><div class="fold"><i id="fold${i}"></i></div></div></div>`).join('');
$('#srvTable').innerHTML=FN.map((n,i)=>`<tr><td>${n}</td><td><input type="number" id="st${i}" min="0" max="180"></td><td><input type="number" id="op${i}" min="0" max="180"></td><td><input type="number" id="cl${i}" min="0" max="180"></td></tr>`).join('');
$('#fsrChk').innerHTML=SN.map((n,i)=>`<label><input type="checkbox" id="fm${i}">${n}</label>`).join('');
$('#fsrCalTable').innerHTML=SN.map((n,i)=>`<tr><td>${n}</td><td class="mono" id="fz${i}">-</td><td class="mono" id="ff${i}">-</td></tr>`).join('');

/* ---------- interactions ---------- */
document.addEventListener('click',e=>{
  const b=e.target.closest('button,a');if(!b)return;
  if(b.dataset.mv){const[a,p]=b.dataset.mv.split(':');cmd(`/api/move?ch=${a}&pos=${p}`)}
  else if(b.dataset.all)cmd('/api/all?pos='+b.dataset.all);
  else if(b.dataset.cal){cmd('/api/cal?what='+b.dataset.cal,'Capture started')}
  else if(b.dataset.ms){const ms=+b.dataset.ms;cmd('/api/speed?ms='+ms);cfg&&(cfg.ms=ms);$('#ms').value=ms;$('#msVal').textContent=ms;speedSeg()}
  else if(b.dataset.save)saveSettings();
});
function speedSeg(){$$('#speedSeg button').forEach(b=>b.classList.toggle('on',cfg&&+b.dataset.ms===cfg.ms))}
for(let i=0;i<3;i++){const sl=$('#sl'+i);
  sl.addEventListener('input',()=>{drag=i;const n=Date.now();if(n-lastSend>80){lastSend=n;api(`/api/move?ch=${i}&pos=${sl.value}`)}});
  sl.addEventListener('change',()=>{api(`/api/move?ch=${i}&pos=${sl.value}`);setTimeout(()=>drag=-1,500)});
}
$('#btnEmg').onclick=async()=>{const on=!(st&&st.emg.on);await cmd('/api/emg?on='+(on?1:0),on?'EMG active':'EMG off')};
$('#swCtl').onchange=async e=>{const r=await cmd('/api/control?on='+(e.target.checked?1:0));if(r&&r.ok===0)e.target.checked=false};
$('#btnStop').onclick=()=>cmd('/api/estop','Stopped');
$('#btnRec').onclick=()=>cmd('/api/log?on='+(st&&st.log.on?0:1));
$('#btnMark').onclick=()=>cmd('/api/log/marker','Marker added');
$('#btnClear').onclick=async()=>{if(confirm('Delete all stored data?'))await cmd('/api/log/clear','Data cleared')};
$('#btnReset').onclick=async()=>{if(confirm('Reset servo positions and speed to defaults?')){cfg=await api('/api/settings/reset');fillSettings();toast('Defaults restored')}};
$('#btnTime').onclick=async()=>{await syncTime();toast('Clock synced')};
$('#btnReboot').onclick=async()=>{if(confirm('Restart the controller?')){await api('/api/reboot');toast('Restarting')}};
$('#ms').oninput=e=>$('#msVal').textContent=e.target.value;

async function syncTime(){await api('/api/time?ms='+Date.now())}
function fillSettings(){
  if(!cfg)return;
  for(let i=0;i<3;i++){$('#st'+i).value=cfg.start[i];$('#op'+i).value=cfg.open[i];$('#cl'+i).value=cfg.closed[i];
    const sl=$('#sl'+i);sl.min=Math.min(cfg.open[i],cfg.closed[i]);sl.max=Math.max(cfg.open[i],cfg.closed[i])}
  for(const k of['ms','cth','oth','chold','ohold','minst','glim','lhz','lmax'])$('#'+k).value=cfg[k];
  $('#msVal').textContent=cfg.ms;$('#gon').checked=!!cfg.gon;
  for(let i=0;i<5;i++){$('#fm'+i).checked=!!(cfg.mask&(1<<i));$('#fz'+i).textContent=cfg.zero[i];$('#ff'+i).textContent=cfg.full[i]}
  $('#vRest').textContent=cfg.rest+' mV';$('#vFlex').textContent=cfg.flex+' mV';speedSeg();
}
async function loadSettings(){const c=await api('/api/settings');if(c){cfg=c;fillSettings()}}
async function saveSettings(){
  let q=[];
  for(let i=0;i<3;i++)q.push(`st${i}=${$('#st'+i).value}`,`op${i}=${$('#op'+i).value}`,`cl${i}=${$('#cl'+i).value}`);
  for(const k of['ms','cth','oth','chold','ohold','minst','glim','lhz','lmax'])q.push(`${k}=${$('#'+k).value||0}`);
  let m=0;for(let i=0;i<5;i++)if($('#fm'+i).checked)m|=1<<i;
  q.push('mask='+m,'gon='+($('#gon').checked?1:0));
  const c=await api('/api/settings/set?'+q.join('&'));
  if(c){cfg=c;fillSettings();toast('Saved')}else toast('Save failed',1);
}
async function loadSys(){
  const s=await api('/api/sys');if(!s)return;
  $('#sFw').textContent='v'+s.fw;$('#sNet').textContent=s.net+' ('+s.mode+')';$('#sIp').textContent=s.ip+'  or  mepa-hand.local';
  $('#sRssi').textContent=s.rssi?s.rssi+' dBm':'-';$('#sHeap').textContent=s.heap+' KB';$('#sFs').textContent=Math.round(s.fsTotal/1024*10)/10+' MB';$('#sUp').textContent=fmtDur(s.up);
}

/* ---------- charts ---------- */
function drawChart(cv,series,o){
  const dpr=window.devicePixelRatio||1,w=cv.clientWidth,h=cv.clientHeight;if(!w)return;
  if(cv.width!==Math.round(w*dpr)||cv.height!==Math.round(h*dpr)){cv.width=Math.round(w*dpr);cv.height=Math.round(h*dpr)}
  const g=cv.getContext('2d');g.setTransform(dpr,0,0,dpr,0,0);g.clearRect(0,0,w,h);
  const L=30,Y=v=>h-6-(v-o.min)/(o.max-o.min)*(h-12);
  g.font='10px system-ui';g.lineWidth=1;
  for(const t of o.grid){g.strokeStyle='#262626';g.beginPath();g.moveTo(L,Y(t));g.lineTo(w,Y(t));g.stroke();g.fillStyle='#777';g.fillText(t,3,Y(t)+3)}
  for(const l of(o.lines||[])){g.setLineDash([5,4]);g.strokeStyle=l.c;g.beginPath();g.moveTo(L,Y(l.y));g.lineTo(w,Y(l.y));g.stroke()}
  g.setLineDash([]);g.lineJoin='round';
  for(const s of series){g.strokeStyle=s.c;g.lineWidth=s.w||1.5;g.beginPath();const d=s.d,off=o.n-d.length;
    d.forEach((v,i)=>{const x=L+(off+i)/(o.n-1)*(w-L-2);i?g.lineTo(x,Y(clamp(v,o.min,o.max))):g.moveTo(x,Y(clamp(v,o.min,o.max)))});g.stroke()}
}
function draw(){
  if(curPage!=='home')return;
  drawChart($('#cvEmg'),[{d:emgR,c:'#6b6b6b',w:1.2},{d:emgK,c:'#FF6A00',w:2.2}],{min:-20,max:120,n:EN,grid:[0,50,100],lines:cfg?[{y:cfg.cth,c:'#ff5a4a'},{y:cfg.oth,c:'#35d07f'}]:[]});
  drawChart($('#cvFsr'),fsrH.map((d,i)=>({d,c:FCOL[i],w:1.8})),{min:0,max:100,n:FN2,grid:[0,50,100]});
}

/* ---------- render ---------- */
function render(s){
  const prev=st;st=s;
  const e=s.emg;
  // header chips
  const cn=$('#chNet');cn.classList.add('ok');cn.querySelector('span').textContent='Online';
  $('#chRec').style.display=s.log.on?'inline-flex':'none';
  const ct=$('#chTime');ct.classList.toggle('ok',!!s.log.synced);ct.classList.toggle('warn',!s.log.synced);ct.querySelector('span').textContent=s.log.synced?'Clock set':'No clock';
  // EMG
  $('#btnEmg span').textContent=e.on?'Deactivate EMG':'Activate EMG';$('#btnEmg').classList.toggle('pri',!e.on);$('#btnEmg').classList.toggle('on',!!e.on);
  const eb=$('#emgBadge');eb.textContent=e.on?(e.ctl?'CONTROLLING':'SENSING'):'OFF';eb.classList.toggle('on',!!e.on);
  const sw=$('#swCtl');sw.disabled=!e.on;if(document.activeElement!==sw)sw.checked=!!e.ctl;
  $('#stRms').textContent=e.on?e.rms:'0';$('#stAct').textContent=e.on?e.act:'0';$('#stState').textContent=e.on?(e.closed?'CLOSED':'OPEN'):'-';
  $$('[data-cal^=rest],[data-cal^=flex]').forEach(b=>b.disabled=!e.on);
  const calTxt=e.cal?(e.cal===1?'Relax the muscle':'Squeeze and hold'):(e.calmsg||'');
  $('#calTxt1').textContent=calTxt;$('#calBar').style.width=(e.cal?e.calp*100:0)+'%';
  $('#calBadge').textContent=e.cal?(e.cal===1?'rest':'flex'):'idle';
  if(prev&&prev.emg.cal&&!e.cal){loadSettings()}
  // fsr calibration
  $('#fcalBadge').textContent=s.fcal?(s.fcal===1?'zeroing':'press now'):'idle';
  if(prev&&prev.fcal&&!s.fcal)loadSettings();
  // chart data
  if(s.seq<lastSeq){emgR.length=0;emgK.length=0}
  lastSeq=s.seq;s.r.forEach(v=>emgR.push(v));s.k.forEach(v=>emgK.push(v));
  while(emgR.length>EN)emgR.shift();while(emgK.length>EN)emgK.shift();
  if(!e.on&&s.seq===0){emgR.length=0;emgK.length=0}
  // servos and hand
  let mean=0;
  for(let i=0;i<3;i++){
    const a=s.servo[i];$('#ang'+i).textContent=a+'°';
    $('#hold'+i).hidden=!s.hold[i];
    if(drag!==i)$('#sl'+i).value=a;
    let f=0;if(cfg&&cfg.open[i]!==cfg.closed[i])f=clamp((a-cfg.open[i])/(cfg.closed[i]-cfg.open[i]),0,1);
    $('#fold'+i).style.width=(f*100)+'%';$('#fold'+i).style.background=f>.5?'#FF6A00':'#777';
    s['f'+i]=f;mean+=f;
  }
  for(let i=0;i<5;i++)pose(i,s['f'+FOLDOF[i]]);
  mean/=3;const lbl=mean>.85?'CLOSED':mean<.15?'OPEN':'MOVING';
  $('#handTxt').textContent=lbl;$('#handBadge').textContent=lbl;$('#handBadge').classList.toggle('on',lbl==='CLOSED');
  // fsr
  for(let i=0;i<5;i++){
    const on=cfg?!!(cfg.mask&(1<<i)):true,f=on?s.fsr[i]:0;
    const r=$('#fr'+i);r.classList.toggle('off',!on);r.querySelector('i').style.width=f+'%';r.querySelector('b').textContent=f;
    press(i,f,on);
    fsrH[i].push(f);if(fsrH[i].length>FN2)fsrH[i].shift();
  }
  // recording and storage
  const l=s.log;
  $('#btnRec span').textContent=l.on?'Stop recording':'Start recording';$('#btnRec').classList.toggle('on',!!l.on);
  const rb=$('#recBadge');rb.textContent=l.on?'RECORDING':'STOPPED';rb.classList.toggle('on',!!l.on);
  const pct=l.max?Math.min(100,l.kb/l.max*100):0;
  $('#homeMeter').style.width=pct+'%';$('#dataMeter').style.width=pct+'%';
  $('#homeStore').textContent=l.fs?`${l.kb} KB of ${l.max} KB used, ${l.hz} per second`:'Storage not available';
  $('#dataBadge').textContent=l.kb+' KB';$('#dKb').textContent=l.kb+' KB';$('#dMax').textContent=l.max+' KB';
  $('#dFree').textContent=l.free+' KB';$('#dSegs').textContent=l.segs;
  $('#dTime').textContent=fmtDur(l.kb*1024/26/l.hz)+' recorded, room for '+fmtDur(l.max*1024/26/l.hz);
  $('#dClock').textContent=l.synced?'Synced':'Not synced (uses time since boot)';
  draw();
}

/* ---------- poll loop ---------- */
async function poll(){
  const s=await api('/api/state?since='+lastSeq);
  if(s){fails=0;render(s)}else if(++fails>=3){const c=$('#chNet');c.classList.remove('ok');c.querySelector('span').textContent='Offline'}
  setTimeout(poll,110);
}
window.addEventListener('resize',draw);
(async()=>{await loadSettings();await syncTime();poll();setInterval(syncTime,600000)})();
</script>
</body>
</html>
)MEPA";