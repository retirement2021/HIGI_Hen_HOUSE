// ======================================================
// DASHBOARD - LAN web page (separate tab: Dashboard.ino)
// Reads the CoopCam globals. Registered from setup() by dashboardSetup().
// ======================================================

#include <WebServer.h>

// ---- web dashboard --------------------------------------------------
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Coop Cam %VERSION%</title>
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
    linear-gradient(145deg,var(--t2,var(--t)),#0d121a 80%);
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
.card[hidden]{display:none}
.ic svg{display:block;width:100%;height:auto;filter:drop-shadow(0 3px 3px rgba(0,0,0,.55))}
.row{display:grid;gap:10px;margin-top:12px}
.row[hidden]{display:none}
/* The two top cards carry the schedule rows and the ON/OFF buttons, and the
   pair is levelled against the Hen House page, so give that row a floor. */
.row.bigrow{min-height:184px}
@media (min-width:400px) and (max-width:419px){.row.bigrow{min-height:187.5px}}
@media (min-width:420px){.row.bigrow{min-height:200px}}
.r2{grid-template-columns:repeat(2,minmax(0,1fr))}
/* Bottom row: watch, heater, sun, log. */
.r4{grid-template-columns:repeat(4,minmax(0,1fr));gap:8px}
.r3{grid-template-columns:repeat(3,minmax(0,1fr));gap:8px}
/* When the row falls back to two by two, keep both rows the same height. */
.r4,.r3{grid-auto-rows:1fr}
.tile{display:flex;flex-direction:column;align-items:center;text-align:center;gap:4px;padding:10px 6px}
.r2 .card{justify-content:space-between}
/* The two top cards hold different amounts of schedule data, and the row was
   spreading each card's contents to fill its height, so the title and the
   separator sat at different levels. Pin the top of both cards to the same
   grid: icon, title, separator line, and let only the buttons float to the
   bottom. */
.r2 .tile{justify-content:flex-start}
.r2 .tile .lb{flex:0 0 auto}
.r2 .tile .schedule{margin-top:auto;margin-bottom:0}
.r2 .tile .tile-toggle{margin-top:3px;min-width:96px}
.r2 .ic{width:clamp(28px,7.5vw,36px)}
.r2 .lb{margin-top:1px;font-size:clamp(12px,3.2vw,14px)}
.r4 .card,.r3 .card{justify-content:space-between}
/* The four small cards are short and narrow, and at that size the diagonal
   body tint of .card reads as one flat colour. These get a vertical ramp with
   a lit top edge instead, so the glass look survives at 94 px wide. */
.r4 .card,.r3 .card{
  background:
    linear-gradient(180deg,rgba(255,255,255,.20),rgba(255,255,255,.03) 38%,rgba(0,0,0,.26)),
    linear-gradient(180deg,var(--t2s,var(--t2,var(--t))),#070a10 100%)}
.r4 .card::after,.r3 .card::after{display:none}
.r4 .tile,.r3 .tile{padding:9px 4px}
.r4 .ic,.r3 .ic{width:clamp(20px,5.4vw,24px)}
/* The strike lives inside a full-width label box. The height covers two title
   lines plus a little clearance, so all four strikes share the same length and
   the same height regardless of what each card holds underneath. */
.r4 .lb,.r3 .lb{position:relative;width:100%;padding-bottom:5px;
  font-size:clamp(10px,2.8vw,12px);line-height:1.08;
  min-height:calc(2.2em + 8px)}
.r4 .lb::after,.r3 .lb::after{content:"";position:absolute;left:0;right:0;bottom:0;
  height:1px;background:rgba(255,255,255,.16)}
.r4 .pill,.r3 .pill{padding:3px 8px;font-size:clamp(10px,2.9vw,12px)}
.r4 .schedule,.r3 .schedule{border-top:0;margin-top:2px;padding-top:4px;gap:3px;
  font-size:clamp(10px,2.8vw,12px)}
/* Optical alignment inside the two schedule tiles. Rise is raised further than
   Set so it sits level with the Dew row; Set keeps its current position. */
#heaterCard .schedule{transform:translateY(-3px)}
#sunCard .schedule div:first-child{transform:translateY(-11px)}
#sunCard .schedule div:nth-child(2){transform:translateY(-5px)}
/* Automatic and IR Brightness get a little more room than the top row needs. */
.grow .card{min-height:88px}
/* The watch tile can be hidden at build time; the IR card then takes the
   whole row instead of sitting in the left half. */
.row.grow > .card[hidden] + .card:last-child{grid-column:1 / -1}

.card.head{display:flex;align-items:center;justify-content:center;padding:14px;
  transition:border-color .35s ease,background .35s ease,box-shadow .35s ease}
.titlebox{flex:1 1 auto;min-width:0;text-align:center}
.title{font-size:clamp(18px,5.6vw,26px);font-weight:800;font-style:italic;line-height:1.05;letter-spacing:-.01em}
.title .hh{background:linear-gradient(180deg,#f7ecc8,#fdf6d8 55%,#fffdf5);
  -webkit-background-clip:text;background-clip:text;-webkit-text-fill-color:transparent;color:transparent}
.title b{font-weight:900;background:linear-gradient(180deg,#ffe98f,#f5c518 50%,#d99a0b);
  -webkit-background-clip:text;background-clip:text;-webkit-text-fill-color:transparent;color:transparent;
  filter:drop-shadow(0 1px 0 rgba(255,247,200,.65)) drop-shadow(0 0 8px rgba(245,197,24,.45))}
.title #ver{font-size:.52em;background:linear-gradient(180deg,#f7ecc8,#fff 65%);
  -webkit-background-clip:text;background-clip:text;-webkit-text-fill-color:transparent;color:transparent}
.tag{margin-top:6px;font-style:italic;font-size:clamp(9px,2.6vw,11.5px);color:#d3d9e6;line-height:1.25}

.lb{font-style:italic;color:#e6ebf5;line-height:1.15;display:flex;align-items:center;justify-content:center;
  min-height:2.3em;font-size:clamp(11px,3.3vw,13px)}
.vl{font-weight:800;line-height:1.05;font-variant-numeric:tabular-nums;text-shadow:0 2px 3px rgba(0,0,0,.5);
  font-size:clamp(20px,6vw,26px)}

.schedule{display:flex;justify-content:space-around;gap:8px;width:100%;margin-top:3px;padding-top:7px;
  border-top:1px solid rgba(255,255,255,.16);font-size:clamp(11px,3vw,13px);font-variant-numeric:tabular-nums}
.schedule div{display:flex;flex-direction:column;gap:3px;min-width:0}
.schedule span{color:#cfd8e8;font-style:italic;font-size:.9em}
.schedule b{color:#fff;font-size:1.1em}
.schedule.col{flex-direction:column;gap:4px}
/* Label and value stop just inside the border. Hard against it, the value reads
   as if the card edge were clipping it. */
.schedule.col div{flex-direction:row;align-items:baseline;justify-content:space-between;gap:6px;
  padding:0 5px}
/* The Coop Camera card is nearly twice as wide as the schedule rows in the
   bottom row, so 5 px there is barely a hair. It gets twice the inset. */
#cardCam .schedule.col div,#cardLamp .schedule.col div{padding:0 10px}
.schedule div[hidden]{display:none}

.pill{display:inline-block;max-width:100%;padding:3px 10px;border-radius:999px;font-weight:800;
  font-size:clamp(11px,3.4vw,14px);border:1.5px solid #4a5568;color:#fff;
  background:linear-gradient(#3a4352,#1b212b);
  box-shadow:inset 0 2px 0 rgba(255,255,255,.35),inset 0 -3px 5px rgba(0,0,0,.35),0 2px 4px rgba(0,0,0,.5)}
/* One palette rule: gold marks whatever is currently true, steel means neutral. */
.pill.on{background:linear-gradient(#ffe45c,#eea000);border-color:#ffe98f;color:#2a1a00}

.bulb{filter:grayscale(.75) brightness(.65);transition:filter .3s}
.ic svg.bulb.lit{filter:drop-shadow(0 0 7px #ffd23a)}

a.act{display:flex;align-items:center;justify-content:center;text-align:center;text-decoration:none;
  padding:8px 4px;border-radius:999px;font-weight:800;font-size:clamp(12px,3.4vw,14px);color:#fff;
  border:1.5px solid #4a5568;background:linear-gradient(#3a4352,#1b212b);
  box-shadow:inset 0 2px 0 rgba(255,255,255,.25),inset 0 -3px 5px rgba(0,0,0,.35),0 2px 4px rgba(0,0,0,.45);
  -webkit-tap-highlight-color:transparent;user-select:none;transition:transform .08s ease,filter .12s ease}
a.act:active{transform:translateY(1px);filter:brightness(.85)}
a.act.on{border-color:#ffe98f;background:linear-gradient(#ffe45c,#eea000);color:#2a1a00;
  box-shadow:inset 0 2px 0 rgba(255,255,255,.55),inset 0 -3px 5px rgba(0,0,0,.18),0 0 10px -3px #f5c518}

.auto{flex-direction:column;align-items:stretch;gap:7px;padding:12px 14px;text-align:left;
  text-decoration:none;color:inherit;
  transition:border-color .35s ease,background .35s ease,box-shadow .35s ease}
.duo .hint{margin-top:4px;flex:0 0 auto}
/* The outline icons keep one fixed colour, the way the illustrated camera icon
   does: they mark the function, not the state. State lives in the card border. */
.ic svg.lic{fill:none;stroke:#eef3ff;stroke-width:4;stroke-linecap:round;stroke-linejoin:round}
.hint{margin-top:3px;font-size:clamp(9.5px,2.8vw,11.5px);color:#cfd8e8;font-style:italic;
  line-height:1.25}
.auto .pill{flex:0 0 auto}
a.auto:active{filter:brightness(.9)}
a.card.tile{text-decoration:none;color:inherit}
/* View Camera and IR Brightness are the same card in two roles: title line
   with the pill on the right, one hint line underneath. One rule set drives
   both so they cannot drift apart again. */
.card.duo{display:flex;flex-direction:column;align-items:stretch;justify-content:flex-start;
  padding:10px 11px;gap:0;text-align:left;min-width:0}
.duo .lb{flex:0 0 auto;justify-content:flex-start;min-height:0;
  font-size:clamp(12px,3.4vw,14px);font-weight:400}
/* The pill sits on the floor of the card, like every other button on the page,
   instead of riding at the top of it. */
.duo > .pill{margin-top:auto;align-self:flex-end}
/* View Camera: the countdown pill sits on the left, the state pill is pinned
   right by margin-left:auto so it stays there even when the count is hidden.
   The state pill fades to a ghost while the camera boots, then ramps back up
   as VIEW when the countdown ends. */
.viewbar{display:flex;align-items:flex-end;gap:8px;margin-top:auto;width:100%;padding:0 6px}
.viewbar > .pill{margin-top:0}
#viewCount[hidden]{display:none}
#viewPill{margin-left:auto;transition:opacity .45s ease,filter .45s ease}
#viewPill.dim{opacity:.15;filter:grayscale(.8)}

.card.auto{cursor:pointer}
a.auto:focus-visible{outline:2px solid var(--c);outline-offset:3px}

/* Only the IR panel is unique to this card now. */
.duo .ir-panel{margin-top:2px}
button.pill{font-family:inherit;cursor:pointer;-webkit-tap-highlight-color:transparent}
button.pill:disabled{opacity:.38;cursor:not-allowed;filter:grayscale(.7)}
.ir-toggle{flex:0 0 auto;margin-right:6px}
.ir-panel{width:100%;padding-top:7px;border-top:1px solid rgba(255,255,255,.16)}
.ir-panel[hidden]{display:none}
.ir-current{text-align:center;font-size:clamp(19px,5vw,24px);font-weight:900;
  font-variant-numeric:tabular-nums;text-shadow:0 2px 3px rgba(0,0,0,.5);margin-bottom:8px}
.ir-buttons{display:grid;grid-template-columns:1fr 1fr;gap:6px}
.ir-step{width:100%;padding:7px 4px}
.ir-note{text-align:center;margin-top:6px;font-size:10px;color:#cfd8e8;font-style:italic}

/* View Camera and serial log: tiles in the bottom row. The log expands into a
   full-width panel; View Camera opens the stream in a new tab. */
#viewCard,#logTile{cursor:pointer;-webkit-tap-highlight-color:transparent;user-select:none}
#viewCard:focus-visible,#logTile:focus-visible{outline:2px solid var(--c);outline-offset:3px}
.logcard{display:flex;flex-direction:column;gap:8px;padding:12px;margin-top:12px}
.logcard[hidden]{display:none}
.logbar{display:flex;align-items:center;gap:8px;min-width:0}
.logttl{flex:1 1 auto;min-width:0;font-style:italic;font-weight:800;
  font-size:clamp(13px,3.6vw,15px);line-height:1.1}
.logst{flex:0 0 auto;font-style:italic;font-size:clamp(10px,2.8vw,12px);color:#cfd8e8}
button.logbtn{font-family:inherit;font-size:clamp(11px,3vw,12px);line-height:1.2;padding:4px 9px;cursor:pointer}
.logout{margin:0;height:270px;max-height:50vh;overflow:auto;overscroll-behavior:contain;
  -webkit-overflow-scrolling:touch;padding:8px;border-radius:10px;border:1px solid rgba(255,255,255,.12);
  background:#05090d;color:#cfeee9;box-shadow:inset 0 2px 8px rgba(0,0,0,.65);
  /* One log line stays one line: long lines scroll sideways instead of
     wrapping, so the boot summary keeps its columns. */
  font:11px/1.35 ui-monospace,Consolas,Menlo,monospace;white-space:pre;word-break:normal}
/* Time card. It is built from the same blocks as the Sunrise / Sunset card
   beside it: the clock takes the title band, and the date and zone are rows of
   the same schedule block. The divider height and the row baselines then come
   from the same rules as next door instead of from hand-tuned margins. */
.clk{font-style:normal;font-size:clamp(14px,4vw,16px);font-weight:900;
  font-variant-numeric:tabular-nums;line-height:1.05;text-shadow:0 2px 3px rgba(0,0,0,.5)}
.clkrows div{justify-content:center}
/* .schedule.col div sets space-between, so the date and zone need to be named
   more specifically than that to sit centred. gap must go too: the invisible
   spacer below would otherwise count as a second item and pull the text left. */
.schedule.col.clkrows div{justify-content:center;gap:0}
/* The Rise / Set rows next door carry a 1.1em value. A zero-width glyph at the
   same size gives these rows the same ascent, so their baselines line up. */
.clkrows div::after{content:"\200B";font-size:1.1em}
.clkrows span{white-space:nowrap}
/* The zone is plain text, not a pill: a pill on this card read as a button. */
.clkzone{letter-spacing:.09em}

/* Coop High: one thin full-width strip for the ceiling air. The label sits on
   two lines at the left behind a hairline rule, and the two readings take the
   right-hand side. Keeping the values on one line each is what keeps the card
   at 42 px, so the whole page still fits a 375 px phone. */
/* Ceiling air: two cards, one per reading. Each carries its own banded border
   and tint, so temperature and moisture can never be mistaken for each other. */
.card.envc{display:flex;flex-direction:column;align-items:center;justify-content:center;
  gap:3px;padding:8px 9px;text-align:center}
.envc .l1{display:flex;align-items:baseline;justify-content:center;gap:4px;white-space:nowrap;
  font-size:10px;line-height:1;font-style:italic;color:#cfd8e8;letter-spacing:.05em}
.envc .l1 b{font-size:19px;line-height:1;font-weight:900;font-style:normal;color:#fff;
  letter-spacing:0;font-variant-numeric:tabular-nums;text-shadow:0 2px 3px rgba(0,0,0,.5)}
.envc .tr{font-size:13px;font-weight:900;font-style:normal;line-height:1}
.envc .tr.up{color:#ffb86b}
.envc .tr.down{color:#8fd0ff}
.envc .tr.flat{color:#cfd8e8}
/* Min and Max rows. The timestamp carries the day's extreme, so it comes first,
   then the label, then the value. Min is blue and Max orange, the same two
   colours the Hen House temperature cards use; the current reading above stays
   white. */
.envc .mm{display:grid;grid-template-columns:auto auto auto;justify-content:center;
  column-gap:5px;row-gap:3px;font-size:9.5px;line-height:1.12;font-style:italic;color:#cdd6e4;
  white-space:nowrap;font-variant-numeric:tabular-nums}
.envc .mm .lo,.envc .mm .hi{display:contents}
.envc .mm .t{min-width:4.4ch;text-align:right;font-style:normal;color:#9aa6b8}
.envc .mm .k{text-align:left}
.envc .mm b{font-style:normal;font-weight:700}
.envc .mm .lo b{color:#8fd0ff}
.envc .mm .hi b{color:#ffb86b}

/* Four across by default, two-by-two only on the narrowest phones. */
@media (max-width:359px){.r4,.r3{grid-template-columns:repeat(2,minmax(0,1fr))}}

.foot{display:flex;align-items:center;gap:10px;margin-top:18px;color:#b8c0cf;font-style:italic;
  font-size:clamp(10px,3vw,13px)}
.foot i{flex:1;height:1px;background:#4b5566}
.foot svg{width:18px;flex:0 0 auto}
@media (prefers-reduced-motion:reduce){*{transition:none!important}}
</style>
</head>
<body>

<svg width="0" height="0" style="position:absolute" aria-hidden="true"><defs>
  <linearGradient id="gMetal" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#f1f4fa"/><stop offset="1" stop-color="#7f8ba0"/></linearGradient>
  <linearGradient id="gBulb" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff0a0"/><stop offset="1" stop-color="#f2a900"/></linearGradient>
  <linearGradient id="gSun" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff3a8"/><stop offset="1" stop-color="#f28a00"/></linearGradient>
  <linearGradient id="gLens" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#8fd0ff"/><stop offset="1" stop-color="#1a5fb0"/></linearGradient>
</defs></svg>

<main>

<div class="card head" id="cardHead" style="--c:#5a6578;--t:#1b2436">
  <div class="titlebox">
    <div class="title"><b>HIGI</b> <span class="hh">Coop Cam</span> <span id="ver">%VERSION%</span></div>
    <div class="tag">Camera &bull; IR Lamp &bull; Sunrise &amp; Sunset</div>
  </div>
</div>

<!-- Camera | IR lamp -->
<div class="row r2 bigrow">

  <div class="card tile" id="cardCam" style="--c:#5a6578;--t:#1b2436">
    <div class="ic"><svg viewBox="0 0 64 64">
      <rect x="5" y="20" width="40" height="30" rx="6" fill="#1b2230" stroke="url(#gMetal)" stroke-width="3"/>
      <circle cx="25" cy="35" r="11" fill="#0a0f18" stroke="url(#gMetal)" stroke-width="2.5"/>
      <circle cx="25" cy="35" r="6" fill="url(#gLens)"/>
      <circle cx="22.5" cy="32.5" r="2" fill="#fff" fill-opacity=".85"/>
      <path d="M45 28l13-7v32l-13-7z" fill="#2b3646" stroke="url(#gMetal)" stroke-width="2.5" stroke-linejoin="round"/>
      <rect x="11" y="13" width="12" height="7" rx="2.5" fill="#2b3646" stroke="url(#gMetal)" stroke-width="2"/>
      <circle cx="38" cy="26" r="2" fill="#ff5a4e"/>
    </svg></div>
    <div class="lb">Coop Camera</div>
    <a class="act tile-toggle" id="camToggleBtn" href="/cam/on"
       title="Tap to turn camera ON">OFF</a>
    <div class="schedule col">
      <div><span>OFF</span><b id="camOff1">--:--</b></div>
      <div><span>OFF</span><b id="camOff2">--:--</b></div>
    </div>
  </div>

  <div class="card tile" id="cardLamp" style="--c:#5a6578;--t:#1b2436">
    <div class="ic"><svg class="bulb" id="bulb" viewBox="0 0 64 64">
      <path d="M32 3a19 19 0 0 0-11 34c2.5 2 4 4.5 4 9h14c0-4.5 1.5-7 4-9A19 19 0 0 0 32 3z" fill="url(#gBulb)" stroke="#fff2b0" stroke-opacity=".7" stroke-width="2"/>
      <path d="M26 22l6 9 6-9" fill="none" stroke="#a86a00" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
      <path d="M32 31v15" stroke="#a86a00" stroke-width="2"/>
      <rect x="24" y="48" width="16" height="4" rx="2" fill="url(#gMetal)"/>
      <rect x="25" y="53" width="14" height="4" rx="2" fill="url(#gMetal)"/>
      <rect x="28" y="58" width="8" height="4" rx="2" fill="url(#gMetal)"/>
      <path d="M20 16a14 14 0 0 1 8-8" stroke="#fff" stroke-opacity=".8" stroke-width="3" fill="none" stroke-linecap="round"/>
    </svg></div>
    <div class="lb">IR Illuminator</div>
    <a class="act tile-toggle" id="lampToggleBtn" href="/lamp/on"
       title="Tap to turn IR illuminator ON">OFF</a>
    <div class="schedule col">
      <div><span>ON</span><b id="irOn">--:--</b></div>
      <div><span>OFF</span><b id="irOff">--:--</b></div>
    </div>
  </div>

</div>

<!-- Automatic schedule | Temporary IR brightness adjustment -->
<div class="row r2 grow">
  <div class="card auto duo" id="viewCard" role="button" tabindex="0" %VIEWHIDE%
       style="--c:#55558F;--t:#292A50" title="Camera off &mdash; press WAKE to power it up">
    <div class="lb">View Camera</div>
    <div class="hint">Tap for the live stream</div>
    <div class="viewbar">
      <span class="pill" id="viewCount" hidden></span>
      <span class="pill" id="viewPill">WAKE</span>
    </div>
  </div>

  <div class="card duo" id="irAdjustCard" style="--c:#5a6578;--t:#1b2436"
       title="Brightness can only be adjusted while the IR lamp is ON">
    <div class="lb">IR Brightness</div>
    <div class="hint" id="irBrightnessHint">Current: 92%</div>
    <div class="ir-panel" id="irPanel" hidden>
      <div class="ir-current" id="irBrightnessValue">92%</div>
      <div class="ir-buttons">
        <button class="pill ir-step" id="irDown" type="button" disabled>− DOWN</button>
        <button class="pill ir-step" id="irUp" type="button" disabled>+ UP</button>
      </div>
      <div class="ir-note" id="irAdjustHint">1% per press · temporary until reboot</div>
    </div>
    <button class="pill ir-toggle" id="irSettingsToggle" type="button" aria-expanded="false"
            aria-controls="irPanel" disabled>ADJUST</button>
  </div>
</div>

<!-- Watch camera | Lens heater | Sun | serial log -->
<div class="row r4">

  <a class="card tile" id="autoCard" href="/override/clear" title="Return to the automatic schedule">
    <div class="lb">Automatic</div>
    <span class="pill" id="autoPill">AUTO</span>
    <span id="autoHint" hidden></span>
  </a>

  <a class="card tile" id="heaterCard" href="/heater/toggle"
     aria-disabled="%HEATERDISABLED%"
     style="--c:#55558F;--t:#292A50" title="Lens heater">
    <div class="lb">Lens Heater</div>
    <div class="schedule col">
      <div><span>Dew</span><b id="heaterDew">--</b></div>
    </div>
    <span class="pill" id="heaterPill">N/A</span>
  </a>

  <div class="card tile" id="sunCard" style="--c:#55558F;--t:#292A50" title="Sunrise and sunset">
    <div class="lb">Sunrise / Sunset</div>
    <div class="schedule col">
      <div><span>Rise</span><b id="sunrise">--:--</b></div>
      <div><span>Set</span><b id="sunset">--:--</b></div>
    </div>
  </div>

  <div class="card tile logtile" id="logTile" role="button" tabindex="0"
       aria-expanded="false" aria-controls="logCard"
       style="--c:#5a6578;--t:#1b2436" title="Show the serial log">
    <div class="lb">Serial Log</div>
    <span class="pill" id="logPill">Show</span>
  </div>

</div>

<!-- Ceiling air: temperature and humidity, one banded card each -->
<div class="row r2"%SENSORHIDE%>
  <div class="card envc" id="tempCard" style="--c:#5a6578;--t:#222a38"
       title="Ceiling air temperature">
    <div class="l1">TEMP <b id="envTemp">--</b><i class="tr" id="envTempTr"></i></div>
    <div class="mm">
      <span class="lo"><span class="t" id="envTempMinAt">--:--</span><span class="k">Min</span><b id="envTempMin">--</b></span>
      <span class="hi"><span class="t" id="envTempMaxAt">--:--</span><span class="k">Max</span><b id="envTempMax">--</b></span>
    </div>
  </div>
  <div class="card envc" id="humCard" style="--c:#5a6578;--t:#222a38"
       title="Ceiling air humidity">
    <div class="l1">HUMIDITY <b id="envHum">--</b><i class="tr" id="envHumTr"></i></div>
    <div class="mm">
      <span class="lo"><span class="t" id="envHumMinAt">--:--</span><span class="k">Min</span><b id="envHumMin">--</b></span>
      <span class="hi"><span class="t" id="envHumMaxAt">--:--</span><span class="k">Max</span><b id="envHumMax">--</b></span>
    </div>
  </div>
</div>

<!-- Serial log panel, revealed by the Serial Log tile -->
<div class="card logcard" id="logCard" hidden style="--c:#5a6578;--t:#1b2436">
  <div class="logbar">
    <span class="logttl">Serial Log</span>
    <span class="logst" id="logSt">--</span>
    <button class="pill logbtn" id="logPause" type="button">Pause</button>
    <button class="pill logbtn" id="logClear" type="button">Clear</button>
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
const setPill = (id, txt, cls) => { const e = $(id); e.textContent = txt; e.className = 'pill ' + cls; };

// Same three states as the Hen House: up, down and a double-headed steady.
// The server sends "up" / "down" / "steady", or an empty string until it has
// enough history to be honest.
function setTrend(id, t) {
  const m = { up: ['\u2191', 'up', 'Rising'], down: ['\u2193', 'down', 'Falling'],
              steady: ['\u2194', 'flat', 'Steady'] }[t];
  const el = $(id);
  el.textContent = m ? m[0] : '';
  el.className = 'tr' + (m ? ' ' + m[1] : '');
  if (m) el.setAttribute('aria-label', m[2]); else el.removeAttribute('aria-label');
}

const PAL = {
  night:     ['#55558F', '#292A50'],
  preDawn:   ['#8068B8', '#382A5B'],
  sunrise:   ['#E8AD3F', '#5B3310'],
  morning:   ['#FFE98A', '#4A3C10'],
  midday:    ['#FFD23F', '#4A3800'],
  afternoon: ['#FF9F45', '#4A2A08'],
  sunset:    ['#F4A082', '#4E2430']
};

// Card colours. Only the two loads keep gold when they are ON; everything else
// follows the sky. This gold sits a shade below the pill gold (#ffe45c ->
// #eea000) so the ON/OFF buttons stay the brightest thing on the card.
const CARD_GOLD = ['#d9a52a', '#3b3115'];
// The two loads drop to the Hen House "off" blue when they are not running,
// so they read the same as the Door / Lighting cards next door.
const CARD_SKY  = ['#63b8ff', '#16385a'];

// Ceiling air bands. The two readings get their own border and tint instead of
// following the sky: temperature runs pale blue at freezing through green in
// the comfortable band to red above 30 C, and moisture runs amber when dry
// through green when comfortable to indigo at saturation. Hysteresis stops a
// reading that sits on an edge from flickering between two colours.
const TEMP_EDGES = [0, 5, 10, 20, 25, 30];
const TEMP_HYST  = 0.3;
const TEMP_COLOURS = [
  ['#b7b9ff', '#2a2d5e'],  // below 0   freezing   pale violet-blue
  ['#2f7de0', '#17305a'],  // 0 to 5    cold       blue
  ['#4fd0e8', '#12414f'],  // 5 to 10   cooler     light cyan
  ['#3fcf3a', '#164022'],  // 10 to 20  comfortable green
  ['#e6d23a', '#4a4312'],  // 20 to 25  warmer     yellow
  ['#f08a1c', '#4d2a0c'],  // 25 to 30  hot        orange
  ['#e0393e', '#4a1c24']   // above 30  dangerous  red
];
const HUM_EDGES = [40, 60, 80, 91, 97];
const HUM_HYST  = 1.5;
const HUM_COLOURS = [
  ['#f59e0b', '#4a2f03'],  // below 40  dry          amber
  ['#84cc16', '#283d07'],  // 40 to 60  fresh        lime
  ['#10b981', '#063a29'],  // 60 to 80  comfortable  green
  ['#06b6d4', '#043b45'],  // 80 to 91  damp         teal
  ['#3b82f6', '#12274a'],  // 91 to 97  wet          blue
  ['#6366f1', '#232253']   // 97 to 100 saturated    indigo
];
const AIR_NONE = ['#5a6578', '#222a38'];  // no reading: neutral grey
const tempBandState = {}, humBandState = {};

function bandFor(x, prev, edges, hyst) {
  if (prev === undefined) {
    let i = 0;
    while (i < edges.length && x >= edges[i]) i++;
    return i;
  }
  let i = prev;
  while (i < edges.length && x >= edges[i] + hyst) i++;      // climb
  while (i > 0 && x < edges[i - 1] - hyst) i--;              // fall
  return i;
}

// Each card owns its band state, so the two readings cannot borrow each
// other's colour if one of them drops out.
function setAirCard(id, reading, edges, hyst, colours, state) {
  let col = AIR_NONE;
  if (reading !== null && reading !== undefined && isFinite(reading)) {
    state[id] = bandFor(reading, state[id], edges, hyst);
    col = colours[state[id]];
  } else {
    state[id] = undefined;
  }
  setCardColour(id, col);
}

// One-tap camera watch. VIEW_URL is filled in by handleRoot().
const VIEW_URL     = '%VIEWURL%';
const WAKE_SECONDS = %WAKESECS%;
let cameraOn = false;   // relay state from the last /status.json
let waking   = false;   // wake countdown in progress

// Lift only the 24-hour palette cards toward their accent, then pull that
// lift back toward grey, so the body reads as a quiet tint rather than a
// colour. Everything else keeps its own flat tint; CSS falls back to --t.
// The four small tiles carry the tint over their whole height, so they read
// stronger at the same strength: they get a gentler, greyer lift of their own.
const CARD_PUNCH = 0.14, CARD_GREY = 0.15;
const CARD_PUNCH_SMALL = 0.10, CARD_GREY_SMALL = 0.40;
function cardTint(c, t, a, grey) {
  const px = h => [1, 3, 5].map(i => parseInt(h.slice(i, i + 2), 16));
  const C = px(c), T = px(t);
  let o = T.map((v, i) => v + (C[i] - v) * a);
  if (grey) {
    const g = 0.299 * o[0] + 0.587 * o[1] + 0.114 * o[2];
    o = o.map(v => v + (g - v) * grey);
  }
  return '#' + o.map(v => Math.round(v).toString(16).padStart(2, '0')).join('');
}
function setCardColour(id, col, punch) {
  const el = $(id);
  el.style.setProperty('--c', col[0]);
  el.style.setProperty('--t', col[1]);
  if (punch) {
    el.style.setProperty('--t2', cardTint(col[0], col[1], CARD_PUNCH, CARD_GREY));
    el.style.setProperty('--t2s', cardTint(col[0], col[1], CARD_PUNCH_SMALL, CARD_GREY_SMALL));
  } else {
    el.style.removeProperty('--t2');
    el.style.removeProperty('--t2s');
  }
}

function render(d) {
  document.title = 'Coop Cam ' + d.version;
  $('ver').textContent  = d.version;
  $('camOff1').textContent = d.camOff1 || '';
  $('camOff2').textContent = d.camOff2 || '';
  $('camOff2').parentNode.hidden = !d.camOff2;
  $('irOn').textContent    = d.irOn;
  $('irOff').textContent   = d.irOff;

  // One centred toggle per load. Gold follows the live state and the href is
  // always the opposite action, so a tap flips the current state.
  const camBtn = $('camToggleBtn');
  camBtn.textContent = d.camera ? 'ON' : 'OFF';
  camBtn.classList.toggle('on', !!d.camera);
  camBtn.setAttribute('href', d.camera ? '/cam/off' : '/cam/on');
  camBtn.title = d.camera ? 'Tap to turn camera OFF' : 'Tap to turn camera ON';

  const lampBtn = $('lampToggleBtn');
  lampBtn.textContent = d.lamp ? 'ON' : 'OFF';
  lampBtn.classList.toggle('on', !!d.lamp);
  lampBtn.setAttribute('href', d.lamp ? '/lamp/off' : '/lamp/on');
  lampBtn.title = d.lamp ? 'Tap to turn IR illuminator OFF'
                         : 'Tap to turn IR illuminator ON';
  $('bulb').classList.toggle('lit', !!d.lamp);

  const brightness = Number.isFinite(Number(d.irBrightness)) ? Number(d.irBrightness) : 0;
  $('irBrightnessHint').textContent = 'Current: ' + brightness + '%';
  $('irBrightnessValue').textContent = brightness + '%';
  $('irAdjustCard').title = d.lamp
    ? 'Adjust IR brightness \\u00b7 1% per press \\u00b7 temporary until reboot'
    : 'Brightness can only be adjusted while the IR lamp is ON';
  $('irSettingsToggle').disabled = !d.lamp;
  $('irDown').disabled = !d.lamp || brightness <= 0;
  $('irUp').disabled = !d.lamp || brightness >= 100;
  if (!d.lamp) {
    $('irPanel').hidden = true;
    $('irSettingsToggle').setAttribute('aria-expanded', 'false');
    $('irSettingsToggle').textContent = 'ADJUST';
  }

  // Card borders. Gold means one thing only: this load is running; when it is
  // off the card drops to the Hen House blue. Header, schedule, log and camera
  // tiles follow the sky. The heater tile mirrors the ceiling temperature
  // colour and the Sun card mirrors the ceiling humidity colour.
  const pal = PAL[d.dayPhase] || PAL.night;
  setCardColour('cardCam',      d.camera ? CARD_GOLD : CARD_SKY);
  setCardColour('cardLamp',     d.lamp   ? CARD_GOLD : CARD_SKY);
  setCardColour('autoCard',     pal, true);
  setCardColour('irAdjustCard', pal, true);
  setCardColour('cardHead',     pal, true);
  setCardColour('viewCard',     pal, true);
  setCardColour('logTile',      pal, true);
  setCardColour('logCard',      pal, true);

  // Coop High. The card stays in place while the sensor is missing or disabled,
  // showing dashes rather than disappearing and moving every other card.
  const sensorOk = d.sensor === 'ok';
  const num = v => (v === null || v === undefined || v === '') ? null : Number(v);
  const temp = num(d.temp), hum = num(d.hum);
  const tmin = num(d.tempMin), hmax = num(d.humMax);

  $('envTemp').textContent = sensorOk && temp !== null ? temp.toFixed(1) + '\u00b0C' : '--';
  $('envHum').textContent  = sensorOk && hum  !== null ? Math.round(hum) + '%' : '--';
  const tmax = num(d.tempMax), hmin = num(d.humMin);
  $('envTempMin').textContent = sensorOk && tmin !== null ? tmin.toFixed(1) + '\u00b0' : '--';
  $('envTempMax').textContent = sensorOk && tmax !== null ? tmax.toFixed(1) + '\u00b0' : '--';
  $('envHumMin').textContent  = sensorOk && hmin !== null ? Math.round(hmin) + '%' : '--';
  $('envHumMax').textContent  = sensorOk && hmax !== null ? Math.round(hmax) + '%' : '--';
  $('envTempMinAt').textContent = sensorOk ? (d.tempMinAt || '--:--') : '--:--';
  $('envTempMaxAt').textContent = sensorOk ? (d.tempMaxAt || '--:--') : '--:--';
  $('envHumMinAt').textContent  = sensorOk ? (d.humMinAt  || '--:--') : '--:--';
  $('envHumMaxAt').textContent  = sensorOk ? (d.humMaxAt  || '--:--') : '--:--';
  setTrend('envTempTr', sensorOk ? d.tempTrend : null);
  setTrend('envHumTr',  sensorOk ? d.humTrend  : null);
  setAirCard('tempCard', sensorOk ? temp : null, TEMP_EDGES, TEMP_HYST, TEMP_COLOURS, tempBandState);
  setAirCard('humCard',  sensorOk ? hum  : null, HUM_EDGES,  HUM_HYST,  HUM_COLOURS,  humBandState);
  // The small bottom tiles mirror the two air cards. Grey follows the matching
  // air card when there is no reading.
  const heaterBorder = TEMP_COLOURS[tempBandState['tempCard']] || AIR_NONE;
  const sunBorder    = HUM_COLOURS[humBandState['humCard']] || AIR_NONE;
  setCardColour('heaterCard', heaterBorder);
  setCardColour('sunCard',    sunBorder);
  const sensorNote = sensorOk
    ? 'Ceiling air \u00b7 last sample ' + (d.sensorAt || '--:--')
    : (d.sensor === 'off' ? 'SHT41 disabled' : 'SHT41 not responding');
  $('tempCard').title = 'Temperature \u00b7 ' + sensorNote;
  $('humCard').title  = 'Humidity \u00b7 ' + sensorNote;

  const heaterEnabled = !!d.heaterEnabled;
  const heaterOn      = !!d.heater;
  const dew = num(d.dewPoint);
  const manualMin = Number(d.heaterMin) || 0;
  const autoMin = Number(d.heaterAutoMin) || 0;
  const heaterAuto = !!d.heaterAuto;
  setPill('heaterPill',
          !heaterEnabled ? 'N/A'
          : (d.heaterManual ? manualMin + 'm'
          : (heaterAuto ? 'Auto ' + autoMin + 'm'
                        : (heaterOn ? 'Heat ON' : 'Heat OFF'))),
          heaterEnabled && heaterOn ? 'on' : '');
  $('heaterDew').textContent = dew !== null ? dew.toFixed(1) + '\u00b0C' : '--';
  $('heaterCard').title = !heaterEnabled
    ? 'Heater disabled in firmware settings'
    : (d.heaterManual
        ? 'Tap to cancel manual heating \u00b7 ' + manualMin + 'm left'
        : (heaterAuto
            ? 'One-shot auto heat \u00b7 ' + autoMin + 'm left \u00b7 tap to take over'
            : 'Tap for ' + (d.heaterManualMinutes || 5) + 'm manual heat'));
  $('heaterCard').setAttribute('aria-disabled', heaterEnabled ? 'false' : 'true');

  $('sunrise').textContent = d.sunrise || '--:--';
  $('sunset').textContent  = d.sunset  || '--:--';

  cameraOn = !!d.camera;
  if (!waking) {
    setPill('viewPill', cameraOn ? 'VIEW' : 'WAKE', '');
    $('viewCard').title = !VIEW_URL ? 'Camera not configured yet'
      : cameraOn ? 'Press VIEW to open the live stream in a new tab'
                 : 'Camera off \u2014 press WAKE to power it up';
  }

  const manual = !!d.overrideActive;
  setPill('autoPill', manual ? 'MANUAL' : 'AUTO', manual ? 'on' : '');
  $('autoHint').textContent = manual
    ? 'Tap to cancel \u00b7 ' + d.overrideMin + 'm left'
    : 'Daily schedule running';
}

async function update() {
  try {
    const res = await fetch('/status.json', { cache: 'no-store', signal: AbortSignal.timeout(4000) });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    render(await res.json());
  } catch (e) {}
}

// ON/OFF act in place: no reload, no white flash. The handler applies the
// override and replies with the fresh status, so the gold moves immediately.
let acting = false;
async function act(el) {
  if (acting) return;
  acting = true;
  try {
    const res = await fetch(el.getAttribute('href'), {
      cache: 'no-store',
      headers: { 'Accept': 'application/json' }
    });
    if (res.ok) render(await res.json());
  } catch (e) {}
  acting = false;
}

document.querySelectorAll('a.act').forEach(el => {
  el.addEventListener('click', e => {
    if (e.button !== 0 || e.metaKey || e.ctrlKey || e.shiftKey || e.altKey) return;
    e.preventDefault();
    act(el);
  });
});

// The heater tile is a whole card, not a small a.act button. It gets its own
// handler so it keeps the normal 18px card corners instead of the pill shape.
const heaterCard = $('heaterCard');
function heaterTap() {
  if (heaterCard.getAttribute('aria-disabled') === 'true') return;
  act(heaterCard);
}
heaterCard.addEventListener('click', e => {
  if (e.button !== 0 || e.metaKey || e.ctrlKey || e.shiftKey || e.altKey) return;
  e.preventDefault();
  heaterTap();
});
heaterCard.addEventListener('keydown', e => {
  if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); heaterTap(); }
});

// ---- Watch camera ----
// One tap opens the camera's own page. If the relay is off, the WAKE pill
// fades to a ghost, the left pill counts the camera down, and at zero the
// right pill ramps back up as VIEW. Pressing VIEW is a real click, so the
// browser allows the window with no workarounds.
// The same named window is reused on every tap. The camera stores its login
// in sessionStorage, so after one sign-in that tab goes straight to the live
// view instead of showing the login box again. Closing the tab resets it.
let wakeTimer = 0;

function openView() {
  if (!VIEW_URL) return;
  window.open(VIEW_URL, 'coopcamview');
}

function viewWake() {
  waking = true;
  let left = WAKE_SECONDS;
  $('viewPill').classList.add('dim');         // WAKE fades to a ghost
  setPill('viewCount', left + 's', 'on');
  $('viewCount').hidden = false;
  $('viewCard').title = 'Camera waking \u2014 ' + left + ' s';
  fetch('/cam/on', { cache: 'no-store' }).catch(() => {});
  clearInterval(wakeTimer);
  wakeTimer = setInterval(() => {
    if (--left > 0) {
      setPill('viewCount', left + 's', 'on');
      $('viewCard').title = 'Camera waking \u2014 ' + left + ' s';
      return;
    }
    clearInterval(wakeTimer);
    waking = false;
    $('viewCount').hidden = true;             // left pill disappears
    setPill('viewPill', 'VIEW', '');           // right pill ramps back up
    $('viewCard').title = !VIEW_URL ? 'Camera not configured yet'
      : 'Press VIEW to open the live stream in a new tab';
    update();
  }, 1000);
}

function viewTap() {
  if (!VIEW_URL || waking) return;
  if (cameraOn) openView();
  else viewWake();
}

$('viewCard').addEventListener('click', viewTap);
$('viewCard').addEventListener('keydown', e => {
  if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); viewTap(); }
});

$('irSettingsToggle').addEventListener('click', () => {
  if ($('irSettingsToggle').disabled) return;
  const open = $('irPanel').hidden;
  $('irPanel').hidden = !open;
  $('irSettingsToggle').setAttribute('aria-expanded', open ? 'true' : 'false');
  $('irSettingsToggle').textContent = open ? 'CLOSE' : 'ADJUST';
});

async function changeIrBrightness(direction) {
  if ($('irDown').disabled && direction < 0) return;
  if ($('irUp').disabled && direction > 0) return;
  try {
    const endpoint = direction > 0 ? '/ir/up' : '/ir/down';
    const res = await fetch(endpoint, { cache: 'no-store' });
    if (!res.ok) throw new Error('IR adjustment rejected');
    const data = await res.json();
    $('irBrightnessHint').textContent = 'Current: ' + data.irBrightness + '%';
    $('irBrightnessValue').textContent = data.irBrightness + '%';
    $('irAdjustHint').textContent = '1% per press · temporary until reboot';
    await update();
  } catch (e) {
    $('irAdjustHint').textContent = 'Adjustment unavailable — check that IR is ON';
    await update();
  }
}
$('irDown').addEventListener('click', () => changeIrBrightness(-1));
$('irUp').addEventListener('click', () => changeIrBrightness(1));

// ---- Serial log ----
// Same behaviour as the Hen House: the tile reveals a full-width panel, and the
// log is only fetched while that panel is open. A changed boot id means the
// device restarted. Pause freezes the text while the device keeps recording;
// Clear only empties the page.
const logTile = $('logTile'), logCard = $('logCard'), logOut = $('logOut'),
      logSt = $('logSt'), logPause = $('logPause');
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
        const r = await fetch('/api/log?since=' + logSince + '&boot=' + logBoot,
                              { signal: AbortSignal.timeout(6000), cache: 'no-store' });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        const t = await r.text();
        const nb = Number(r.headers.get('X-Log-Boot'));
        const nx = Number(r.headers.get('X-Log-Next'));
        const fr = Number(r.headers.get('X-Log-From'));
        if (logBoot && nb !== logBoot) logAdd('\n--- device restarted ---\n');
        else if (logSince > 0 && fr > logSince) logAdd('\n--- some lines were missed ---\n');
        logBoot = nb; logSince = nx;
        if (t) logAdd(t);
        logSt.textContent = 'Live';
        if (t.length >= 4096) wait = 50;
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
  logCard.hidden = !logOpen;
  setPill('logPill', logOpen ? 'Hide' : 'Show', logOpen ? 'on' : '');
  logTile.setAttribute('aria-expanded', logOpen ? 'true' : 'false');
  if (logOpen) {
    logSt.textContent = logPaused ? 'Paused' : 'Loading';
    logPoll();
    logCard.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
  }
}
logTile.addEventListener('click', logToggle);
logTile.addEventListener('keydown', e => {
  if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); logToggle(); }
});
logPause.addEventListener('click', () => {
  logPaused = !logPaused;
  logPause.textContent = logPaused ? 'Resume' : 'Pause';
  logSt.textContent = logPaused ? 'Paused' : 'Live';
});
$('logClear').addEventListener('click', () => { logOut.textContent = ''; });

update();
setInterval(update, 5000);
</script>
</body>
</html>
)rawliteral";

void redirectHome() { server.sendHeader("Location", "/"); server.send(303); }

// The dashboard's ON/OFF buttons fetch these endpoints and ask for JSON, so a
// tap updates in place. Plain navigation (no JavaScript) still gets the 303.
// tick() runs first so the reply describes the state that was just requested.
bool wantsJson() { return server.header("Accept").indexOf("application/json") >= 0; }

void replyAction() {
  if (wantsJson()) { server.send(200, "application/json", buildJson()); return; }
  redirectHome();
}

void handleRoot() {
  bool showWatch = SHOW_WATCH_CARD_ANYWAY || CAMERA_VIEW_URL[0];
  String html = FPSTR(INDEX_HTML);
  html.replace("%VERSION%", FW_VERSION);
  html.replace("%VIEWURL%",  CAMERA_VIEW_URL);
  html.replace("%WAKESECS%", String((int)CAMERA_WAKE_SECONDS));
  html.replace("%VIEWHIDE%", showWatch ? "" : " hidden");
  html.replace("%ROWCLS%",   showWatch ? "" : "r3");
  html.replace("%HEATERDISABLED%", HEATER_ENABLED ? "false" : "true");
  html.replace("%SENSORHIDE%", SENSOR_ENABLED ? "" : " hidden");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html", html);
}
void handleStatus() { server.send(200, "application/json", buildJson()); }

// ---- remote log ------------------------------------------------------
// RAM ring buffer written by Log.* (RemoteLog.h). The page polls for everything
// after the byte offset it already has, so no line is shown twice. A random
// boot id lets the page notice a restart and say so.
RemoteLog Log;
static uint32_t logBootId = 0;

void handleLog() {
  if (logBootId == 0) logBootId = esp_random() | 1UL;
  uint32_t since = strtoul(server.arg("since").c_str(), nullptr, 10);
  uint32_t boot  = strtoul(server.arg("boot").c_str(),  nullptr, 10);
  if (boot != logBootId) since = 0;   // first visit or device restarted

  String body;
  uint32_t from = 0;
  uint32_t next = Log.readFrom(since, body, 4096, from);

  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Log-Boot", String(logBootId));
  server.sendHeader("X-Log-From", String(from));
  server.sendHeader("X-Log-Next", String(next));
  server.send(200, "text/plain; charset=utf-8", body);
}

void handleLampOn() { startOverride(true, true);        tick(); replyAction(); }
void handleLampOff(){ startOverride(false, appliedCam); tick(); replyAction(); }
void handleIrUp() {
  if (!adjustIrBrightness(1)) {
    server.send(409, "application/json", "{\"error\":\"IR lamp is off\"}");
    return;
  }
  server.send(200, "application/json", buildJson());
}
void handleIrDown() {
  if (!adjustIrBrightness(-1)) {
    server.send(409, "application/json", "{\"error\":\"IR lamp is off\"}");
    return;
  }
  server.send(200, "application/json", buildJson());
}
void handleCamOn()  { startOverride(appliedLamp, true);  tick(); replyAction(); }
void handleCamOff() { startOverride(false, false);        tick(); replyAction(); }
void handleClear()  { overrideActive = false;             tick(); replyAction(); }
void handleHeaterToggle() { toggleHeaterManual();         tick(); replyAction(); }

void handleSim() {
  String a = server.arg("t");
  a.trim();
  if (a.length() == 0 || a.equalsIgnoreCase("off")) {
    simMinutes = -1;
  } else {
    int h = -1, m = 0;
    if (sscanf(a.c_str(), "%d:%d", &h, &m) != 2) {
      if (sscanf(a.c_str(), "%d", &h) == 1) m = 0;
    }
    if (h >= 0 && h < 24 && m >= 0 && m < 60) simMinutes = h * 60 + m;
  }
  Serial.printf("Sim: %s\n", simMinutes >= 0 ? fmtMin(simMinutes).c_str() : "off");
  redirectHome();
}

void handleReboot() {
  server.send(200, "text/plain", "Rebooting...");
  delay(100);
  ESP.restart();
}

// ---- registration ---------------------------------------------------
void dashboardSetup() {
  server.on("/",               handleRoot);
  server.on("/status.json",    handleStatus);
  server.on("/api/log",        handleLog);
  server.on("/lamp/on",        handleLampOn);
  server.on("/lamp/off",       handleLampOff);
  server.on("/ir/up",          handleIrUp);
  server.on("/ir/down",        handleIrDown);
  server.on("/cam/on",         handleCamOn);
  server.on("/cam/off",        handleCamOff);
  server.on("/override/clear", handleClear);
  server.on("/heater/toggle",  handleHeaterToggle);
  server.on("/sim",            handleSim);
  server.on("/reboot",         handleReboot);
  server.begin();
}
