// ======================================================
// DASHBOARD - read-only LAN web page (separate tab: Dashboard.ino)
// Reads the Hen House.ino variables. Never writes to them, never reads sensors.
// Runs inside loop() (WebServer is synchronous), so there is no thread race.
// It does allow a door close when the evening closing window is active. Door button will turn golden, for early door closure in foul weather.
// ======================================================
#include <WebServer.h>
#include <esp_timer.h>
#include <esp_random.h>
#include <esp_system.h>
#include "RemoteLog.h"
#include "HumTrend.h"

const bool DASH_SERVE_WHILE_MOVING = false;

static WebServer dashServer(80);
static bool dashServerRunning = false;

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>HIGI Hen House</title>
<style>
*{box-sizing:border-box}
html{-webkit-text-size-adjust:100%}
body{margin:0;padding:12px;min-height:100vh;color:#fff;
  background:radial-gradient(circle at 50% -10%,#1b2840,#0a0e15 65%) fixed;
  font-family:"Segoe UI","Avenir Next",Roboto,system-ui,sans-serif;-webkit-font-smoothing:antialiased}
main{max-width:400px;margin:0 auto}

.card{position:relative;min-width:0;padding:12px;border-radius:18px;border:1.5px solid var(--c);overflow:hidden;
  background:
    linear-gradient(180deg,rgba(255,255,255,.13),rgba(255,255,255,0) 45%,rgba(0,0,0,.22)),
    linear-gradient(145deg,var(--t),#0d121a 80%);
  box-shadow:
    inset 0 1px 0 rgba(255,255,255,.4),
    inset 0 -12px 20px rgba(0,0,0,.45),
    inset 0 0 0 1px rgba(255,255,255,.04),
    0 10px 18px rgba(0,0,0,.6),
    0 2px 0 rgba(0,0,0,.5),
    0 0 16px -6px var(--c)}
.card::after{content:"";position:absolute;left:0;right:0;top:0;height:45%;pointer-events:none;
  background:linear-gradient(rgba(255,255,255,.10),rgba(255,255,255,0));
  border-radius:18px 18px 60% 60%/18px 18px 14px 14px}
.card>*{position:relative;z-index:1}
.ic svg{display:block;width:100%;height:auto;filter:drop-shadow(0 3px 3px rgba(0,0,0,.55))}
.tfill{fill:var(--c)}
.row{display:grid;gap:10px;margin-top:12px}
.r3{grid-template-columns:1fr .68fr 1fr}
.temps .tile{min-height:178px}
.temps .tile:not(.difference){padding:16px 6px}
.temps .difference{justify-content:center;padding:9px 4px}
.temps .difference .lb{font-size:clamp(10px,2.8vw,12px);min-height:auto}
.temps .difference .vl{font-size:clamp(16px,4.5vw,20px)}
.r4{grid-template-columns:repeat(4,1fr);gap:8px}
.r2{grid-template-columns:repeat(2,minmax(0,1fr))}
.r2>.card:nth-child(1){order:2}
.r2>.card:nth-child(2){order:1}
.r4>.card:nth-child(1){order:1}
.r4>.card:nth-child(2){order:2}
.r4>.card:nth-child(3){order:3}
.r4>.card:nth-child(4){order:4}

/* Middle column of the temperature row: stacked Timezone (short) + Difference (taller) */
.temps .midstack{
  display:flex;
  flex-direction:column;
  gap:10px;
  min-height:178px;
}
.temps .midstack>.card{
  flex:0 0 auto;
  min-height:0;
  justify-content:center;
  padding:8px 4px;
}
.temps .midstack>.tz{flex:2 1 0}
.temps .midstack>.difference{flex:3 1 0}
.temps .tz .lb,
.temps .difference .lb{font-size:clamp(10px,2.8vw,12px);min-height:auto}
.temps .tz .vl,
.temps .difference .vl{font-size:clamp(16px,4.5vw,20px)}
.temps .tz .pill{font-size:clamp(10px,2.9vw,12.5px);padding:3px 6px}

.card.head{display:flex;align-items:center;justify-content:center;padding:14px}
.card.head{border-width:1.5px;box-shadow:
    inset 0 0 16px rgba(125,138,168,.55),
    inset 0 1px 0 rgba(255,255,255,.4),
    inset 0 -12px 20px rgba(0,0,0,.25),
    inset 0 0 0 1px rgba(255,255,255,.04),
    0 10px 18px rgba(0,0,0,.6),
    0 2px 0 rgba(0,0,0,.5)}
.logo{flex:0 0 auto;width:clamp(36px,11vw,50px)}
.logo svg{display:block;width:100%;height:auto;filter:drop-shadow(0 2px 3px rgba(0,0,0,.6))}
.titlebox{flex:1 1 auto;min-width:0;text-align:center}
.title{font-size:clamp(18px,5.6vw,26px);font-weight:800;font-style:italic;line-height:1.05;letter-spacing:-.01em}
.title .hh{
  background:linear-gradient(180deg,#f7ecc8,#fdf6d8 55%,#fffdf5);
  -webkit-background-clip:text;background-clip:text;
  -webkit-text-fill-color:transparent;color:transparent}
.title b{
  font-weight:900;
  background:linear-gradient(180deg,#ffe98f,#f5c518 50%,#d99a0b);
  -webkit-background-clip:text;background-clip:text;
  -webkit-text-fill-color:transparent;color:transparent;
  filter:drop-shadow(0 1px 0 rgba(255,247,200,.65)) drop-shadow(0 0 8px rgba(245,197,24,.45))}
.title #ver{
  background:linear-gradient(180deg,#f7ecc8,#fff 65%);
  -webkit-background-clip:text;background-clip:text;
  -webkit-text-fill-color:transparent;color:transparent}
.tag{margin-top:6px;font-style:italic;font-size:clamp(9px,2.6vw,11.5px);color:#d3d9e6;line-height:1.25}

.tile{display:flex;flex-direction:column;align-items:center;text-align:center;gap:6px;padding:12px 6px}
.r3 .ic{width:clamp(32px,10.5vw,42px)}
.r2 .ic{width:clamp(34px,9vw,42px)}
.r4 .ic{width:clamp(24px,7.4vw,30px)}
.r4 .tile{padding:10px 4px}
.r2 .tile{padding:13px 8px;gap:7px}

.lb{font-style:italic;color:#e6ebf5;line-height:1.15;display:flex;align-items:center;justify-content:center;min-height:2.3em;
  font-size:clamp(11px,3.3vw,13px)}
.r4 .lb{font-size:clamp(9px,2.7vw,11px)}
.r2 .lb{font-size:clamp(12px,3.2vw,14px)}
.vl{font-weight:800;line-height:1.05;font-variant-numeric:tabular-nums;text-shadow:0 2px 3px rgba(0,0,0,.5)}
.r3 .vl{font-size:clamp(22px,6.8vw,28px)}
.r2 .vl{font-size:clamp(14px,3.8vw,17px)}
.r4 .vl{font-size:clamp(12px,3.6vw,15px)}
.temps .vl{font-size:clamp(20px,6.4vw,28px)}
.unit{font-size:.5em;font-weight:500;margin-left:3px}
.tr{font-size:.95em;font-weight:900;margin-left:4px}
.tr.up{color:#ffb86b}
.tr.down{color:#8fd0ff}
.tr.flat{color:#cfd8e8}

.mm{display:flex;flex-direction:column;gap:2px;font-size:clamp(10px,3vw,12px);font-style:italic;color:#cfd8e8;line-height:1.2}
.mm b{font-style:normal;font-weight:700}
.mm .lo b{color:#8fd0ff}
.mm .hi b{color:#ffb86b}

.schedule{display:flex;justify-content:space-around;gap:8px;width:100%;margin-top:3px;padding-top:7px;border-top:1px solid rgba(255,255,255,.16);font-size:clamp(11px,3vw,13px);font-variant-numeric:tabular-nums}
.schedule div{display:flex;flex-direction:column;gap:3px;min-width:0}
.schedule span{color:#cfd8e8;font-style:italic;font-size:.9em}
.schedule b{color:#fff;font-size:1.1em}
.pill{display:inline-block;max-width:100%;padding:3px 10px;border-radius:999px;font-weight:800;
  font-size:clamp(11px,3.4vw,14px);border:1.5px solid #4a5568;color:#fff;
  background:linear-gradient(#3a4352,#1b212b);
  box-shadow:inset 0 2px 0 rgba(255,255,255,.35),inset 0 -3px 5px rgba(0,0,0,.35),0 2px 4px rgba(0,0,0,.5)}
.r4 .pill{font-size:clamp(9.5px,2.9vw,12px);padding:2px 7px}
.pill.green{background:linear-gradient(#5cf055,#1c9a1b);border-color:#6bf063}
.pill.blue{background:linear-gradient(#63b8ff,#1d62c4);border-color:#8cc9ff}
.pill.amber{background:linear-gradient(#ffcd4a,#ee8400);border-color:#ffd978;color:#2a1a00}
.pill.night{background:linear-gradient(#7a82ee,#363c9a);border-color:#9aa1ff}
.pill.red{background:linear-gradient(#ff6a5e,#b5201a);border-color:#ff9a90}
.pill.on{background:linear-gradient(#ffe45c,#eea000);border-color:#ffe98f;color:#2a1a00}
.pill.cold{background:linear-gradient(#7fd8ff,#2a7fd6);border-color:#b5ecff;
  box-shadow:inset 0 2px 0 rgba(255,255,255,.35),inset 0 -3px 5px rgba(0,0,0,.35),0 2px 4px rgba(0,0,0,.5),0 0 10px rgba(110,200,255,.75)}

/* Log tile + log card (teal) */
.pill.teal{background:linear-gradient(#4fe0d0,#127f78);border-color:#7ff0e3;color:#04201d}
.pill.armed{
  cursor:pointer;
  -webkit-tap-highlight-color:transparent;
  user-select:none;
  padding:6px 16px;
  background:linear-gradient(#ffe45c,#eea000);
  border-color:#ffe98f;
  color:#2a1a00;
  box-shadow:inset 0 2px 0 rgba(255,255,255,.35),inset 0 -3px 5px rgba(0,0,0,.35),0 2px 4px rgba(0,0,0,.5),0 0 10px rgba(255,228,92,.75);
}
#doorPill.sending,#doorPill.closing{
  cursor:default;
  background:linear-gradient(#d6b845,#9c7410);
  border-color:#d6b845;
  color:#211800;
  box-shadow:inset 0 1px 0 rgba(255,255,255,.18),inset 0 -2px 4px rgba(0,0,0,.35);
}
.logtile{cursor:pointer;-webkit-tap-highlight-color:transparent;user-select:none}
.logcard{display:none;margin-top:12px;padding:12px}
.logbar{display:flex;align-items:center;gap:8px}
.logttl{flex:1;min-width:0;font-style:italic;font-weight:800;font-size:clamp(13px,3.6vw,15px)}
.logst{font-style:italic;font-size:clamp(10px,2.8vw,12px);color:#bfeee8}
button.pill{font-family:inherit;font-size:12px;line-height:1.2;padding:3px 10px;cursor:pointer}
.logout{margin:10px 0 0;height:270px;max-height:50vh;overflow:auto;overscroll-behavior:contain;-webkit-overflow-scrolling:touch;
  padding:8px;border-radius:10px;border:1px solid rgba(255,255,255,.12);background:#05090d;color:#cfeee9;
  box-shadow:inset 0 2px 8px rgba(0,0,0,.65);font:11px/1.35 ui-monospace,Consolas,Menlo,monospace;white-space:pre-wrap;word-break:break-word}
.bulb{filter:grayscale(.75) brightness(.65);transition:filter .3s}
.ic svg.bulb.lit{filter:drop-shadow(0 0 7px #ffd23a)}
.glowB{filter:drop-shadow(0 0 5px rgba(60,190,255,.75))}
.glowO{filter:drop-shadow(0 0 6px rgba(255,170,40,.8))}
.b{fill:#27382c}
.b.on{fill:url(#gGreen)}
.offline .wifi{--c:#e0393e!important;--t:#4a1c24!important}
.offline #rssi{color:#ff7b70}

.foot{display:flex;align-items:center;gap:10px;margin-top:18px;color:#b8c0cf;font-style:italic;font-size:clamp(10px,3vw,13px)}
.foot i{flex:1;height:1px;background:#4b5566}
.foot svg{width:18px;flex:0 0 auto}
.hill{position:relative;height:54px;margin-top:4px}
.hill>svg.g{position:absolute;inset:0;width:100%;height:100%}
.hillhen{position:absolute;right:18px;bottom:16px;width:34px;height:auto}
@media (prefers-reduced-motion:reduce){*{transition:none!important}}
</style>
</head>
<body>

<svg width="0" height="0" style="position:absolute" aria-hidden="true"><defs>
  <linearGradient id="gRed" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ff9a8a"/><stop offset="1" stop-color="#d4202c"/></linearGradient>
  <linearGradient id="gShade" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff" stop-opacity=".45"/><stop offset=".5" stop-color="#fff" stop-opacity="0"/><stop offset="1" stop-color="#000" stop-opacity=".3"/></linearGradient>
  <linearGradient id="gMetal" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#f1f4fa"/><stop offset="1" stop-color="#7f8ba0"/></linearGradient>
  <linearGradient id="gDrop" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#8bdcff"/><stop offset="1" stop-color="#1668d8"/></linearGradient>
  <linearGradient id="gWood" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#efbd75"/><stop offset="1" stop-color="#a9651f"/></linearGradient>
  <radialGradient id="rSun" cx=".4" cy=".35" r=".7"><stop offset="0" stop-color="#fff3a8"/><stop offset=".55" stop-color="#ffb52e"/><stop offset="1" stop-color="#ef7a00"/></radialGradient>
  <radialGradient id="rMoon" cx=".35" cy=".3" r=".8"><stop offset="0" stop-color="#ffffff"/><stop offset="1" stop-color="#9aa6e8"/></radialGradient>
  <linearGradient id="gBulb" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff0a0"/><stop offset="1" stop-color="#f2a900"/></linearGradient>
  <linearGradient id="gClock" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#9fd8ff"/><stop offset="1" stop-color="#2a78d6"/></linearGradient>
  <linearGradient id="gGreen" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#8aff84"/><stop offset="1" stop-color="#1fb02a"/></linearGradient>
  <linearGradient id="gDiff" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#cfa9ff"/><stop offset="1" stop-color="#6a3fd0"/></linearGradient>
  <linearGradient id="gHen" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffffff"/><stop offset="1" stop-color="#a9b5cf"/></linearGradient>
  <symbol id="hen" viewBox="0 0 314.655 471.005">
    <g fill="#a8703f" fill-rule="evenodd" transform="translate(314.655,0) scale(-1,1)">
      <path d="m153.29 396.97c4.529 1.138 6.289 5.044 7 10-2.52-3.14-6.08-5.26-7-10z"/>
      <path d="m198.29 399.97c-7.17 8.83-19 13-27 21-1.904-1.874-2.513-0.109-5 0 2.675-3.325 7.535-4.465 11-7-3.009-4.622-9.602 1.717-12 0 1.815-4.954 10.261-4.812 15-4 6.74-2.58 9.9-8.75 18-10z"/>
      <path d="m302.29 399.97c-3.107 6.56-9.771 9.563-15 14 8.973 2.693 21.071 2.262 29 6-3.821 3.24-9.277-0.393-14-1-5.428-0.698-12.327 1.133-15-3-4.045-0.007-6.039 7.243-9 5 7.44-7.54 13.26-16.74 24-21z"/>
      <path d="m226.29 405.97c-3.697 4.303-15.072 0.928-22 2 0.88-6.61 18.16-4.66 22-2z"/>
      <path d="m87.283 409.97c4.378 0.955 13.128-2.462 13 3-4.379-0.95-13.129 2.47-13-3z"/>
      <path d="m60.283 415.97c-3.553 4.78-16.64 0.027-22 3 5.048-6.75 13.624-4.77 22-3z"/>
      <path d="m218.29 416.97c-11.47 0.818-26.8 1.202-40 3 7.852-6.148 22.173-5.827 36-6-0.32 2.66 5.6-0.93 4 3z"/>
      <path d="m94.283 422.97c0.336 1.71-3.188 8.115-8 8-0.63299-5.96 5.829-4.83 8-8z"/>
      <path d="m275.29 422.97c14.961-1.545 26.27 3.336 37 3-4.59 6.743-13.065 9.602-18 16-1.367-5.056 7.392-8.593 10-13-7.518-4.636-20.957-1.576-27-3-2.14 0.47-1.91-1.42-2-3z"/>
      <path d="m260.29 430.97c10.397-3.461 27.155-2.027 38 0-10.734 0.929-21.299-1.352-31 2 6.753 4.914 16.363 6.97 25 10-0.92899 3.781-7.271-2.521-6 2-10.706-2.294-16.97-9.03-28-11-3.8156-10.803-6.5752-7.5009 2-3z"/>
      <path d="m73.283 426.97c-3.676 4.01-10.583 1.34-16 2-3.928 0.479-6.852 2.563-11 2 5.841-3.83 18.619-6.03 27-4z"/>
      <path d="m207.29 427.97h2c0.467 2.134-1.429 1.904-3 2 0.18-0.8 1-0.99 1-2z"/>
      <path d="m166.29 435.97c3.137-4.863 11.007-4.993 19-5 2.475-0.127 1.7 1.225 0 1-7.85-0.17-12.72 2.62-19 4z"/>
      <path d="m191.29 433.97c0.027-0.973 1.224-0.776 2-1 10.054 5.994 24.775 11.719 33 22-12.93-5.74-23.56-13.78-35-21z"/>
      <path d="m143.29 434.97c-3.517 3.881-9.551 1.619-14 2-11.977 1.025-24.052 3.09-36 4 2.567-3.334 10.668-4.294 16-5 10.2-1.35 22.35-0.85 34-1z"/>
      <path d="m153.29 444.97c3.395-6.271 12.509-6.824 22-7 4.709-0.489 2.418 1.802-1 1-7.52 1.49-14.8 3.21-21 6z"/>
      <path d="m33.283 440.97c6.097 1.236 10.548 4.118 14 8-5.084-2.24-12.362-2.3-14-8z"/>
      <path d="m261.29 440.97c-4.073 8.594-14.097 11.237-22 16 2.119-4.948 8.106-6.163 12-9 3.28-2.39 5.43-6.76 10-7z"/>
      <path d="m119.29 444.97c-3.507 5.901-10.486 8.379-16 12-3.371 2.214-6.138 6.342-11 6 6.792-8.2 16.11-13.88 27-18z"/>
      <path d="m166.29 445.97c11.617 5.972 35.255-6.263 42 6-5.228-2.708-18.149-2.62-27-1 0.744 2.923 5.398 1.936 5 6-4.62-5.72-15.77-4.9-20-11z"/>
      <path d="m267.29 451.97c-4.14 5.193-9.408 9.258-15 13 2.88-5.9 9.85-12.63 15-13z"/>
      <path d="m159.29 456.97c-9.61 1.399-22.921 1.521-36 0 3.702-4.556 13.764-2.618 21-2 3.28 0.28 11.25-1.24 15 2z"/>
      <path d="m76.283 419.97c2.594-2.851 3.77 4.173 6 5 5.133 0.067 8.235-5.01 12-7 1.176-0.622 3.793-0.048 5-1 9.076-7.165 13.157-26.807 17-42 1.499-5.927 4.494-11.291 4-17-3.372-4.218-8.925-5.867-13-10-1.605-1.628-2.146-4.646-4-6-2.16-1.578-5.86-1.246-8-3-3.323-2.725-4.364-7.562-8-10-0.635-18.035-16.402-23.332-27-34-1.632-1.643-2.332-4.41-4-6-1.612-1.536-4.284-1.521-6-3-2.053-1.771-2.978-4.972-5-7-11.128-11.16-25.032-22.289-31-40 1.736-3.332-0.504-4.436 0-9-6.009-19.562-2.837-50.009 0-70 2.336-16.458 5.607-30.977 10-45 3.9-1.1 2.226-7.774 5-10-2.251-4.416-5.471-7.862-9-11-0.763-6.571-1.078-13.589-2-20-2.677-5.511-12.113-1.997-16 0-2.327-5.352 1.91-12.738 7-14-2.51-10.54-2.869-22.578 6-26-0.718-4.949-1.373-9.96-1-16 4.118-0.785 5.132 1.535 9 1 3.617-2.383 1.22-10.779 6-12 4.281 0.719 4.833 5.167 8 7 3.564-1.102 2.098-7.236 4-10h3c3.024 1.31 4.181 4.486 4 9 8.933 2.266 2.03-11.304 9-11 5.977 3.222 3.768 8.468 5 16 6.597 0.597 4.768-7.232 9-9 11.602 7.833-3.97 24.349-9 29 16.749 0.999 31.739 12.193 38 27 1.605 3.796 1.78 8.135 3 12 2.927 9.269 6.563 16.521 7 29 0.46801 13.388-1.035 26.105 4 36 3.984 7.83 14.357 15.814 24 17 5.708 0.702 9.495-1.665 15-3 15.078-3.657 32.852-7.733 44-21 6.338-17.83 13.396-36.791 15-58 0.689-9.116-0.809-19.031 2-24 4.807 0.527 6.467 4.2 7 9 4.629-7.704 4.462-20.205 12-25 6.584 0.25 8.466-4.201 14-5 4.519 1.148 6.767 4.567 11 6 1.423 12.397 7.281 14.168 10 25-1.581 2.752-3.601 5.066-5 8 2.733 0.343 9.085-5.251 14-1-4.594 17.209-14.592 33.889-18 49 5.766 2.704 16.501 12.454 18 20 1.001 5.039-2.159 11.187-1 18 0.98701 5.8 7.534 12.141 7 19-6.598 9.572-6.477 20.26-6 33-3.703 0.297-6.568 1.432-6 6h-6c0.921 8.754 0.578 12.47-5 16 0.181 3.152 3.289 3.378 7 3 0.518 7.815 1.974 14.693 5 20-5.365 3.974 1.635 9.227 1 17-3.14 0.493-2.642-3.447-5-1-0.22 5.554 5.94 4.726 7 9 0.127 1.794-1.224 2.109-1 4-2.678 1.623-3.803 2.297-6 0 6.145 6.208-7.208 6.225-3 14-1.926 1.407-4.023 2.643-7 3-1.881 9.785-12.278 11.055-14 21-1.313 1.02-3.687 0.98-5 2-0.974 10.182-6.535 14.39-3 23-3.463-0.129-2.268 4.399-5 5 7.293 19.374 16.583 36.75 27 53 3.8924 6.8032 10.765 10.715 17.056 14.524 2.5266 1.5296 6.5678 4.8218-7.1159 2.9913-7.8439-1.4029-13.829-6.6829-6.3133 5.4035-16.742 10.734-7.167-22.481-19.627 6.0815 5.2 0.482 8.777-4.198 12 0-6.135 4.199-16.477 4.189-25 6 2.454 3.545 6.396 5.604 11 7-5.657 2.081-12.139-4.437-18-7-3.147-1.377-9.491-1.236-10-6 4.374-1.994 9.386 3.004 13 5 5.521-0.146 8.397-2.937 14-3-1.766-4.497 1.256-9.992 1-16-2.582 3.084-4.694 6.639-7 10-5.104-2.283 1.022-5.704 0-10-4.364 1.316-15.85 4.141-20 9-5.524-2.777-5.913 0.041-12 1 9.275-6.392 21.11-10.223 33-14 10.99-10.505-11.254-34.767-17-45-1.41-2.512-1.439-6.061-3-8-2.091 0.576-2.854 2.479-6 2-8.419-7.247-25.678-5.655-31-16-11.909 0.55876-13.118-12.753-16-2h-4c-2.036 7.485-12.454 1.052-17 5 1.308 3.402-2.228 6.979 0 8-4.348 9.406-12.681 28.285-14 43-0.716 7.986 0.316 13.223 5 17 3.976-5.024 11.587-6.412 18-9-2.266 8.734-14.844 7.156-18 15 7.629 1.616 19.62-1.037 30 0-5.354 5.906-23.552 1.595-33 6-4.396-0.604-4.988-5.012-10-5-5.411 1.198-9.954 11.871-16 6 0.443-2.531 4.572-1.305 7-4 1.5975-9.3772-3.936 0.0476-4 0-2.3948-7.6926-0.42496-4.2086-2 0-9.8-2.56-6.865 3.797-11 8-13.915-0.086-26.336-1.664-41-1 7.911-3.756 21.731-1.603 31-4-0.059-1.64-4.225-7.75-6.005-11zm10.005 11c4.812 0.115 8.336-6.29 8-8-2.171 3.17-8.633 2.04-8 8z"/>
      <path d="m166.29 407.97c-7.734 5.266-15.125 10.875-24 15 5.88-7.11 14.61-11.38 24-15z"/>
      <path d="m73.283 410.97c2.565 0.102 1.607 3.727 3 5-0.451 1.66-3.539-2.36-3-5z"/>
      <path d="m77.283 440.97c-1.145 4.19-6.433 6.085-10 8-6.384 3.427-12.623 6.203-21 9 1.571-4.186 8.382-6.762 14-10 5.306-3.05 11.328-8.92 17-7z"/>
      <path d="m86.283 450.97c0.27501 3.509 3.725-2.509 4 1-0.20399 3.797-4.54 3.461-7 5-7.414 4.919-13.383 11.284-21 16-0.248-1.086-1.988-0.679-2-2 1.914-5.087 8.56-5.44 11-10-8.734 0.401-17.291 1.796-27 7 13.247-9.42 28.828-16.505 48-20-0.578 2.43-3.777 2.23-6 3z"/>
    </g>
  </symbol>
</defs></svg>

<main id="app">

<div class="card head" style="--c:#7d8aa8;--t:#1b2436">

  <div class="titlebox">
  <div class="title"><b>HIGI</b> <span class="hh">Hen House</span> <span id="ver"></span></div>
    <div class="tag">Smart Monitoring &bull; Happy Hens &bull; Healthy Flock</div>
  </div>
</div>

<!-- LINE 2: door/light -->
<div class="row r2">

  <div class="card tile" id="cardDoor" style="--c:#c9a93a;--t:#3f3515">
    <div class="ic"><svg viewBox="0 0 64 64">
      <path d="M4 31L32 8l28 23" fill="none" stroke="#d9a35a" stroke-width="5" stroke-linejoin="round" stroke-linecap="round"/>
      <path d="M11 30L32 13l21 17v28H11z" fill="url(#gWood)" stroke="#7a4513" stroke-width="2" stroke-linejoin="round"/>
      <path d="M17 34v24M47 34v24" stroke="#7a4513" stroke-opacity=".45" stroke-width="1.5"/>
      <rect x="22" y="35" width="20" height="23" rx="1.5" fill="#8a4f17" stroke="#5e3209" stroke-width="2"/>
      <path d="M32 35v23" stroke="#5e3209" stroke-width="2"/>
      <circle cx="29" cy="47" r="1.4" fill="#f6d27a"/><circle cx="35" cy="47" r="1.4" fill="#f6d27a"/>
    </svg></div>
    <div class="lb">Door Position</div>
    <span class="pill v" id="doorPill">--</span>
    <div class="schedule">
      <div><span>Open</span><b id="doorOpen">--:--</b></div>
      <div><span>Close</span><b id="doorClose">--:--</b></div>
    </div>
  </div>

  <div class="card tile" id="cardLight" style="--c:#c9a93a;--t:#3f3515">
    <div class="ic"><svg class="bulb" id="bulb" viewBox="0 0 64 64">
      <path d="M32 3a19 19 0 0 0-11 34c2.5 2 4 4.5 4 9h14c0-4.5 1.5-7 4-9A19 19 0 0 0 32 3z" fill="url(#gBulb)" stroke="#fff2b0" stroke-opacity=".7" stroke-width="2"/>
      <path d="M26 22l6 9 6-9" fill="none" stroke="#a86a00" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
      <path d="M32 31v15" stroke="#a86a00" stroke-width="2"/>
      <rect x="24" y="48" width="16" height="4" rx="2" fill="url(#gMetal)"/>
      <rect x="25" y="53" width="14" height="4" rx="2" fill="url(#gMetal)"/>
      <rect x="28" y="58" width="8" height="4" rx="2" fill="url(#gMetal)"/>
      <path d="M20 16a14 14 0 0 1 8-8" stroke="#fff" stroke-opacity=".8" stroke-width="3" fill="none" stroke-linecap="round"/>
    </svg></div>
    <div class="lb">Coop Lighting</div>
    <span class="pill v" id="lightPill">--</span>
    <div class="schedule">
      <div><span>ON</span><b id="lightOn">--:--</b></div>
      <div><span>OFF</span><b id="lightOff">--:--</b></div>
    </div>
  </div>

</div>

<!-- LINE 3: temps -->
<div class="row r3 temps">

  <div class="card tile" id="cardOut" style="--c:#5a6578;--t:#222a38">
    <div class="ic"><svg viewBox="0 0 64 64">
      <rect x="24" y="4" width="16" height="42" rx="8" fill="#1b2230" stroke="url(#gMetal)" stroke-width="3"/>
      <circle cx="32" cy="46" r="13" fill="#1b2230" stroke="url(#gMetal)" stroke-width="3"/>
      <circle class="tfill" cx="32" cy="46" r="9"/>
      <rect class="tfill" x="29" y="18" width="6" height="30" rx="3"/>
      <circle cx="32" cy="46" r="9" fill="url(#gShade)"/>
      <rect x="29" y="18" width="6" height="30" rx="3" fill="url(#gShade)"/>
      <path d="M27 9v20" stroke="#fff" stroke-opacity=".5" stroke-width="2" stroke-linecap="round"/>
      <path d="M44 14h6M44 22h4M44 30h6" stroke="#cfd6e4" stroke-width="2" stroke-linecap="round"/>
    </svg></div>
    <div class="lb">Outside Temperature</div>
    <div class="vl"><span class="v" id="tout">--</span><span class="unit">&deg;C</span><span class="tr" id="toutTr"></span></div>
    <div class="mm">
      <span class="lo">Min <b class="v" id="omin">--</b>&deg;</span>
      <span class="hi">Max <b class="v" id="omax">--</b>&deg;</span>
    </div>
  </div>

  <div class="midstack">

        <div class="card tile tz" style="--c:#2f7de0;--t:#17305a">
      <div class="lb">Cold Delay</div>
      <span class="pill v" id="coldPill">--</span>
    </div>

    <div class="card tile difference" style="--c:#6a72f2;--t:#232a66">
      <div class="lb">Difference</div>
      <div class="vl"><span class="v" id="diff">--</span><span class="unit">&deg;C</span></div>
    </div>

  </div>

  <div class="card tile" id="cardIn" style="--c:#5a6578;--t:#222a38">
    <div class="ic"><svg viewBox="0 0 64 64">
      <rect x="24" y="4" width="16" height="42" rx="8" fill="#1b2230" stroke="url(#gMetal)" stroke-width="3"/>
      <circle cx="32" cy="46" r="13" fill="#1b2230" stroke="url(#gMetal)" stroke-width="3"/>
      <circle class="tfill" cx="32" cy="46" r="9"/>
      <rect class="tfill" x="29" y="18" width="6" height="30" rx="3"/>
      <circle cx="32" cy="46" r="9" fill="url(#gShade)"/>
      <rect x="29" y="18" width="6" height="30" rx="3" fill="url(#gShade)"/>   
      <path d="M27 9v20" stroke="#fff" stroke-opacity=".5" stroke-width="2" stroke-linecap="round"/>
      <path d="M44 14h6M44 22h4M44 30h6" stroke="#cfd6e4" stroke-width="2" stroke-linecap="round"/>
    </svg></div>
    <div class="lb">Inside Temperature</div>
        <div class="vl"><span class="v" id="tin">--</span><span class="unit">&deg;C</span><span class="tr" id="tinTr"></span></div>
    <div class="mm">
      <span class="lo">Min <b class="v" id="imin">--</b>&deg;</span>
      <span class="hi">Max <b class="v" id="imax">--</b>&deg;</span>
    </div>
  </div>

</div>

<!-- LINE 4: outside humidity | log | wifi | inside humidity -->
<div class="row r4">

  <div class="card tile" style="--c:#1fa0d8;--t:#12304d">
    <div class="ic"><svg viewBox="0 0 64 64">
      <path d="M32 4C32 4 12 28 12 41a20 20 0 0 0 40 0C52 28 32 4 32 4z" fill="url(#gDrop)" stroke="#bfe9ff" stroke-opacity=".6" stroke-width="1.5"/>
      <path d="M21 41a11 11 0 0 0 8 11" stroke="#fff" stroke-opacity=".75" stroke-width="3" fill="none" stroke-linecap="round"/>
      <ellipse cx="25" cy="30" rx="2.6" ry="6" fill="#fff" fill-opacity=".35" transform="rotate(25 25 30)"/>
    </svg></div>
    <div class="lb">Outside Humidity</div>
    <div class="vl"><span class="v" id="ohum">--</span><span class="unit">%</span><span class="tr" id="ohumTr"></span></div>
  </div>

  <div class="card tile logtile" id="logTile" role="button" tabindex="0" aria-expanded="false" style="--c:#1fb5a6;--t:#10373a">
    <div class="ic"><svg viewBox="0 0 64 64">
      <rect x="5" y="9" width="54" height="46" rx="7" fill="#0b1618" stroke="#2fd6c4" stroke-width="3"/>
      <path d="M15 24l8 7-8 7" fill="none" stroke="#5ff0df" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>
      <path d="M29 40h18M29 31h12" stroke="#9fe9e0" stroke-width="3" stroke-linecap="round"/>
    </svg></div>
    <div class="lb">Coop Log</div>
    <span class="pill teal" id="logPill">Show</span>
  </div>

  <div class="card tile wifi" style="--c:#1898a8;--t:#0c3542">
    <div class="ic"><svg viewBox="0 0 40 40">
      <rect class="b" id="b1" x="2" y="26" width="7" height="12" rx="1.5"/>
      <rect class="b" id="b2" x="12" y="18" width="7" height="20" rx="1.5"/>
      <rect class="b" id="b3" x="22" y="10" width="7" height="28" rx="1.5"/>
      <rect class="b" id="b4" x="32" y="2" width="7" height="36" rx="1.5"/>
    </svg></div>
    <div class="lb">Wi-Fi Signal</div>
    <div class="vl"><span class="v" id="rssi">--</span><span id="rssiU"> dBm</span></div>
  </div>

  <div class="card tile" style="--c:#1fa0d8;--t:#12304d">
    <div class="ic"><svg viewBox="0 0 64 64">
      <path d="M32 4C32 4 12 28 12 41a20 20 0 0 0 40 0C52 28 32 4 32 4z" fill="url(#gDrop)" stroke="#bfe9ff" stroke-opacity=".6" stroke-width="1.5"/>
      <path d="M21 41a11 11 0 0 0 8 11" stroke="#fff" stroke-opacity=".75" stroke-width="3" fill="none" stroke-linecap="round"/>
      <ellipse cx="25" cy="30" rx="2.6" ry="6" fill="#fff" fill-opacity=".35" transform="rotate(25 25 30)"/>
    </svg></div>
    <div class="lb">Inside Humidity</div>
    <div class="vl"><span class="v" id="hum">--</span><span class="unit">%</span><span class="tr" id="humTr"></span></div>
  </div>

</div>

<!-- LOG CARD (opened by the Coop Log tile) -->
<div class="card logcard" id="logCard" style="--c:#1fb5a6;--t:#10373a">
  <div class="logbar">
    <span class="logttl">Coop Log</span>
    <span class="logst" id="logSt">--</span>
    <button class="pill teal" id="logPause" type="button">Pause</button>
    <button class="pill teal" id="logClear" type="button">Clear</button>
  </div>
  <pre class="logout" id="logOut"></pre>
</div>

<div class="foot">
  <i></i>
  <svg viewBox="0 0 64 64" fill="none" stroke="#b8c0cf" stroke-width="5" stroke-linecap="round" stroke-linejoin="round">
    <path d="M52 34A20 20 0 1 1 42 15"/><path d="M40 4l3 13-13 3"/>
  </svg>
  <span>Real Time Schedule &bull; Refresh every 5 seconds</span>
  <i></i>
</div>

</main>

<script>
const $ = id => document.getElementById(id);
const isNum = v => typeof v === 'number' && isFinite(v);
const f = (v, d) => isNum(v) ? v.toFixed(d) : '--';
const up = s => String(s || '--').toUpperCase();
let doorCloseUiState = '';

function setPill(id, text, cls) {
  const el = $(id);
  const keep = el.classList.contains('armed') ? ' armed' : '';
  el.textContent = text;
  el.className = 'pill v ' + cls + keep;
}

// Humidity trend arrow: server sends "up" / "down" / "steady", or null while it has no usable history
function setTrend(id, t) {
  const m = { up: ['\u2191', 'up', 'Rising'], down: ['\u2193', 'down', 'Falling'], steady: ['\u2192', 'flat', 'Steady'] }[t];
  const el = $(id);
  el.textContent = m ? m[0] : '';
  el.className = 'tr' + (m ? ' ' + m[1] : '');
  if (m) el.setAttribute('aria-label', m[2]); else el.removeAttribute('aria-label');
}

// Temperature card colours: one shared scale for the inside and outside cards.
// Band i is below TEMP_EDGES[i]; the last band is above the final edge. [border colour, tint colour]
const TEMP_EDGES = [0, 5, 10, 20, 25, 30];
const TEMP_HYST = 0.3;  // degrees C a reading must pass an edge by before the band changes (stops flicker)
const TEMP_COLOURS = [
  ['#b7b9ff', '#2a2d5e'],  // below 0   freezing   pale violet-blue
  ['#2f7de0', '#17305a'],  // 0 to 5    cold       blue
  ['#4fd0e8', '#12414f'],  // 5 to 10   cooler     light cyan
  ['#3fcf3a', '#164022'],  // 10 to 20  comfortable green
  ['#e6d23a', '#4a4312'],  // 20 to 25  warmer     yellow
  ['#f08a1c', '#4d2a0c'],  // 25 to 30  hot        orange
  ['#e0393e', '#4a1c24']   // above 30  dangerous  red
];
const TEMP_NONE = ['#5a6578', '#222a38'];  // no reading: neutral grey
const tempBandState = {};

function tempBand(t, prev) {
  if (prev === undefined) {
    let i = 0;
    while (i < TEMP_EDGES.length && t >= TEMP_EDGES[i]) i++;
    return i;
  }
  let i = prev;
  while (i < TEMP_EDGES.length && t >= TEMP_EDGES[i] + TEMP_HYST) i++;  // climb
  while (i > 0 && t < TEMP_EDGES[i - 1] - TEMP_HYST) i--;               // fall
  return i;
}

function setTempCard(id, t) {
  let col = TEMP_NONE;
  if (isNum(t)) {
    tempBandState[id] = tempBand(t, tempBandState[id]);
    col = TEMP_COLOURS[tempBandState[id]];
  } else {
    tempBandState[id] = undefined;
  }
  const el = $(id);
  el.style.setProperty('--c', col[0]);
  el.style.setProperty('--t', col[1]);
}

// Door and Light card colours: [border, tint]
const CARD_AMBER = ['#c9a93a', '#3f3515'];
const CARD_SKY = ['#63b8ff', '#16385a'];
const CARD_RED = ['#e0393e', '#4a1c24'];

function setCardColour(id, col) {
  const el = $(id);
  el.style.setProperty('--c', col[0]);
  el.style.setProperty('--t', col[1]);
}

function blank() {
  document.querySelectorAll('.v').forEach(e => { if (!e.classList.contains('pill')) e.textContent = '--'; });
    ['doorPill', 'lightPill', 'coldPill'].forEach(id => setPill(id, '--', ''));
  for (let i = 1; i <= 4; i++) $('b' + i).classList.remove('on');
  $('bulb').classList.remove('lit');
  setTempCard('cardIn', null);
  setTempCard('cardOut', null);
  setCardColour('cardDoor', TEMP_NONE);
  setCardColour('cardLight', TEMP_NONE);
  setTrend('ohumTr', null);
  setTrend('humTr', null);
  setTrend('toutTr', null);
  setTrend('tinTr', null);
  ['doorOpen', 'doorClose', 'lightOn', 'lightOff'].forEach(id => $(id).textContent = '--:--');
  $('rssi').textContent = 'Offline';
  $('rssiU').style.display = 'none';
}

function render(d) {
  if (d.version) {
    const v = String(d.version);
    const shown = /^v/i.test(v) ? v : 'V' + v;
    $('ver').textContent = shown;
    document.title = 'HIGI Hen House ' + shown;
  }
  $('tin').textContent = f(d.insideTemp, 1);
  $('tout').textContent = f(d.outsideTemp, 1);
  setTempCard('cardIn', d.insideTemp);
  setTempCard('cardOut', d.outsideTemp);
  $('omin').textContent = f(d.outMin, 1);
  $('omax').textContent = f(d.outMax, 1);
  $('imin').textContent = f(d.inMin, 1);
  $('imax').textContent = f(d.inMax, 1);

  if (isNum(d.insideTemp) && isNum(d.outsideTemp)) {
    const x = d.insideTemp - d.outsideTemp;
    const small = Math.abs(x) < 0.05;
    $('diff').textContent = (small ? '' : x > 0 ? '+' : '\u2212') + Math.abs(x).toFixed(1);
  } else {
    $('diff').textContent = '--';
  }

  $('ohum').textContent = f(d.outsideHumidity, 0);
  $('hum').textContent = f(d.humidity, 0);
  setTrend('ohumTr', d.outHumTrend);
  setTrend('humTr', d.humTrend);
  setTrend('toutTr', d.outTempTrend);
  setTrend('tinTr', d.tempTrend);  
  $('doorOpen').textContent = d.doorOpen || '--:--';
  $('doorClose').textContent = d.doorClose || '--:--';
  $('lightOn').textContent = d.lightOn || '--:--';
  $('lightOff').textContent = d.lightOff || '--:--';

  // Cold Delay card: pill text is the setpoint, pill colour is the state (decided on the board)
  const ct = isNum(d.coldTrigger) ? (Math.round(d.coldTrigger * 10) / 10) + '\u00B0C' : '--';
  if (d.coldState === 'override') setPill('coldPill', 'FORCED', 'amber');
  else if (d.coldState === 'off') setPill('coldPill', 'OFF', '');
  else if (d.coldState === 'cold') setPill('coldPill', ct, 'cold');
  else if (d.coldState === 'idle') setPill('coldPill', ct, '');
  else setPill('coldPill', '--', '');

  const door = up(d.door);
  const dp = $('doorPill');

  if (doorCloseUiState === 'sending') {
    setPill('doorPill', 'Sending', 'sending');
    dp.classList.remove('armed');
  } else if (doorCloseUiState === 'closing' && door === 'CLOSING') {
    setPill('doorPill', 'Closing', 'closing');
    dp.classList.remove('armed');
  } else {
    if (doorCloseUiState === 'closing') doorCloseUiState = '';
    setPill('doorPill', door, door === 'CLOSED' ? 'green' : door === 'OPEN' ? 'blue' : door === 'FAULT' ? 'red' : 'amber');
      // Door card: blue when open, amber when closed, red on fault. Any other state keeps the current colour.
  // Door card: amber when open, blue when closed, red on fault. Any other state keeps the current colour.
  if (door === 'OPEN') setCardColour('cardDoor', CARD_AMBER);
  else if (door === 'CLOSED') setCardColour('cardDoor', CARD_SKY);
  else if (door === 'FAULT') setCardColour('cardDoor', CARD_RED);
    if (d.closeAllowed === true) dp.classList.add('armed');
    else dp.classList.remove('armed');
  }

  const lamp = up(d.lighting) === 'ON';
  setPill('lightPill', up(d.lighting), lamp ? 'on' : '');
  $('bulb').classList.toggle('lit', lamp);
  setCardColour('cardLight', lamp ? CARD_AMBER : CARD_SKY);

  $('rssi').textContent = isNum(d.rssi) ? d.rssi : '--';
  $('rssiU').style.display = '';
  const r = d.rssi;
  const lvl = !isNum(r) ? 0 : r > -55 ? 4 : r > -67 ? 3 : r > -75 ? 2 : r > -85 ? 1 : 0;
  for (let i = 1; i <= 4; i++) $('b' + i).classList.toggle('on', i <= lvl);
}

let misses = 0;
async function update() {
  try {
    const res = await fetch('/api/status', { signal: AbortSignal.timeout(4000), cache: 'no-store' });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    render(await res.json());
    misses = 0;
    document.body.classList.remove('offline');
  } catch (e) {
    misses++;
    document.body.classList.add('offline');
    $('rssi').textContent = 'Offline';
    $('rssiU').style.display = 'none';
    if (misses >= 4) blank();
  }
}

// ---- Coop Log ----
const logTile = $('logTile'), logCard = $('logCard'), logOut = $('logOut'), logSt = $('logSt'), logPause = $('logPause');
let logOpen = false, logPaused = false, logPolling = false, logSince = 0, logBoot = 0;

function logAdd(t) {
  const atEnd = logOut.scrollTop + logOut.clientHeight >= logOut.scrollHeight - 24;
  logOut.appendChild(document.createTextNode(t));
  if (logOut.textContent.length > 60000) logOut.textContent = logOut.textContent.slice(-40000);
  if (atEnd) logOut.scrollTop = logOut.scrollHeight;
}

async function logPoll() {
  if (logPolling) return;
  logPolling = true;
  while (logOpen) {
    let wait = 2000;
    if (!logPaused) {
      try {
        const r = await fetch('/api/log?since=' + logSince + '&boot=' + logBoot, { signal: AbortSignal.timeout(6000), cache: 'no-store' });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        const t = await r.text();
        const nb = Number(r.headers.get('X-Log-Boot')), nx = Number(r.headers.get('X-Log-Next')), fr = Number(r.headers.get('X-Log-From'));
        if (logBoot && nb !== logBoot) logAdd('\n--- device restarted ---\n');
        else if (logSince > 0 && fr > logSince) logAdd('\n--- some lines were missed ---\n');
        logBoot = nb; logSince = nx;
        if (t) logAdd(t);
        logSt.textContent = 'Live';
        if (t.length >= 4000) wait = 50;
      } catch (e) {
        logSt.textContent = 'No response';
      }
    }
    await new Promise(res => setTimeout(res, wait));
  }
  logPolling = false;
}

function logToggle() {
  logOpen = !logOpen;
  logCard.style.display = logOpen ? 'block' : 'none';
  $('logPill').textContent = logOpen ? 'Hide' : 'Show';
  logTile.setAttribute('aria-expanded', logOpen ? 'true' : 'false');
  if (logOpen) {
    logSt.textContent = logPaused ? 'Paused' : 'Loading';
    logPoll();
    logCard.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
  }
}
logTile.addEventListener('click', logToggle);
logTile.addEventListener('keydown', e => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); logToggle(); } });
logPause.addEventListener('click', () => {
  logPaused = !logPaused;
  logPause.textContent = logPaused ? 'Resume' : 'Pause';
  logSt.textContent = logPaused ? 'Paused' : 'Live';
});
$('logClear').addEventListener('click', () => { logOut.textContent = ''; });

update();
setInterval(update, 5000);

// Armed door pill -> manual close (bedtime window only; server re-checks)
$('doorPill').addEventListener('click', async () => {
  const dp = $('doorPill');
  if (!dp.classList.contains('armed') || doorCloseUiState) return;

  doorCloseUiState = 'sending';
  setPill('doorPill', 'Sending', 'sending');
  dp.classList.remove('armed');

  try {
    const res = await fetch('/api/door/close', {
      method: 'POST',
      cache: 'no-store',
      signal: AbortSignal.timeout(4000)
    });

    if (res.ok) {
      doorCloseUiState = 'closing';
      setPill('doorPill', 'Closing', 'closing');
    } else {
      // Board rejected: keep the gold glow only if still allowed
      doorCloseUiState = 'unknown';
      setPill('doorPill', 'Check', '');
      dp.classList.remove('armed');
      await update();
    }
  } catch (e) {
    // Network/timeout: do not claim success; refresh status
    doorCloseUiState = 'unknown';
    setPill('doorPill', 'Check', '');
    dp.classList.remove('armed');
    await update();
  }
});
</script>
</body>
</html>
)rawliteral";

// ======================================================
// JSON HELPERS (NaN/invalid becomes null so the page never breaks)
// ======================================================

static void dashNum(String& j, const char* key, float v, int dp) {
  j += '"';
  j += key;
  j += "\":";
  if (isnan(v) || isinf(v)) j += "null";
  else j += String(v, dp);
  j += ',';
}

static void dashStr(String& j, const char* key, const char* v) {
  j += '"';
  j += key;
  j += "\":";
  if (v == nullptr || v[0] == '\0') j += "null";
  else {
    j += '"';
    j += v;
    j += '"';
  }
  j += ',';
}

static float dashMin(float v) {
  return (v >= 99.0f) ? NAN : v;
}
static float dashMax(float v) {
  return (v <= -99.0f) ? NAN : v;
}

static const char* dashDoorText() {
  if (faultState == FAULT_ACTIVE || systemLockout) return "FAULT";
  switch (doorState) {
    case DOOR_OPEN: return "OPEN";
    case DOOR_CLOSED: return "CLOSED";
    case DOOR_OPENING: return "OPENING";
    case DOOR_CLOSING: return "CLOSING";
    case DOOR_STOPPED: return "STOPPED";
    default: return "UNKNOWN";
  }
}

static bool dashDoorMoving() {
  return doorState == DOOR_OPENING || doorState == DOOR_CLOSING;
}

static void dashHHMM(String& j, const char* key, int mins) {
  if (mins < 0) {
    dashStr(j, key, nullptr);
    return;
  }
  char buf[8];
  formatMinutesToHHMM(mins, buf, sizeof(buf));
  dashStr(j, key, buf);
}

// ======================================================
// HUMIDITY TREND (read-only: copies the humidity values, never touches sensors)
// One sample every 5 min, last 7 kept (30 min window). Arrow needs >= 15 min of history.
// ======================================================
const uint32_t HUM_TREND_SAMPLE_MS = 300000UL;
const uint32_t HUM_TREND_MIN_SPAN_MS = 900000UL;
const float HUM_TREND_DEADBAND = 1.0f;  // %RH change across the window before it counts as up/down

static HumTrend humTrendIn, humTrendOut;

// Temperature trend: same sampling as humidity (one sample per sensor reading, last 7 kept)
const float TEMP_TREND_DEADBAND = 0.5f;  // deg C change across the window before it counts as up/down
static HumTrend tempTrendIn, tempTrendOut;

static void humTrendSample(HumTrend& h, float v) {
  if (isnan(v) || isinf(v)) {  // bad/absent reading: drop history so a stale arrow can't linger
    h.n = 0;
    return;
  }
  uint32_t now = millis();
  if (h.n > 0 && (uint32_t)(now - h.t[h.n - 1]) < HUM_TREND_SAMPLE_MS) return;
  if (h.n == HumTrend::N) {
    for (uint8_t i = 1; i < HumTrend::N; i++) {
      h.v[i - 1] = h.v[i];
      h.t[i - 1] = h.t[i];
    }
    h.n--;
  }
  h.v[h.n] = v;
  h.t[h.n] = now;
  h.n++;
}

void dashboardHumiditySample(bool outside, float humidity) {
  if (outside) {
    humTrendSample(humTrendOut, humidity);
  } else {
    humTrendSample(humTrendIn, humidity);
  }
}

void dashboardTempSample(bool outside, float tempC) {
  if (outside) {
    humTrendSample(tempTrendOut, tempC);
  } else {
    humTrendSample(tempTrendIn, tempC);
  }
}

static const char* humTrendText(const HumTrend& h) {
  if (h.n < 2) return nullptr;
  if ((uint32_t)(h.t[h.n - 1] - h.t[0]) < HUM_TREND_MIN_SPAN_MS) return nullptr;
  float d = h.v[h.n - 1] - h.v[0];
  if (d >= HUM_TREND_DEADBAND) return "up";
  if (d <= -HUM_TREND_DEADBAND) return "down";
  return "steady";
}

static const char* tempTrendText(const HumTrend& h) {
  if (h.n < 2) return nullptr;
  if ((uint32_t)(h.t[h.n - 1] - h.t[0]) < HUM_TREND_MIN_SPAN_MS) return nullptr;
  float d = h.v[h.n - 1] - h.v[0];
  if (d >= TEMP_TREND_DEADBAND) return "up";
  if (d <= -TEMP_TREND_DEADBAND) return "down";
  return "steady";
}

static float dashOutHumidity() {
  return (enableOutsideSHT && outsideTempValid()) ? currentHumidityOutside : NAN;
}

// ======================================================
// HANDLERS
// ======================================================

static void dashHandleRoot() {
  dashServer.sendHeader("Cache-Control", "no-store");
  dashServer.send_P(200, "text/html", DASHBOARD_HTML);
}

static void dashHandleStatus() {
  struct tm ti;
  bool haveTime = getTime(ti);

  const char* mode = nullptr;
  int lightOnMin = -1, lightOffMin = -1;
  if (haveTime) {
    int nowMin = ti.tm_hour * 60 + ti.tm_min;
    mode = (nowMin >= openTime && nowMin < closeTime) ? "DAY" : "NIGHT";
    if (actualSunset >= 0) {
      lightOnMin = actualSunset + getSeasonalOffset(lightOnOffsetBST, lightOnOffsetGMT, ti);
      lightOffMin = actualSunset + getSeasonalOffset(lightOffMinutesBST, lightOffMinutesGMT, ti);
    }
  }

  unsigned long upSec = (unsigned long)(esp_timer_get_time() / 1000000ULL);
  char up[32];
  snprintf(up, sizeof(up), "%lud %luh %lum %lus",
           upSec / 86400UL, (upSec % 86400UL) / 3600UL, (upSec % 3600UL) / 60UL, upSec % 60UL);

  String j;
  j.reserve(620);
  j += '{';
  dashNum(j, "insideTemp", currentTemp, 1);
  dashNum(j, "outsideTemp", outsideTempValid() ? currentTempOutside : NAN, 1);
  dashNum(j, "inMin", dashMin(todayMinTemp), 1);
  dashNum(j, "inMax", dashMax(todayMaxTemp), 1);
  dashNum(j, "outMin", dashMin(todayMinTempOutside), 1);
  dashNum(j, "outMax", dashMax(todayMaxTempOutside), 1);
  dashNum(j, "humidity", currentHumidity, 1);
  dashNum(j, "outsideHumidity", dashOutHumidity(), 0);
  dashStr(j, "humTrend", humTrendText(humTrendIn));
  dashStr(j, "tempTrend", tempTrendText(tempTrendIn));
  dashStr(j, "outTempTrend", tempTrendText(tempTrendOut));
  dashStr(j, "outHumTrend", humTrendText(humTrendOut));
  dashStr(j, "door", dashDoorText());
  dashStr(j, "mode", mode);
  dashStr(j, "lighting", lightOn ? "ON" : "OFF");
  dashHHMM(j, "doorOpen", haveTime ? openTime : -1);
  dashHHMM(j, "doorClose", haveTime ? closeTime : -1);
  dashHHMM(j, "lightOn", lightOnMin);
  dashHHMM(j, "lightOff", lightOffMin);
  dashStr(j, "rtc", rtcAvailable ? "RUNNING" : "ERROR");
  dashNum(j, "rssi", (float)WiFi.RSSI(), 0);
  dashStr(j, "version", systemVersion);
  // Cold Delay card state. Priority: sensor off, then override (amber), then cold (blue), else idle.
  // "cold" = the blue LED condition (valid outside reading below the setpoint) OR a door delay pending today.
  const char* coldState;
  if (!enableOutsideSHT) coldState = "off";
  else if (forceOpenDespiteCold) coldState = "override";
  else if (delayedOpenTime >= 0 || (outsideTempValid() && currentTempOutside < outsideTempTriggerC)) coldState = "cold";
  else coldState = "idle";
  dashNum(j, "coldTrigger", outsideTempTriggerC, 1);
  dashStr(j, "coldState", coldState);

  j += "\"bedtimeWarning\":";
  j += (bedtimeWarning ? "true" : "false");
  j += ',';
  j += "\"closeAllowed\":";
  j += (dashboardCloseAllowed() ? "true" : "false");
  j += ',';

  j += "\"uptime\":\"";
  j += up;
  j += "\"}";

  dashServer.sendHeader("Cache-Control", "no-store");
  dashServer.send(200, "application/json", j);
}

// ======================================================
// REMOTE LOG - RAM ring buffer, shown in the dashboard's Coop Log card (class in RemoteLog.h)
// Read-only. Paused with the rest of the dashboard while the door moves, but the buffer keeps recording.
// ======================================================

RemoteLog Log;
static uint32_t logBootId = 0;  // random per boot, lets the page notice a restart

static void dashHandleLogData() {
  if (logBootId == 0) logBootId = esp_random() | 1UL;
  uint32_t since = strtoul(dashServer.arg("since").c_str(), nullptr, 10);
  uint32_t boot = strtoul(dashServer.arg("boot").c_str(), nullptr, 10);
  if (boot != logBootId) since = 0;  // first visit or device restarted: start from the oldest line held

  String body;
  uint32_t from = 0;
  uint32_t next = Log.readFrom(since, body, 4096, from);

  dashServer.sendHeader("Cache-Control", "no-store");
  dashServer.sendHeader("X-Log-Boot", String(logBootId));
  dashServer.sendHeader("X-Log-From", String(from));
  dashServer.sendHeader("X-Log-Next", String(next));
  dashServer.send(200, "text/plain; charset=utf-8", body);
}

static void dashHandleDoorClose() {
  if (!dashboardRequestClose()) {
    dashServer.send(409, "text/plain", "Close not allowed");
    return;
  }
  dashServer.sendHeader("Cache-Control", "no-store");
  dashServer.send(200, "text/plain", "Closing");
}


static const char* logResetName(int r) {  // int, not esp_reset_reason_t (IDE prototype hoisting)
  switch (r) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXT";
    case ESP_RST_SW: return "SW";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    default: return "OTHER";
  }
}

void logBoot(int reason, unsigned prevStage) {  // once, in setup(): why we restarted + where the loop was
  Log.printf("BOOT %s reset=%s(%d) prevLoopStage=%u\n", systemVersion, logResetName(reason), reason, prevStage);
}

// ======================================================
// PUBLIC HOOKS (called from the main sketch)
// ======================================================

void dashboardSetup() {
  dashServer.on("/", HTTP_GET, dashHandleRoot);
  dashServer.on("/api/status", HTTP_GET, dashHandleStatus);
  dashServer.on("/api/log", HTTP_GET, dashHandleLogData);
  dashServer.on("/api/door/close", HTTP_POST, dashHandleDoorClose);
  dashServer.onNotFound([]() {
    dashServer.send(404, "text/plain", "Not found");
  });
}

void dashboardOnWifiConnected() {
  dashServer.close();
  dashServer.begin();
  dashServerRunning = true;
  Log.print("Dashboard: http://");
  Log.println(WiFi.localIP());
}

void dashboardOnWifiLost() {
  if (dashServerRunning) dashServer.close();
  dashServerRunning = false;
}

void dashboardLoop() {
  if (!dashServerRunning) return;
  if (!DASH_SERVE_WHILE_MOVING && dashDoorMoving()) return;
  dashServer.handleClient();
}