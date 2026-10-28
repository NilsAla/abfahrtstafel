'use strict';

const $ = (id) => document.getElementById(id);
const TABS = ['status', 'einstellungen', 'update', 'protokoll'];
const WIFI_STATE = {
  online: 'verbunden',
  connecting: 'Verbindung wird aufgebaut …',
  fallback: 'nicht verbunden – Einrichtungs-WLAN ist offen',
  provisioning: 'noch nicht eingerichtet',
};

const UPDATE_STATE = {
  idle: '',
  checking: 'Suche nach Updates …',
  up_to_date: 'Die Firmware ist aktuell.',
  available: 'Ein Update ist verfügbar.',
  downloading: 'Update wird heruntergeladen – bitte nicht den Strom trennen.',
  verifying: 'Update wird geprüft …',
  rebooting: 'Neustart … die Seite lädt gleich neu.',
};

let current = null;
let statusTimer = null;
let logTimer = null;
let updateTimer = null;
let logLast = 0;
let updateRebooting = false;

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
  clearInterval(updateTimer);
  if (name === 'status') {
    loadStatus();
    statusTimer = setInterval(loadStatus, 5000);
  }
  if (name === 'update') {
    loadUpdate();
    updateTimer = setInterval(loadUpdate, 2000);
  }
  if (name === 'einstellungen') loadConfig();
  if (name === 'protokoll') {
    loadLog();
    logTimer = setInterval(() => { if ($('log-follow').checked) loadLog(); }, 3000);
  }
}

// --- Status ----------------------------------------------------------------

async function loadStatus() {
  loadBoard();
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
  setText('st-fetch', s.last_fetch ? s.last_fetch.replace('T', ' ').slice(0, 19) : 'noch keine');
  setText('st-heap', `${kb(s.heap.free)} frei, min. ${kb(s.heap.min_free)}, größter Block ${kb(s.heap.largest_block)}`);
  setText('st-psram', s.psram.total ? `${kb(s.psram.free)} frei von ${kb(s.psram.total)}` : 'nicht erkannt');
  setText('st-part', s.partition);
  setText('st-mac', w.mac);
  try {
    const u = await (await api('/api/update')).json();
    setText('st-update', u.offer ? `Version ${u.offer.version} verfügbar – siehe Reiter „Update“`
      : u.state === 'up_to_date' ? 'aktuell' : (u.auto_check ? 'wird täglich geprüft' : 'automatische Prüfung aus'));
  } catch (e) {
    // Status bleibt auch ohne Update-Info nutzbar
  }
}

// --- Update ----------------------------------------------------------------

async function loadUpdate() {
  let u;
  try {
    u = await (await api('/api/update')).json();
  } catch (e) {
    // Während des Neustarts ist das Gerät kurz weg – das ist erwartet
    if (updateRebooting) {
      $('offline').hidden = true;
      setText('up-state', 'Neustart … die Seite lädt gleich neu.');
    } else {
      offline();
    }
    return;
  }

  if (updateRebooting && u.state !== 'rebooting') updateRebooting = false;
  setText('up-version', u.current);

  const last = $('up-last');
  if (u.last_update) {
    last.hidden = false;
    last.className = u.last_update.result === 'ok' ? 'banner ok' : 'banner error';
    last.textContent = u.last_update.result === 'ok'
      ? `Update auf Version ${u.last_update.version} erfolgreich installiert.`
      : `Das Update auf Version ${u.last_update.version} hat nicht geklappt. Die bisherige Version läuft weiter.`;
  } else {
    last.hidden = true;
  }

  let text = u.state === 'error' ? u.error : (UPDATE_STATE[u.state] ?? u.state);
  if (u.skipped && u.state === 'up_to_date') {
    text = `Version ${u.skipped} wird nicht angeboten, weil sie auf diesem Gerät nicht funktioniert hat. Es gibt noch keine neuere.`;
  }
  if (u.selftest_running) text = 'Neue Firmware wird geprüft (Selbsttest) …';
  setText('up-state', text);

  $('up-offer').hidden = !u.offer;
  if (u.offer) {
    setText('up-new', u.offer.version);
    setText('up-notes', u.offer.notes || '');
  }

  const loading = u.state === 'downloading' || u.state === 'verifying' || u.state === 'rebooting';
  $('up-progress').hidden = !loading;
  $('up-bar').value = u.percent;
  setText('up-pct', `${u.percent} %`);

  $('up-check').disabled = u.busy;
  $('up-install').hidden = !u.offer;
  $('up-install').disabled = u.busy || u.selftest_running;
  if (u.state === 'rebooting') updateRebooting = true;
}

async function postUpdate(path) {
  try {
    const res = await api(path, { method: 'POST' });
    const data = await res.json();
    if (!data.ok) setText('up-state', data.message);
  } catch (e) {
    offline();
  }
  loadUpdate();
}

$('up-check').addEventListener('click', () => postUpdate('/api/update/check'));

$('up-install').addEventListener('click', () => {
  const version = $('up-new').textContent;
  if (!confirm(`Update auf Version ${version} installieren?\n\nDas dauert etwa eine Minute, danach startet die Tafel neu.`)) return;
  postUpdate('/api/update/install');
});

// --- Einstellungen ---------------------------------------------------------

const form = $('cfg');

function fillForm(cfg) {
  current = cfg;
  setStops(cfg.stops || []);
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
    else if (typeof current[key] === 'number') body[key] = Number(el.value);  // auch Radiogruppen
    else body[key] = el.value;
  }
  body.stops = collectStops();
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

// --- Abfahrten (Vorschau) --------------------------------------------------

const MODE_LABEL = { train: 'Zug', subway: 'U-Bahn', tram: 'Straßenbahn', bus: 'Bus', ferry: 'Fähre', other: 'Sonstige' };
const MODES = Object.keys(MODE_LABEL);

// Kleiner DOM-Baukasten: h('div', {class: 'x', onclick: fn}, kind1, 'Text', …)
function h(tag, attrs = {}, ...children) {
  const el = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (v === null || v === undefined || v === false) continue;
    if (k.startsWith('on')) el.addEventListener(k.slice(2), v);
    else if (k === 'class') el.className = v;
    else if (k === 'style') el.setAttribute('style', v);
    else if (k in el && typeof v !== 'string') el[k] = v;
    else el.setAttribute(k, v === true ? '' : v);
  }
  for (const c of children.flat()) {
    if (c === null || c === undefined || c === false) continue;
    el.append(c instanceof Node ? c : document.createTextNode(String(c)));
  }
  return el;
}

function badge(line, mode, color, textColor) {
  const style = color ? `background:${color};color:${textColor || '#fff'}` : null;
  return h('span', { class: `badge ${mode || ''}`, style }, line || '?');
}

function ageText(seconds) {
  const m = Math.round(seconds / 60);
  return m < 1 ? 'gerade eben' : m < 60 ? `vor ${m} min` : `vor ${Math.floor(m / 60)} Std ${m % 60} min`;
}

// --- Nachbildung der Tafel (gleiche Regeln wie das Display ab M4) ----------

function scaleTafel() {
  const wrap = $('tafel-wrap');
  if (wrap.clientWidth) $('tafel').style.transform = `scale(${wrap.clientWidth / 820})`;
}
new ResizeObserver(scaleTafel).observe($('tafel-wrap'));

function hhmmMinus(now, seconds) {
  // Uhrzeit des Datenstands aus Gerätezeit und Datenalter
  const [hh, mm] = now.split(':').map(Number);
  const t = ((hh * 60 + mm - Math.round(seconds / 60)) % 1440 + 1440) % 1440;
  return `${String(Math.floor(t / 60)).padStart(2, '0')}:${String(t % 60).padStart(2, '0')}`;
}

// Ab dieser Verspätung: Minuten rot, rechts Planzeit durchgestrichen + neue Zeit (wie kLateMinutes im Display)
const LATE_MIN = 2;

function tafelRow(r) {
  const min = r.cancelled ? r.scheduled
    : r.minutes <= 0 ? 'jetzt'
      : r.minutes < 60 ? [String(r.minutes), h('small', {}, 'min')] : r.time;
  const where = r.track ? `${r.mode === 'bus' ? 'Steig' : 'Gleis'} ${r.track}` : '';
  const late = !r.cancelled && r.realtime && r.delay >= LATE_MIN;
  let info;
  if (r.cancelled) info = h('span', { class: 't-cancel' }, 'fällt', h('br'), 'aus');
  else if (late) info = [h('span', { class: 't-plan' }, r.scheduled), h('span', { class: 't-late' }, r.time)];
  else info = where;
  const style = r.color ? `background:${r.color};color:${r.text_color || '#fff'}` : null;
  return h('div', { class: `t-row${r.cancelled ? ' cancelled' : ''}${late ? ' late' : ''}` },
    h('div', { class: 't-min' }, min),
    h('div', { class: `t-badge ${r.mode}`, style }, r.line),
    h('div', { class: 't-to' }, r.to),
    h('div', { class: 't-info' }, info));
}

// Kopfzeile wie am Display: Haltestelle (normal, groß oder ausgeblendet), Datum, Uhr
function tafelHead(b, name, style, extra, withClock) {
  if (style === 'off') {
    return h('div', { class: 't-head bare' }, h('div', { class: 't-stop' }, b.date_long || ''), extra,
      withClock ? h('div', { class: 't-clock' }, b.now || '--:--') : null);
  }
  const cls = style === 'large' ? ' large' : style === 'small' ? ' small' : '';
  return h('div', { class: `t-head${cls}` }, h('div', { class: 't-stop' }, name), extra,
    withClock && b.date ? h('div', { class: 't-date' }, b.date) : null,
    withClock ? h('div', { class: 't-clock' }, b.now || '--:--') : null);
}

function renderTafel(b) {
  const t = $('tafel');
  const d = b.display || {};
  // Bei zwei Haltestellen bleibt der Name sichtbar, damit klar ist, welche gezeigt wird
  const style = d.stop_name === 'off' && b.stops?.length > 1 ? 'normal' : (d.stop_name || 'normal');
  t.replaceChildren();
  const night = d.theme === 'night';
  t.classList.toggle('night', night);
  $('tafel-wrap').classList.toggle('night', night);
  t.classList.toggle('off', d.backlight === 0);
  t.classList.toggle('rows3', d.rows === 3);
  renderThemeToggle(d);
  if (b.status !== 'ok' || !b.stops.length) {
    t.classList.remove('t-two');
    t.append(tafelHead(b, 'Abfahrtstafel', style, null, true),
      h('div', { class: 't-msg' }, b.text || 'Noch keine Haltestelle eingerichtet.'));
    return;
  }
  // Wie am Display: untereinander je 2 Abfahrten oder im Wechsel alle 10 s
  let stops = b.stops;
  let page = null;
  if (stops.length > 1 && d.two_stops === 'alternate') {
    const i = Math.floor(Date.now() / 10000) % stops.length;
    stops = [stops[i]];
    page = h('div', { class: 't-page' }, `${i + 1}/${b.stops.length}`);
  }
  const two = stops.length > 1;
  t.classList.toggle('t-two', two);
  const stale = stops.find((s) => s.stale && s.rows.length);
  const perStop = two ? 2 : (d.rows || 4) - (stale ? 1 : 0);
  stops.forEach((stop, i) => {
    t.append(tafelHead(b, stop.name, two ? 'small' : style, i === 0 ? page : null, i === 0));
    if (!stop.rows.length) {
      t.append(h('div', { class: 't-msg' }, stop.text || 'Zurzeit keine Abfahrten.'));
      return;
    }
    for (const r of stop.rows.slice(0, perStop)) t.append(tafelRow(r));
  });
  if (stale && b.now) {
    t.append(h('div', { class: 't-banner' },
      `${stale.offline ? 'Keine WLAN-Verbindung' : 'Fahrplandaten nicht erreichbar'} · Stand ${hhmmMinus(b.now, stale.age_s)} Uhr`));
  }
  scaleTafel();
}

// Schneller Umschalter Dunkel/Nacht über der Vorschau: speichert sofort
let themeBusy = false;

function renderThemeToggle(d) {
  if (themeBusy || !d.theme_setting) return;
  for (const r of document.querySelectorAll('input[name="theme-toggle"]')) r.checked = r.value === d.theme_setting;
  let hint = '';
  if (d.backlight === 0) hint = `Nachtzeit – Display ist bis ${d.night_end} Uhr aus.`;
  else if (d.night) hint = `Nachtzeit – bis ${d.night_end} Uhr gilt der Nacht-Stil.`;
  setText('theme-hint', hint);
}

for (const r of document.querySelectorAll('input[name="theme-toggle"]')) {
  r.addEventListener('change', async () => {
    themeBusy = true;
    try {
      const res = await api('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ theme: r.value }),
      });
      const data = await res.json();
      if (!data.ok) setText('theme-hint', data.message || 'Speichern fehlgeschlagen.');
    } catch (e) {
      offline();
    } finally {
      themeBusy = false;
      loadBoard();
    }
  });
}

async function loadBoard() {
  let b;
  try {
    b = await (await api('/api/departures')).json();
  } catch (e) {
    return;
  }
  renderTafel(b);
  const box = $('board');
  box.replaceChildren();
  if (b.status !== 'ok') {
    box.append(h('p', { class: 'hint' }, b.text,
      b.status === 'no_stops' ? ' Im Reiter „Einstellungen“ eine Haltestelle hinzufügen.' : ''));
    return;
  }
  for (const stop of b.stops) {
    box.append(h('h3', {}, stop.name));
    if (stop.stale) {
      box.append(h('p', { class: 'hint' }, `Stand: ${ageText(stop.age_s)}`,
        stop.offline ? ' (keine WLAN-Verbindung)' : ' (Fahrplandaten nicht erreichbar)'));
    }
    if (!stop.rows.length) {
      box.append(h('p', { class: 'hint' }, stop.text));
      continue;
    }
    for (const r of stop.rows) {
      // Links Planzeit, darunter bei Abweichung die echte Zeit; rechts die Minuten bis zur Abfahrt
      const changed = r.realtime && !r.cancelled && r.time !== r.scheduled;
      const time = h('span', { class: 'dep-time' },
        h('span', { class: 'dep-plan' }, r.scheduled),
        changed ? h('span', { class: r.delay > 0 ? 'dep-late' : 'dep-early' }, r.time) : null);
      const mins = r.cancelled ? 'fällt aus'
        : r.minutes <= 0 ? 'jetzt'
          : r.minutes < 60 ? `${r.minutes} min` : `${Math.floor(r.minutes / 60)} h ${r.minutes % 60} min`;
      box.append(h('div', { class: `dep${r.cancelled ? ' cancelled' : ''}`, title: r.realtime ? 'mit Echtzeitdaten' : 'laut Fahrplan (keine Echtzeitdaten)' },
        time,
        badge(r.line, r.mode, r.color, r.text_color),
        h('span', { class: 'dep-mid' },
          h('span', { class: 'dep-to' }, r.to),
          r.track ? h('span', { class: 'dep-track' }, `Gl. ${r.track}`) : null),
        h('span', { class: 'dep-min' }, mins)));
    }
  }
}

// --- Haltestellen (Einstellungen) ------------------------------------------

let stops = [];

function setStops(list) {
  stops = list.map((s) => ({ ...s, ui: { dirs: null, search: null } }));
  renderStops();
}

function collectStops() {
  return stops.filter((s) => s.id).map((s) => ({
    id: s.id, name: s.name, town: s.town || '', filter: s.filter, modes: s.modes,
    select: s.select, walk: Number(s.walk) || 0,
  }));
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// Asynchrone Abfragen des Geräts: so lange wiederholen, bis das Ergebnis da ist
async function poll(url, firstUrl = url) {
  let next = firstUrl;
  for (let i = 0; i < 60; i++) {
    const r = await (await api(next)).json();
    if (r.state !== 'pending') return r;
    next = url;
    await sleep(750);
  }
  return { state: 'error', message: 'Keine Antwort von den Fahrplandaten. Bitte später erneut versuchen.' };
}

function sameSel(a, b) {
  return a.route === b.route && (a.dir ?? null) === (b.dir ?? null);
}

function isSelected(stop, opt) {
  return stop.select.some((s) => sameSel(s, opt));
}

function toggleSel(stop, opt, on) {
  stop.select = stop.select.filter((s) => !sameSel(s, opt));
  if (on) stop.select.push({ route: opt.route, dir: opt.dir ?? null, line: opt.line, to: opt.to });
}

async function searchStop(stop) {
  const q = (stop.ui.query || '').trim();
  stop.ui.search = { state: 'pending' };
  renderStops();
  try {
    stop.ui.search = await poll(`/api/stops?q=${encodeURIComponent(q)}`);
  } catch (e) {
    stop.ui.search = { state: 'error', message: 'Gerät nicht erreichbar.' };
  }
  renderStops();
}

async function loadDirections(stop, more = false) {
  const base = `/api/directions?stop=${encodeURIComponent(stop.id)}`;
  stop.ui.dirs = { ...(stop.ui.dirs || {}), state: 'pending' };
  renderStops();
  try {
    stop.ui.dirs = await poll(base, more ? `${base}&more=1` : base);
  } catch (e) {
    stop.ui.dirs = { state: 'error', message: 'Gerät nicht erreichbar.' };
  }
  renderStops();
}

function renderSearch(stop) {
  const ui = stop.ui;
  const input = h('input', {
    type: 'text', placeholder: 'z. B. Emsdetten Bahnhof', value: ui.query || '', autocomplete: 'off',
    oninput: (e) => { ui.query = e.target.value; },
    onkeydown: (e) => { if (e.key === 'Enter') { e.preventDefault(); searchStop(stop); } },
  });
  const parts = [h('div', { class: 'search-row' }, input, h('button', { type: 'button', onclick: () => searchStop(stop) }, 'Suchen'))];
  const res = ui.search;
  if (res?.state === 'pending') parts.push(h('p', { class: 'hint' }, 'Suche läuft …'));
  if (res?.state === 'error') parts.push(h('p', { class: 'err' }, res.message));
  if (res?.state === 'done') {
    if (!res.results.length) parts.push(h('p', { class: 'hint' }, 'Nichts gefunden. Anders schreiben, z. B. mit Ort.'));
    parts.push(h('ul', { class: 'results' }, res.results.map((m) => h('li', {},
      h('button', { type: 'button', onclick: () => {
        Object.assign(stop, { id: m.id, name: m.name, town: m.town, filter: 'all', modes: [...MODES], select: [], walk: 0 });
        stop.ui = { dirs: null, search: null };
        renderStops();
      } }, m.name, h('small', {}, [m.town, m.region, m.country].filter(Boolean).join(' · ')))))));
  }
  return parts;
}

function renderDirections(stop) {
  const d = stop.ui.dirs;
  const parts = [];
  if (!d || d.state === 'idle') {
    parts.push(h('button', { type: 'button', onclick: () => loadDirections(stop) }, 'Linien und Richtungen laden'));
  }
  if (d?.state === 'pending') parts.push(h('p', { class: 'hint' }, 'Fahrplan wird geladen …'));
  if (d?.state === 'error') {
    parts.push(h('p', { class: 'err' }, d.message), h('button', { type: 'button', onclick: () => loadDirections(stop) }, 'Erneut versuchen'));
  }

  const known = [];
  if (d?.state === 'done') {
    parts.push(h('p', { class: 'hint' }, `Gefunden in den nächsten ${d.departures} Abfahrten${d.until ? ` (bis ${d.until} Uhr)` : ''}. `
      + 'Neue Linien bleiben ausgeblendet, bis du sie hier auswählst.'));
    for (const g of d.groups) {
      known.push(...g.options);
      const on = g.options.filter((o) => isSelected(stop, o)).length;
      const groupBox = h('input', { type: 'checkbox', checked: on === g.options.length,
        onchange: (e) => { for (const o of g.options) toggleSel(stop, o, e.target.checked); renderStops(); } });
      groupBox.indeterminate = on > 0 && on < g.options.length;
      const lines = [...new Set(g.options.map((o) => o.line))];
      const row = h('div', { class: 'dir-group' },
        h('label', { class: 'check' }, groupBox, 'Richtung ', h('strong', {}, g.to),
          lines.map((l) => badge(l, g.options.find((o) => o.line === l).mode))));
      if (g.options.length > 1) {
        row.append(h('details', { open: on > 0 && on < g.options.length },
          h('summary', {}, 'einzelne Linien wählen'),
          g.options.map((o) => h('label', { class: 'check' },
            h('input', { type: 'checkbox', checked: isSelected(stop, o), onchange: (e) => { toggleSel(stop, o, e.target.checked); renderStops(); } }),
            badge(o.line, o.mode), ` → ${o.to} (${o.count}×)`))));
      }
      parts.push(row);
    }
    if (d.can_more) {
      parts.push(h('button', { type: 'button', class: 'link-btn', onclick: () => loadDirections(stop, true) },
        'Weiter in die Zukunft suchen (seltene Linien, Nachtbusse)'));
    }
  }

  // Gespeicherte Auswahl, die gerade nicht im geladenen Fahrplan vorkommt
  const missing = stop.select.filter((s) => !known.some((o) => sameSel(o, s)));
  if (missing.length) {
    parts.push(h('p', { class: 'hint' }, d?.state === 'done' ? 'Ausgewählt, aber gerade nicht im Fahrplan:' : 'Ausgewählt:'));
    for (const s of missing) {
      parts.push(h('label', { class: 'check' },
        h('input', { type: 'checkbox', checked: true, onchange: (e) => { toggleSel(stop, s, e.target.checked); renderStops(); } }),
        badge(s.line), ` → ${s.to}`));
    }
  }
  return parts;
}

function renderStops() {
  const box = $('stops');
  box.replaceChildren();
  stops.forEach((stop, i) => {
    const remove = h('button', { type: 'button', class: 'link-btn', onclick: () => { stops.splice(i, 1); renderStops(); } }, 'Entfernen');
    if (!stop.id) {
      box.append(h('div', { class: 'stop-card' }, h('div', { class: 'stop-head' }, h('strong', {}, 'Neue Haltestelle'), remove), renderSearch(stop)));
      return;
    }
    const name = `filter-${i}`;
    const radio = (value, label) => h('label', {},
      h('input', { type: 'radio', name, value, checked: stop.filter === value, onchange: () => {
        stop.filter = value;
        if (value === 'selected' && !stop.ui.dirs) loadDirections(stop);
        renderStops();
      } }), label);
    const card = h('div', { class: 'stop-card' },
      h('div', { class: 'stop-head' }, h('strong', {}, stop.name), h('span', { class: 'hint' }, stop.town || ''), remove),
      h('div', { class: 'radio-row' }, radio('all', 'Alle Abfahrten'), radio('selected', 'Nur bestimmte Richtungen')));
    if (stop.filter === 'all') {
      card.append(h('div', { class: 'modes' }, MODES.map((m) => h('label', {},
        h('input', { type: 'checkbox', checked: stop.modes.includes(m), onchange: (e) => {
          stop.modes = e.target.checked ? [...stop.modes, m] : stop.modes.filter((x) => x !== m);
        } }), MODE_LABEL[m]))));
    } else {
      card.append(...renderDirections(stop));
    }
    card.append(h('div', { class: 'walk-row' },
      h('label', { for: `walk-${i}` }, 'Fußweg zur Haltestelle'),
      h('input', { id: `walk-${i}`, type: 'number', min: 0, max: 30, value: stop.walk || 0,
        oninput: (e) => { stop.walk = e.target.value; } }), 'Minuten'));
    box.append(card);
  });
  $('stop-add').hidden = stops.length >= 2;
}

$('stop-add').addEventListener('click', () => {
  stops.push({ id: '', name: '', town: '', filter: 'all', modes: [...MODES], select: [], walk: 0, ui: { query: '', search: null, dirs: null } });
  renderStops();
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
