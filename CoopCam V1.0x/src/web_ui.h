#pragma once

#include <Arduino.h>

// Single-page control panel. Kept in flash (PROGMEM) so no filesystem is needed.
const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#0f1419">
<title>Coop Camera</title>
<style>
  :root{
    color-scheme:dark;
    --bg:#0e1318; --card:#171f27; --card2:#1d2833; --line:#2b3a47;
    --text:#e8eef4; --muted:#91a4b5; --accent:#f5b942; --accent2:#ffd166;
    --ok:#3ddc84; --warn:#f5b942; --bad:#ff6b6b; --blue:#5aa9ff;
  }
  *{box-sizing:border-box}
  html,body{margin:0;padding:0}
  body{
    background:radial-gradient(1200px 600px at 50% -200px,#1b2a38 0%,var(--bg) 60%);
    color:var(--text);
    font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;
    min-height:100vh;
  }
  .wrap{max-width:980px;margin:0 auto;padding:18px 16px 48px}
  header{display:flex;align-items:center;justify-content:space-between;gap:12px;margin:6px 0 16px}
  h1{font-size:1.45rem;margin:0;letter-spacing:.2px}
  h1 span{color:var(--accent)}
  h2{font-size:1rem;margin:22px 0 10px;color:var(--accent2);font-weight:650}
  .pill{font-size:.78rem;padding:5px 10px;border-radius:999px;border:1px solid var(--line);background:var(--card);white-space:nowrap}
  .pill.ok{color:var(--ok);border-color:#245c3f}
  .pill.warn{color:var(--warn);border-color:#5c4a1f}
  .pill.bad{color:var(--bad);border-color:#5c2424}
  .grid{display:grid;grid-template-columns:repeat(4,1fr);gap:12px}
  @media(max-width:760px){.grid{grid-template-columns:repeat(2,1fr)}}
  .card{background:linear-gradient(180deg,var(--card2),var(--card));border:1px solid var(--line);border-radius:14px;padding:14px;box-shadow:0 10px 30px #0003}
  .label{font-size:.76rem;text-transform:uppercase;letter-spacing:.09em;color:var(--muted);margin-bottom:6px}
  .big{font-size:1.35rem;font-weight:650;letter-spacing:.2px}
  .muted{color:var(--muted)}
  .mono{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;font-size:.84rem}
  .banner{margin:0 0 14px;padding:11px 13px;border-radius:12px;border:1px solid #5c4a1f;background:#2a2312;color:#ffd98a;font-size:.9rem}
  .banner.bad{border-color:#5c2424;background:#2a1414;color:#ffb3b3}
  .hidden{display:none!important}
  .row{display:flex;flex-wrap:wrap;gap:10px;margin-top:12px}
  button{
    appearance:none;border:1px solid var(--line);background:#22303d;color:var(--text);
    padding:11px 15px;border-radius:11px;font:inherit;font-weight:600;cursor:pointer;
    transition:transform .06s ease,filter .15s ease;touch-action:manipulation;
  }
  button:hover{filter:brightness(1.12)}
  button:active{transform:translateY(1px)}
  button.primary{background:linear-gradient(180deg,#f7c85c,#d99a22);border-color:#b37d13;color:#221a06}
  button.small{padding:8px 11px;font-size:.86rem;font-weight:550}
  button.danger{background:#3a1e1e;border-color:#6b2b2b;color:#ffb3b3}
  button.ghost{background:transparent}
  .grid2{display:grid;grid-template-columns:repeat(2,1fr);gap:12px}
  @media(max-width:620px){.grid2{grid-template-columns:1fr}}
  label{display:flex;flex-direction:column;gap:6px;font-size:.85rem;color:var(--muted)}
  label.check{flex-direction:row;align-items:center;gap:9px;color:var(--text);padding-top:6px}
  input[type=number],input[type=text],input[type=password]{
    width:100%;padding:10px 11px;border-radius:10px;border:1px solid var(--line);
    background:#0f171e;color:var(--text);font:inherit;outline:none;
  }
  input:focus{border-color:#4a6a86;box-shadow:0 0 0 3px #5aa9ff22}
  input[type=checkbox]{width:18px;height:18px;accent-color:var(--accent)}
  .toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%) translateY(20px);
    background:#22303d;border:1px solid var(--line);color:var(--text);padding:10px 14px;
    border-radius:11px;opacity:0;pointer-events:none;transition:.2s;font-size:.9rem;z-index:9;max-width:90vw}
  .toast.show{opacity:1;transform:translateX(-50%) translateY(0)}
  .toast.ok{border-color:#245c3f}
  .toast.bad{border-color:#5c2424}
  footer{margin-top:22px;text-align:center;font-size:.78rem;color:var(--muted)}
  .kv{display:grid;grid-template-columns:130px 1fr;gap:4px 12px}
  @media(max-width:520px){.kv{grid-template-columns:110px 1fr}}
  .kv div:nth-child(odd){color:var(--muted)}
</style>
</head>
<body>
<div class="wrap">
  <header>
    <h1>🐔 Coop <span>Camera</span></h1>
    <div id="conn" class="pill">connecting…</div>
  </header>

  <div id="banner" class="banner hidden"></div>

  <section class="grid">
    <div class="card">
      <div class="label">Local time</div>
      <div id="clock" class="big">—</div>
      <div id="date" class="muted">—</div>
    </div>
    <div class="card">
      <div class="label">Sunset window</div>
      <div id="sunset" class="big">—</div>
      <div id="sunsetSub" class="muted">—</div>
    </div>
    <div class="card">
      <div class="label">Camera</div>
      <div id="camState" class="big">—</div>
      <div id="camSub" class="muted">—</div>
    </div>
    <div class="card">
      <div class="label">IR lamp</div>
      <div id="lampState" class="big">—</div>
      <div id="lampSub" class="muted">—</div>
    </div>
  </section>

  <section class="card" style="margin-top:12px">
    <div class="row">
      <button class="primary" onclick="night()">Night view</button>
      <button onclick="alloff()">All off</button>
      <button class="ghost" onclick="autoMode()">Auto</button>
    </div>
    <div class="row">
      <button class="small" onclick="setCam('on')">Camera on</button>
      <button class="small" onclick="setCam('off')">Camera off</button>
      <button class="small" onclick="setLamp('on')">Lamp on</button>
      <button class="small" onclick="setLamp('off')">Lamp off</button>
    </div>
    <div id="override" class="muted" style="margin-top:12px">—</div>
  </section>

  <section class="card" style="margin-top:12px">
    <h2>Schedule</h2>
    <div class="grid2">
      <label>Latitude
        <input id="lat" type="number" step="0.0001" inputmode="decimal" placeholder="e.g. 37.5665">
      </label>
      <label>Longitude
        <input id="lon" type="number" step="0.0001" inputmode="decimal" placeholder="e.g. 126.9780">
      </label>
      <label>Camera ON before sunset (min)
        <input id="camBefore" type="number" min="0" max="720">
      </label>
      <label>Camera OFF after sunset (min)
        <input id="camAfter" type="number" min="0" max="720">
      </label>
      <label>Lamp ON before sunset (min)
        <input id="lampBefore" type="number" min="0" max="720">
      </label>
      <label>Lamp OFF after sunset (min)
        <input id="lampAfter" type="number" min="0" max="720">
      </label>
      <label>Manual override duration (min)
        <input id="manualMinutes" type="number" min="1" max="1440">
      </label>
      <label class="check"><input id="lampFollowsCamera" type="checkbox"> Lamp only when camera is on</label>
      <label class="check"><input id="dayMode" type="checkbox"> Camera also on during daylight</label>
      <label class="check"><input id="camAlwaysOn" type="checkbox"> Camera always on (ignore schedule)</label>
    </div>

    <h2>Network</h2>
    <div class="grid2">
      <label>Hostname
        <input id="hostname" type="text" maxlength="31">
      </label>
      <label>WiFi SSID
        <input id="ssid" type="text" maxlength="32" placeholder="leave blank to keep current">
      </label>
      <label>WiFi password
        <input id="pass" type="password" maxlength="63" placeholder="leave blank to keep current">
      </label>
    </div>

    <div class="row">
      <button class="primary" onclick="save()">Save settings</button>
      <button onclick="syncTime()">Sync time from browser</button>
      <button class="danger" onclick="reboot()">Reboot</button>
    </div>
    <div id="saveMsg" class="muted" style="margin-top:10px"></div>
  </section>

  <section class="card" style="margin-top:12px">
    <h2>Status</h2>
    <div id="status" class="kv mono muted"></div>
  </section>

  <footer>ESP32-S3 · DS3231 · <span id="ip">—</span></footer>
</div>
<div id="toast" class="toast"></div>

<script>
const $ = id => document.getElementById(id);
let last = null;
let settingsLoaded = false;

function fmtTime(epoch){
  if(!epoch) return '—';
  return new Date(epoch*1000).toLocaleTimeString([], {hour:'2-digit',minute:'2-digit'});
}
function fmtDateTime(epoch){
  if(!epoch) return '—';
  const d = new Date(epoch*1000);
  return d.toLocaleDateString([], {weekday:'short',day:'numeric',month:'short'}) + ' ' +
         d.toLocaleTimeString([], {hour:'2-digit',minute:'2-digit'});
}
function fmtDur(sec){
  sec = Math.max(0, Math.round(sec));
  const h = Math.floor(sec/3600), m = Math.floor((sec%3600)/60), s = sec%60;
  if(h) return h + 'h ' + m + 'm';
  if(m) return m + 'm ' + s + 's';
  return s + 's';
}
function toast(msg, kind){
  const t = $('toast');
  t.textContent = msg;
  t.className = 'toast show ' + (kind||'');
  clearTimeout(toast._t);
  toast._t = setTimeout(()=>{ t.className='toast ' + (kind||''); }, 2600);
}
async function post(path, data){
  const r = await fetch(path, {
    method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:new URLSearchParams(data||{})
  });
  if(!r.ok) throw new Error('HTTP ' + r.status);
  return r.json().catch(()=>({}));
}

async function getStatus(){
  try{
    const r = await fetch('/api/status', {cache:'no-store'});
    if(!r.ok) throw new Error('HTTP ' + r.status);
    const s = await r.json();
    last = s;
    render(s);
    if(!settingsLoaded) { populateSettings(s); settingsLoaded = true; }
  }catch(e){
    $('conn').textContent = 'offline';
    $('conn').className = 'pill bad';
  }
}

function render(s){
  const now = s.now;
  $('clock').textContent = new Date(now*1000).toLocaleTimeString();
  $('date').textContent = new Date(now*1000).toLocaleDateString([], {weekday:'long',day:'numeric',month:'long'});
  $('conn').textContent = s.wifi.ap ? 'setup AP' : (s.wifi.connected ? 'online' : 'offline');
  $('conn').className = 'pill ' + (s.wifi.ap ? 'warn' : (s.wifi.connected ? 'ok' : 'bad'));
  $('ip').textContent = s.wifi.ip || '—';

  // Banner
  const msgs = [];
  let bad = false;
  if(s.wifi.ap){
    msgs.push('Setup mode: join WiFi <b>' + s.apSsid + '</b> and open <b>http://192.168.4.1</b>.');
  }
  if(!s.timeValid){
    msgs.push('Clock is not set. Connect WiFi or press “Sync time from browser”.');
    bad = true;
  }
  if(!s.sun.valid && !s.config.camAlwaysOn){
    msgs.push('Set your latitude and longitude below to enable the sunset schedule.');
  }
  if(s.sun.polar){
    msgs.push('Polar day/night at this location — the sunset schedule is unavailable. Use manual control.');
  }
  const b = $('banner');
  if(msgs.length){ b.innerHTML = msgs.join('<br>'); b.className = 'banner' + (bad?' bad':''); }
  else b.className = 'banner hidden';

  // Sunset card
  if(s.config.camAlwaysOn){
    $('sunset').textContent = 'Always on';
    $('sunsetSub').textContent = 'Camera ignores the schedule';
  }else if(!s.sun.valid){
    $('sunset').textContent = '—';
    $('sunsetSub').textContent = 'Location not set';
  }else if(s.sun.polar){
    $('sunset').textContent = 'Polar';
    $('sunsetSub').textContent = 'No sunset window';
  }else{
    $('sunset').textContent = fmtTime(s.sun.windowSunset);
    $('sunsetSub').textContent =
      'Camera ' + fmtTime(s.sun.camStart) + '–' + fmtTime(s.sun.camEnd) +
      ' · Lamp ' + fmtTime(s.sun.lampStart) + '–' + fmtTime(s.sun.lampEnd);
  }

  // Camera card
  $('camState').textContent = s.cam.on ? 'ON' : 'OFF';
  $('camState').style.color = s.cam.on ? 'var(--ok)' : 'var(--muted)';
  let camSub = s.cam.override !== 'auto'
    ? 'manual · ' + s.cam.override
    : (s.config.camAlwaysOn ? 'always on' : (s.cam.scheduled ? 'schedule' : 'off'));
  if(s.cam.on && s.cam.onSince && (now - s.cam.onSince) < 90) camSub += ' · booting…';
  $('camSub').textContent = camSub;

  // Lamp card
  $('lampState').textContent = s.lamp.on ? 'ON' : 'OFF';
  $('lampState').style.color = s.lamp.on ? 'var(--accent)' : 'var(--muted)';
  $('lampSub').textContent = s.lamp.override !== 'auto'
    ? 'manual · ' + s.lamp.override
    : (s.lamp.scheduled ? 'schedule' : 'off');

  // Override line
  if(s.override.active){
    const who = [];
    if(s.cam.override === 'on') who.push('camera ON');
    if(s.cam.override === 'off') who.push('camera OFF');
    if(s.lamp.override === 'on') who.push('lamp ON');
    if(s.lamp.override === 'off') who.push('lamp OFF');
    const left = s.override.until ? fmtDur(s.override.until - now) + ' left' : 'until Auto';
    $('override').textContent = 'Manual override: ' + who.join(', ') + ' · ' + left;
  }else{
    $('override').textContent = 'Automatic — following the sunset schedule.';
  }

  // Status table
  $('status').innerHTML =
    '<div>WiFi</div><div>' + (s.wifi.connected ? (s.wifi.ssid + ' · ' + s.wifi.rssi + ' dBm · ' + s.wifi.ip) : (s.wifi.ap ? 'AP mode · ' + s.wifi.ip : 'disconnected')) + '</div>' +
    '<div>Time</div><div>' + (s.timeValid ? fmtDateTime(s.now) : 'not set') + '</div>' +
    '<div>Next sunrise</div><div>' + fmtDateTime(s.sun.nextSunrise) + '</div>' +
    '<div>Next sunset</div><div>' + fmtDateTime(s.sun.nextSunset) + '</div>' +
    '<div>DS3231</div><div>' + (s.rtcOk ? ('OK · ' + s.rtcTemp.toFixed(1) + ' °C') : 'not found') + '</div>' +
    '<div>Hostname</div><div>' + s.config.hostname + '.local</div>';
}

function populateSettings(s){
  $('lat').value = s.config.lat || '';
  $('lon').value = s.config.lon || '';
  $('camBefore').value = s.config.camBefore;
  $('camAfter').value = s.config.camAfter;
  $('lampBefore').value = s.config.lampBefore;
  $('lampAfter').value = s.config.lampAfter;
  $('manualMinutes').value = s.config.manualMinutes;
  $('lampFollowsCamera').checked = s.config.lampFollowsCamera;
  $('dayMode').checked = s.config.dayMode;
  $('camAlwaysOn').checked = s.config.camAlwaysOn;
  $('hostname').value = s.config.hostname;
}

async function night(){
  try{
    const mins = last ? last.config.manualMinutes : 30;
    await post('/api/control', {cam:'on', lamp:'on', minutes:mins});
    toast('Night view on for ' + mins + ' min', 'ok');
    getStatus();
  }catch(e){ toast('Failed: ' + e.message, 'bad'); }
}
async function alloff(){
  try{
    const mins = last ? last.config.manualMinutes : 30;
    await post('/api/control', {cam:'off', lamp:'off', minutes:mins});
    toast('All outputs off', 'ok');
    getStatus();
  }catch(e){ toast('Failed: ' + e.message, 'bad'); }
}
async function autoMode(){
  try{
    await post('/api/control', {cam:'auto', lamp:'auto'});
    toast('Back to automatic', 'ok');
    getStatus();
  }catch(e){ toast('Failed: ' + e.message, 'bad'); }
}
async function setCam(v){
  try{
    const mins = last ? last.config.manualMinutes : 30;
    await post('/api/control', {cam:v, minutes:mins});
    toast('Camera ' + v, 'ok');
    getStatus();
  }catch(e){ toast('Failed: ' + e.message, 'bad'); }
}
async function setLamp(v){
  try{
    const mins = last ? last.config.manualMinutes : 30;
    await post('/api/control', {lamp:v, minutes:mins});
    toast('Lamp ' + v, 'ok');
    getStatus();
  }catch(e){ toast('Failed: ' + e.message, 'bad'); }
}

async function save(){
  const lat = parseFloat($('lat').value);
  const lon = parseFloat($('lon').value);
  if(!isFinite(lat) || !isFinite(lon) || Math.abs(lat) > 90 || Math.abs(lon) > 180){
    toast('Enter a valid latitude and longitude', 'bad');
    return;
  }
  const data = {
    lat: lat, lon: lon,
    camBefore: $('camBefore').value,
    camAfter: $('camAfter').value,
    lampBefore: $('lampBefore').value,
    lampAfter: $('lampAfter').value,
    manualMinutes: $('manualMinutes').value,
    lampFollowsCamera: $('lampFollowsCamera').checked ? '1' : '0',
    dayMode: $('dayMode').checked ? '1' : '0',
    camAlwaysOn: $('camAlwaysOn').checked ? '1' : '0',
    hostname: $('hostname').value.trim(),
    ssid: $('ssid').value,
    pass: $('pass').value
  };
  try{
    const r = await post('/api/settings', data);
    $('saveMsg').textContent = r.reboot ? 'Saved. Rebooting…' : 'Saved.';
    toast(r.reboot ? 'Saved. Rebooting…' : 'Settings saved', 'ok');
    $('ssid').value = ''; $('pass').value = '';
    settingsLoaded = false;
    if(r.reboot) setTimeout(()=>{ $('conn').textContent='rebooting…'; }, 700);
    else getStatus();
  }catch(e){ toast('Save failed: ' + e.message, 'bad'); }
}

async function syncTime(){
  try{
    await post('/api/settime', {epoch: Math.floor(Date.now()/1000)});
    toast('Clock set from this device', 'ok');
    getStatus();
  }catch(e){ toast('Failed: ' + e.message, 'bad'); }
}

async function reboot(){
  if(!confirm('Reboot the controller?')) return;
  try{
    await post('/api/reboot', {});
    toast('Rebooting…', 'ok');
    $('conn').textContent = 'rebooting…';
    $('conn').className = 'pill warn';
  }catch(e){ toast('Failed: ' + e.message, 'bad'); }
}

getStatus();
setInterval(getStatus, 4000);
</script>
</body>
</html>
)=====";
