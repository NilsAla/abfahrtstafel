'use strict';

const $ = (id) => document.getElementById(id);
const TABS = ['status', 'einstellungen', 'update', 'protokoll'];
const WIFI_STATE = {
  online: 'verbunden',
  connecting: 'Verbindung wird aufgebaut …',
  fallback: 'nicht verbunden – Einrichtungs-WLAN ist offen',
  provisioning: 'noch nicht eingerichtet',
};

let current = null;
let statusTimer = null;
let logTimer = null;
let logLast = 0;

// --- Hilfsfunktionen -------------------------------------------------------

async function api(path, options = {}) {
  const res = await fetch(path, { cache: 'no-store', ...options });
  $('offline').hidden = true;
  return res;
}

function offline() {
  $('offline').hidden = false;
}

function signalText(rssi) {
  if (rssi >= -60) return 'sehr gut';
  if (rssi >= -70) return 'gut';
  if (rssi >= -80) return 'schwach';
  return 'sehr schwach';
}

function duration(seconds) {
  const d = Math.floor(seconds / 86400);
  const h = Math.floor((seconds % 86400) / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  if (d > 0) return `${d} T ${h} Std`;
  if (h > 0) return `${h} Std ${m} Min`;
  return `${m} Min`;
}

function kb(bytes) {
  return `${Math.round(bytes / 1024).toLocaleString('de-DE')} KB`;
}

function setText(id, text) {
  $(id).textContent = text;
}

// --- Tabs ------------------------------------------------------------------

function showTab() {
  const name = TABS.includes(location.hash.slice(1)) ? location.hash.slice(1) : 'status';
  for (const t of TABS) $(`tab-${t}`).hidden = t !== name;
  for (const a of document.querySelectorAll('nav a')) a.classList.toggle('active', a.dataset.tab === name);

  clearInterval(statusTimer);
  clearInterval(logTimer);
  if (name === 'status' || name === 'update') {
    loadStatus();
    statusTimer = setInterval(loadStatus, 5000);
  }
  if (name === 'einstellungen') loadConfig();
  if (name === 'protokoll') {
    loadLog();
    logTimer = setInterval(() => { if ($('log-follow').checked) loadLog(); }, 3000);
  }
}

// --- Status ----------------------------------------------------------------

async function loadStatus() {
  let s;
  try {
    s = await (await api('/api/status')).json();
  } catch (e) {
    offline();
    return;
  }
  const w = s.wifi;
  let wifi = WIFI_STATE[w.state] || w.state;
  if (w.state === 'online') wifi = `verbunden mit „${w.ssid}“ (Signal ${signalText(w.rssi)}, ${w.rssi} dBm)`;
  setText('st-wifi', wifi);
  setText('st-time', s.time.synced ? s.time.local.replace('T', ' ').slice(0, 19) : 'noch nicht gestellt (wartet auf Internet)');
  setText('st-addr', `http://${s.hostname}.local · ${w.ip || '–'}`);
  setText('st-version', s.version);
  setText('up-version', s.version);
  setText('st-uptime', duration(s.uptime_s));
  setText('st-reset', s.reset_reason);
  setText('st-fetch', s.last_fetch ? s.last_fetch : 'noch keine (Haltestellen folgen in einer späteren Version)');
  setText('st-heap', `${kb(s.heap.free)} frei, min. ${kb(s.heap.min_free)}, größter Block ${kb(s.heap.largest_block)}`);
  setText('st-psram', s.psram.total ? `${kb(s.psram.free)} frei von ${kb(s.psram.total)}` : 'nicht erkannt');
  setText('st-part', s.partition);
  setText('st-mac', w.mac);
}

// --- Einstellungen ---------------------------------------------------------

const form = $('cfg');

function fillForm(cfg) {
  current = cfg;
  for (const [key, value] of Object.entries(cfg)) {
    const el = form.elements[key];
    if (!el) continue;
    if (el.type === 'checkbox') el.checked = value;
    else el.value = value;
  }
  updateOutputs();
}

function updateOutputs() {
  setText('brightness-out', `${form.elements.brightness.value} %`);
  const nb = Number(form.elements.night_brightness.value);
  setText('night_brightness-out', nb === 0 ? 'aus' : `${nb} %`);
  setText('hostname-preview', form.elements.hostname.value.trim().toLowerCase() || '…');
  const night = form.elements.night_enabled.checked;
  for (const k of ['night_start', 'night_end', 'night_brightness']) form.elements[k].disabled = !night;
}

function showErrors(errors) {
  for (const p of form.querySelectorAll('.err')) p.textContent = errors[p.dataset.for] || '';
  for (const el of form.elements) {
    if (el.name) el.setAttribute('aria-invalid', errors[el.name] ? 'true' : 'false');
  }
}

async function loadConfig() {
  setText('cfg-msg', '');
  showErrors({});
  try {
    fillForm(await (await api('/api/config')).json());
  } catch (e) {
    offline();
  }
}

form.addEventListener('input', updateOutputs);

form.addEventListener('submit', async (ev) => {
  ev.preventDefault();
  const body = {};
  for (const key of Object.keys(current || {})) {
    const el = form.elements[key];
    if (!el) continue;
    if (el.type === 'checkbox') body[key] = el.checked;
    else if (el.type === 'range') body[key] = Number(el.value);
    else body[key] = el.value;
  }
  const button = form.querySelector('button[type="submit"]');
  button.disabled = true;
  setText('cfg-msg', 'Wird gespeichert …');
  try {
    const res = await api('/api/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
    const data = await res.json();
    if (data.ok) {
      showErrors({});
      fillForm(data.config);
      setText('cfg-msg', 'Gespeichert ✓');
      $('restart-box').hidden = !data.restart_required;
    } else {
      showErrors(data.errors || {});
      setText('cfg-msg', data.message || 'Bitte die markierten Felder prüfen.');
    }
  } catch (e) {
    offline();
    setText('cfg-msg', 'Speichern fehlgeschlagen.');
  } finally {
    button.disabled = false;
  }
});

$('restart-btn').addEventListener('click', async () => {
  $('restart-btn').disabled = true;
  try {
    await api('/api/restart', { method: 'POST' });
  } catch (e) {
    // Gerät startet ggf. schon neu
  }
  const host = form.elements.hostname.value.trim().toLowerCase();
  $('restart-box').textContent = `Startet neu … Die Seite ist gleich unter http://${host}.local erreichbar.`;
  setTimeout(() => { location.href = `http://${host}.local/#einstellungen`; }, 10000);
});

// --- Protokoll -------------------------------------------------------------

async function loadLog() {
  let res;
  try {
    res = await api(`/api/log?since=${logLast}`);
  } catch (e) {
    offline();
    return;
  }
  const text = await res.text();
  const last = Number(res.headers.get('X-Log-Last') || 0);
  const pre = $('log');
  if (last < logLast) {
    // Gerät wurde neu gestartet → von vorn laden
    pre.textContent = '';
    logLast = 0;
    return loadLog();
  }
  if (res.headers.get('X-Log-Gap') === '1' && logLast > 0) pre.textContent += '… (ältere Zeilen übersprungen)\n';
  logLast = last;
  if (!text) return;
  const atBottom = pre.scrollTop + pre.clientHeight >= pre.scrollHeight - 20;
  pre.textContent += text;
  if (atBottom) pre.scrollTop = pre.scrollHeight;
}

$('log-copy').addEventListener('click', async () => {
  try {
    await navigator.clipboard.writeText($('log').textContent);
    $('log-copy').textContent = 'Kopiert ✓';
  } catch (e) {
    // Zwischenablage braucht HTTPS oder localhost → Text markieren als Ersatz
    const range = document.createRange();
    range.selectNodeContents($('log'));
    getSelection().removeAllRanges();
    getSelection().addRange(range);
    $('log-copy').textContent = 'Markiert – mit Strg+C kopieren';
  }
  setTimeout(() => { $('log-copy').textContent = 'Kopieren'; }, 3000);
});

window.addEventListener('hashchange', showTab);
showTab();
