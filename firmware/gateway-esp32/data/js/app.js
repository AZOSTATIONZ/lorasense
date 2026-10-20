// LoRaSense dashboard + settings application
// Polls /api/state for dashboard, /api/config for settings

// =====================================================================
//  Phase 16: Dark mode toggle
// =====================================================================
var darkModePref = null;
try { darkModePref = localStorage.getItem('ls_dark'); } catch(e) {}

function isDarkActive() {
  if (darkModePref === 'dark') return true;
  if (darkModePref === 'light') return false;
  return window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
}

function applyDarkMode() {
  var dark = isDarkActive();
  if (dark) {
    document.documentElement.setAttribute('data-theme', 'dark');
  } else {
    document.documentElement.removeAttribute('data-theme');
  }
  updateDarkIcon(dark);
}

function updateDarkIcon(dark) {
  var btn = document.getElementById('dark-mode-btn');
  if (!btn) return;
  var sun = '<path d="M12 3v1m0 16v1m-7.07-2.93l.71.71M5.64 5.64l-.71.71M3 12h1m16 0h1m-2.93 7.07l-.71-.71M18.36 5.64l.71-.71"/><circle cx="12" cy="12" r="4"/>';
  var moon = '<path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"/>';
  var svg = btn.querySelector('svg');
  if (svg) svg.innerHTML = dark ? moon : sun;
}

function toggleDarkMode() {
  var dark = isDarkActive();
  darkModePref = dark ? 'light' : 'dark';
  try { localStorage.setItem('ls_dark', darkModePref); } catch(e) {}
  applyDarkMode();
}

applyDarkMode();

if (window.matchMedia) {
  try {
    window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', function() {
      if (!darkModePref) applyDarkMode();
    });
  } catch(e) {}
}

var pollTimer = null;
var historyTimer = null;
var currentView = 'dashboard';
var configData = null;

// Theme mapping: environment type -> CSS theme class
var THEME_MAP = {
  home: '', farm: '', greenhouse: '',
  industrial: 'theme-industrial',
  lab: 'theme-technical', custom: 'theme-technical'
};
var currentTheme = '';
var themeOverride = '';
try { themeOverride = localStorage.getItem('ls_theme') || ''; } catch(e) {}

function applyTheme(envType) {
  var cls;
  if (themeOverride === 'friendly') cls = '';
  else if (themeOverride === 'industrial') cls = 'theme-industrial';
  else if (themeOverride === 'technical') cls = 'theme-technical';
  else cls = THEME_MAP[envType] || '';
  if (cls === currentTheme) return;
  document.body.classList.remove('theme-industrial', 'theme-technical');
  if (cls) document.body.classList.add(cls);
  currentTheme = cls;
}

function setThemeOverride(val) {
  themeOverride = val;
  try { if (val) localStorage.setItem('ls_theme', val); else localStorage.removeItem('ls_theme'); } catch(e) {}
}

// SVG icon helpers — returns inline SVG markup, sized to 1em by default
function svgWrap(inner, cls) {
  return '<svg class="ls-icon' + (cls ? ' ' + cls : '') + '" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">' + inner + '</svg>';
}
var ICO = {
  thermometer: function(){ return svgWrap('<path d="M14 14.76V3.5a2.5 2.5 0 0 0-5 0v11.26a4.5 4.5 0 1 0 5 0z"/>'); },
  droplet:     function(){ return svgWrap('<path d="M12 2.69l5.66 5.66a8 8 0 1 1-11.31 0z"/>'); },
  gauge:       function(){ return svgWrap('<path d="M12 15a3 3 0 1 0 0-6 3 3 0 0 0 0 6z"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09a1.65 1.65 0 0 0-1.08-1.51 1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09a1.65 1.65 0 0 0 1.51-1.08 1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9c.26.604.852.997 1.51 1H21a2 2 0 0 1 0 4h-.09c-.658.003-1.25.396-1.51 1z"/>'); },
  sun:         function(){ return svgWrap('<circle cx="12" cy="12" r="5"/><line x1="12" y1="1" x2="12" y2="3"/><line x1="12" y1="21" x2="12" y2="23"/><line x1="4.22" y1="4.22" x2="5.64" y2="5.64"/><line x1="18.36" y1="18.36" x2="19.78" y2="19.78"/><line x1="1" y1="12" x2="3" y2="12"/><line x1="21" y1="12" x2="23" y2="12"/><line x1="4.22" y1="19.78" x2="5.64" y2="18.36"/><line x1="18.36" y1="5.64" x2="19.78" y2="4.22"/>'); },
  fan:         function(){ return svgWrap('<path d="M12 12c-1.5-3-4-5.5-7-5.5a5 5 0 0 0 0 10c1.5 0 3-.5 4-1.5"/><path d="M12 12c3-1.5 5.5-4 5.5-7a5 5 0 0 0-10 0c0 1.5.5 3 1.5 4"/><path d="M12 12c1.5 3 4 5.5 7 5.5a5 5 0 0 0 0-10c-1.5 0-3 .5-4 1.5"/><path d="M12 12c-3 1.5-5.5 4-5.5 7a5 5 0 0 0 10 0c0-1.5-.5-3-1.5-4"/><circle cx="12" cy="12" r="1.5" fill="currentColor"/>'); },
  pump:        function(){ return svgWrap('<rect x="2" y="8" width="8" height="12" rx="1"/><path d="M10 14h6a4 4 0 0 0 4-4V6"/><path d="M18 6l3 3"/><path d="M18 6l-3 3"/>'); },
  plug:        function(){ return svgWrap('<path d="M12 22v-5"/><path d="M9 7V2"/><path d="M15 7V2"/><rect x="6" y="7" width="12" height="5" rx="1"/><path d="M8 12v2a4 4 0 0 0 8 0v-2"/>'); },
  droplets:    function(){ return svgWrap('<path d="M7 16.3c2.2 0 4-1.83 4-4.05 0-1.16-.57-2.26-1.71-3.19S7.29 6.75 7 5.3c-.29 1.45-1.14 2.84-2.29 3.76S3 11.1 3 12.25c0 2.22 1.78 4.05 4 4.05z"/><path d="M12.56 14.1c1.56 0 2.82-1.28 2.82-2.84 0-.82-.4-1.59-1.2-2.24-.8-.65-1.33-1.39-1.62-2.22-.28 1-.84 1.93-1.62 2.58-.4.32-.7.72-.87 1.15"/>'); },
  flame:       function(){ return svgWrap('<path d="M8.5 14.5A2.5 2.5 0 0 0 11 12c0-1.38-.5-2-1-3-1.072-2.143-.224-4.054 2-6 .5 2.5 2 4.9 4 6.5 2 1.6 3 3.5 3 5.5a7 7 0 1 1-14 0c0-1.153.433-2.294 1-3a2.5 2.5 0 0 0 2.5 2.5z"/>'); },
  cog:         function(){ return svgWrap('<circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-2.82 1.18V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 7.4 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.18 14H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 7.4a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 10 4.18V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 2.82 1.09l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.82 10H21a2 2 0 0 1 0 4h-.09c-.66 0-1.25.4-1.51 1z"/>'); },
  zap:         function(){ return svgWrap('<polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"/>'); },
  signal:      function(){ return svgWrap('<path d="M2 20h.01"/><path d="M7 20v-4"/><path d="M12 20v-8"/><path d="M17 20V8"/><path d="M22 4v16"/>'); },
  wifi:        function(){ return svgWrap('<path d="M5 12.55a11 11 0 0 1 14.08 0"/><path d="M1.42 9a16 16 0 0 1 21.16 0"/><path d="M8.53 16.11a6 6 0 0 1 6.95 0"/><line x1="12" y1="20" x2="12.01" y2="20"/>'); },
  clock:       function(){ return svgWrap('<circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/>'); },
  home:        function(){ return svgWrap('<path d="M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/><polyline points="9 22 9 12 15 12 15 22"/>'); },
  sprout:      function(){ return svgWrap('<path d="M7 20h10"/><path d="M10 20c5.5-2.5.8-6.4 3-10"/><path d="M9.5 9.4c1.1.8 1.8 2.2 2.3 3.7-2 .4-3.5.4-4.8-.3-1.2-.6-2.3-1.9-3-4.2 2.8-.5 4.4 0 5.5.8z"/><path d="M14.1 6a7 7 0 0 0-1.1 4c1.9-.1 3.3-.6 4.3-1.4 1-1 1.6-2.3 1.7-4.6-2.7.1-4 1-4.9 2z"/>'); },
  leaf:        function(){ return svgWrap('<path d="M11 20A7 7 0 0 1 9.8 6.9C15.5 4.9 17 3.5 19 2c1 2 2 4.5 2 8 0 5.5-4.78 10-10 10z"/><path d="M2 21c0-3 1.85-5.36 5.08-6C9.5 14.52 12 13 13 12"/>'); },
  factory:     function(){ return svgWrap('<path d="M2 20V8l7 4V8l7 4V4h6v16z"/><path d="M17 20v-4h3v4"/>'); },
  flask:       function(){ return svgWrap('<path d="M9 3h6"/><path d="M10 9V3h4v6l5 10H5z"/><path d="M7 19h10"/>'); },
  shield:      function(){ return svgWrap('<path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/>'); },
  activity:    function(){ return svgWrap('<polyline points="22 12 18 12 15 21 9 3 6 12 2 12"/>'); },
  alertTri:    function(){ return svgWrap('<path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/>'); },
  wind:        function(){ return svgWrap('<path d="M9.59 4.59A2 2 0 1 1 11 8H2m10.59 11.41A2 2 0 1 0 14 16H2m15.73-8.27A2.5 2.5 0 1 1 19.5 12H2"/>'); }
};

// Comprehensive environment profiles
var PROFILES = {
  home: {
    terms: { zone: 'Room', zones: 'Rooms', sensor: 'Sensor', equipment: 'Device',
             addZone: 'Add Room', addEq: 'Add Device', noEq: 'No devices in this room',
             heroLabel: 'Home Comfort', wizZonePlaceholder: 'e.g. Living room' },
    heroOrder: ['temperature_c', 'humidity_pct', 'analog_pct'],
    catIcons: { lighting: ICO.sun, fan: ICO.fan, pump: ICO.pump, socket: ICO.plug, irrigation: ICO.droplets, heating: ICO.flame, other: ICO.cog },
    sensorIcons: { temperature_c: ICO.thermometer, humidity_pct: ICO.droplet, analog_pct: ICO.gauge },
    zoneSuggestions: ['Living room', 'Bedroom', 'Kitchen', 'Bathroom', 'Garage'],
    eqSuggestions: ['Ceiling light', 'Desk fan', 'Space heater', 'Air purifier'],
    presets: [
      { name: 'Comfort cooling', desc: 'Fan on when temp exceeds 28 °C', sensor: 0, cond: 1, thresh: 280, hyst: 10, cat: 'fan' },
      { name: 'Humidity alert', desc: 'Dehumidifier when humidity > 70%', sensor: 1, cond: 1, thresh: 700, hyst: 20, cat: 'other' }
    ]
  },
  farm: {
    terms: { zone: 'Field', zones: 'Fields', sensor: 'Sensor', equipment: 'Equipment',
             addZone: 'Add Field', addEq: 'Add Equipment', noEq: 'No equipment in this field',
             heroLabel: 'Field Conditions', wizZonePlaceholder: 'e.g. North paddock' },
    heroOrder: ['humidity_pct', 'temperature_c', 'analog_pct'],
    catIcons: { lighting: ICO.sun, fan: ICO.wind, pump: ICO.pump, socket: ICO.plug, irrigation: ICO.droplets, heating: ICO.flame, other: ICO.cog },
    sensorIcons: { temperature_c: ICO.thermometer, humidity_pct: ICO.droplet, analog_pct: ICO.sprout },
    zoneSuggestions: ['North paddock', 'South paddock', 'Barn', 'Well house'],
    eqSuggestions: ['Irrigation pump', 'Ventilation fan', 'Feed dispenser', 'Water heater'],
    presets: [
      { name: 'Irrigation trigger', desc: 'Pump on when soil moisture < 30%', sensor: 2, cond: 0, thresh: 300, hyst: 50, cat: 'irrigation' },
      { name: 'Heat protection', desc: 'Misting when temp > 35 °C', sensor: 0, cond: 1, thresh: 350, hyst: 20, cat: 'irrigation' }
    ]
  },
  greenhouse: {
    terms: { zone: 'Bay', zones: 'Bays', sensor: 'Sensor', equipment: 'Equipment',
             addZone: 'Add Bay', addEq: 'Add Equipment', noEq: 'No equipment in this bay',
             heroLabel: 'Climate Status', wizZonePlaceholder: 'e.g. Propagation bay' },
    heroOrder: ['temperature_c', 'humidity_pct', 'analog_pct'],
    catIcons: { lighting: ICO.sun, fan: ICO.wind, pump: ICO.pump, socket: ICO.plug, irrigation: ICO.droplets, heating: ICO.flame, other: ICO.leaf },
    sensorIcons: { temperature_c: ICO.thermometer, humidity_pct: ICO.droplet, analog_pct: ICO.sprout },
    zoneSuggestions: ['Propagation bay', 'Growing bay', 'Seedling area', 'Storage'],
    eqSuggestions: ['Exhaust fan', 'Water pump', 'Grow lights', 'Shade motor'],
    presets: [
      { name: 'Ventilation cycle', desc: 'Exhaust when temp > 30 °C', sensor: 0, cond: 1, thresh: 300, hyst: 15, cat: 'fan' },
      { name: 'Auto watering', desc: 'Irrigation when moisture < 40%', sensor: 2, cond: 0, thresh: 400, hyst: 50, cat: 'irrigation' },
      { name: 'Frost guard', desc: 'Heater on below 5 °C', sensor: 0, cond: 0, thresh: 50, hyst: 10, cat: 'heating' }
    ]
  },
  industrial: {
    terms: { zone: 'Area', zones: 'Areas', sensor: 'Instrument', equipment: 'Asset',
             addZone: 'Add Area', addEq: 'Add Asset', noEq: 'No assets in this area',
             heroLabel: 'Process Status', wizZonePlaceholder: 'e.g. Production floor' },
    heroOrder: ['temperature_c', 'analog_pct', 'humidity_pct'],
    catIcons: { lighting: ICO.sun, fan: ICO.fan, pump: ICO.pump, socket: ICO.zap, irrigation: ICO.droplets, heating: ICO.flame, other: ICO.factory },
    sensorIcons: { temperature_c: ICO.thermometer, humidity_pct: ICO.droplet, analog_pct: ICO.activity },
    zoneSuggestions: ['Production floor', 'Control room', 'Loading bay', 'Utility room'],
    eqSuggestions: ['Cooling system', 'Alarm relay', 'Exhaust blower', 'Conveyor motor'],
    presets: [
      { name: 'Over-temp alarm', desc: 'Alarm when temp > 45 °C', sensor: 0, cond: 1, thresh: 450, hyst: 20, cat: 'other' },
      { name: 'Process cooling', desc: 'Cooling on above 38 °C', sensor: 0, cond: 1, thresh: 380, hyst: 15, cat: 'fan' }
    ]
  },
  lab: {
    terms: { zone: 'Station', zones: 'Stations', sensor: 'Instrument', equipment: 'Apparatus',
             addZone: 'Add Station', addEq: 'Add Apparatus', noEq: 'No apparatus at this station',
             heroLabel: 'Lab Environment', wizZonePlaceholder: 'e.g. Analysis bench' },
    heroOrder: ['temperature_c', 'humidity_pct', 'analog_pct'],
    catIcons: { lighting: ICO.sun, fan: ICO.fan, pump: ICO.pump, socket: ICO.zap, irrigation: ICO.droplets, heating: ICO.flame, other: ICO.flask },
    sensorIcons: { temperature_c: ICO.thermometer, humidity_pct: ICO.droplet, analog_pct: ICO.activity },
    zoneSuggestions: ['Analysis bench', 'Fume hood', 'Clean room', 'Storage'],
    eqSuggestions: ['Fume hood fan', 'Equipment power', 'Chiller unit', 'UV lamp'],
    presets: [
      { name: 'Temp stability', desc: 'Alert if temp leaves 20–25 °C', sensor: 0, cond: 1, thresh: 250, hyst: 10, cat: 'other' },
      { name: 'Humidity control', desc: 'Dehumidifier above 55%', sensor: 1, cond: 1, thresh: 550, hyst: 20, cat: 'other' }
    ]
  },
  custom: {
    terms: { zone: 'Zone', zones: 'Zones', sensor: 'Sensor', equipment: 'Equipment',
             addZone: 'Add Zone', addEq: 'Add Equipment', noEq: 'No equipment in this zone',
             heroLabel: 'Sensor Readings', wizZonePlaceholder: 'e.g. Main area' },
    heroOrder: ['temperature_c', 'humidity_pct', 'analog_pct'],
    catIcons: { lighting: ICO.sun, fan: ICO.fan, pump: ICO.pump, socket: ICO.plug, irrigation: ICO.droplets, heating: ICO.flame, other: ICO.cog },
    sensorIcons: { temperature_c: ICO.thermometer, humidity_pct: ICO.droplet, analog_pct: ICO.gauge },
    zoneSuggestions: ['Main area', 'Secondary', 'Storage'],
    eqSuggestions: ['Output 1', 'Output 2'],
    presets: []
  }
};

var terms = PROFILES.home.terms;
var currentProfile = PROFILES.home;

// =====================================================================
//  Navigation
// =====================================================================
document.getElementById('settings-btn').onclick = function() { showView('settings'); };
document.getElementById('dark-mode-btn').onclick = toggleDarkMode;
document.getElementById('back-btn').onclick = function() { showView('dashboard'); };

function showView(view) {
  var prevView = currentView;
  currentView = view;
  var views = ['dashboard', 'settings', 'studio'];
  for (var v = 0; v < views.length; v++) {
    var el = document.getElementById('view-' + views[v]);
    if (views[v] === view) {
      el.style.display = '';
      el.classList.add('view-enter');
      el.offsetHeight; // force reflow
      el.classList.remove('view-enter');
      el.classList.add('view-active');
    } else {
      el.classList.remove('view-active');
      el.style.display = 'none';
    }
  }

  var qbar = document.getElementById('quick-bar');
  if (qbar && view !== 'dashboard') qbar.classList.remove('visible');

  if (view === 'settings') {
    clearInterval(pollTimer);
    clearInterval(historyTimer);
    loadConfig();
    loadActionLog();
    loadEvents();
    loadSystemInfo();
  } else if (view === 'studio') {
    clearInterval(pollTimer);
    clearInterval(historyTimer);
    loadStudio();
  } else {
    refresh();
    loadHistory();
    pollTimer = setInterval(refresh, 1000);
    historyTimer = setInterval(loadHistory, 30000);
  }
}

// =====================================================================
//  Dashboard
// =====================================================================
function formatUptime(sec) {
  if (sec < 60) return sec + 's';
  if (sec < 3600) return Math.floor(sec / 60) + 'm';
  return Math.floor(sec / 3600) + 'h ' + Math.floor((sec % 3600) / 60) + 'm';
}

function getCatIcon(cat) {
  var fn = currentProfile.catIcons[cat] || currentProfile.catIcons.other || ICO.cog;
  return fn();
}
function getSensorIcon(key) {
  var fn = currentProfile.sensorIcons[key] || ICO.gauge;
  return fn();
}

function signalLabel(rssi) {
  if (rssi >= -60) return 'Strong';
  if (rssi >= -80) return 'Good';
  if (rssi >= -100) return 'Weak';
  return 'No signal';
}

var sensorMeta = {
  1: { name: 'DHT11', readings: [
    { key: 'temperature_c', label: 'Temperature', unit: ' °C', decimals: 1 },
    { key: 'humidity_pct',  label: 'Humidity',    unit: ' %',  decimals: 0 }
  ]},
  2: { name: 'DHT22', readings: [
    { key: 'temperature_c', label: 'Temperature', unit: ' °C', decimals: 1 },
    { key: 'humidity_pct',  label: 'Humidity',    unit: ' %',  decimals: 0 }
  ]},
  5: { name: 'Analog', readings: [
    { key: 'analog_pct', label: 'Analog', unit: ' %', decimals: 1 }
  ]}
};

function formatAge(seconds) {
  if (seconds === undefined || seconds === null) return '';
  if (seconds < 60) return seconds + 's ago';
  if (seconds < 3600) return Math.floor(seconds / 60) + 'm ago';
  return Math.floor(seconds / 3600) + 'h ago';
}

function qualityBadge(q) {
  if (!q || q === 'fresh') return '';
  var cls = q === 'stale' ? 'q-stale' : q === 'invalid' ? 'q-invalid' : 'q-missing';
  var label = q.charAt(0).toUpperCase() + q.slice(1);
  return '<span class="quality-badge ' + cls + '">' + label + '</span>';
}

function drawSensorHero(s) {
  var box = document.getElementById('sensor-hero');
  var sensors = s.sensors;

  if (!sensors || sensors.length === 0) {
    box.innerHTML = '<div class="empty-state">No ' + terms.sensor.toLowerCase() + 's configured.<br>' +
      '<a href="#" onclick="showView(\'studio\');return false;">Add a ' + terms.sensor.toLowerCase() + ' in Hardware Studio</a></div>';
    return;
  }

  // Collect all reading cards, then sort by profile heroOrder
  var cards = [];
  for (var i = 0; i < sensors.length; i++) {
    var sen = sensors[i];
    var meta = sensorMeta[sen.type];
    if (!meta) {
      cards.push({ key: '_unknown', html: '<div class="hero-card hero-error">' +
        '<div class="hero-icon">' + ICO.alertTri() + '</div>' +
        '<div class="hero-value">--</div>' +
        '<div class="hero-label">' + escHtml(sen.name) + '</div>' +
        '<div class="hero-sub disconnected">Unsupported type ' + sen.type + '</div>' +
      '</div>' });
      continue;
    }

    var readings = meta.readings;
    var quality = sen.quality || 'missing';
    var qClass = quality === 'fresh' ? '' : ' stale';
    var sensorLabel = sen.label || sen.name;
    var ageText = formatAge(sen.reading_age_s);

    for (var r = 0; r < readings.length; r++) {
      var rd = readings[r];
      var val = '--';
      var statusHtml = '';

      if (!sen.connected) {
        statusHtml = '<div class="hero-sub disconnected">' + ICO.alertTri() + ' Node offline</div>';
      } else if (quality === 'invalid') {
        statusHtml = '<div class="hero-sub disconnected">' + ICO.alertTri() + ' Out of range</div>';
      } else if (quality === 'missing') {
        statusHtml = '<div class="hero-sub disconnected">Awaiting data</div>';
      }

      if (sen.readings && sen.readings[rd.key] !== undefined) {
        val = parseFloat(sen.readings[rd.key]).toFixed(rd.decimals);
      }

      var unitLabel = rd.unit;
      if (sen.unit && readings.length === 1) unitLabel = ' ' + sen.unit;

      var trend = computeTrend(rd.key);
      var trendHtml = trend.dir !== 'stable'
        ? '<span class="hero-trend ' + trend.cls + '">' + trend.arrow + '</span>'
        : '';
      var warnCls = getThresholdWarning(rd.key, val);
      var spark = sparklineSvg(rd.key);
      var sparkHtml = spark ? '<div class="hero-spark">' + spark + '</div>' : '';

      cards.push({ key: rd.key, html: '<div class="hero-card' + qClass + warnCls + '">' +
        '<div class="hero-top">' +
          '<div class="hero-icon">' + getSensorIcon(rd.key) + '</div>' +
          trendHtml +
          qualityBadge(quality) +
        '</div>' +
        '<div class="hero-value">' + val + '<span class="hero-unit">' + escHtml(unitLabel) + '</span></div>' +
        '<div class="hero-label">' + escHtml(sensorLabel) + ' — ' + rd.label + '</div>' +
        sparkHtml +
        (ageText ? '<div class="hero-age">' + ageText + '</div>' : '') +
        statusHtml +
      '</div>' });
    }
  }

  // Sort cards by profile heroOrder priority
  var order = currentProfile.heroOrder;
  cards.sort(function(a, b) {
    var ai = order.indexOf(a.key); if (ai < 0) ai = 99;
    var bi = order.indexOf(b.key); if (bi < 0) bi = 99;
    return ai - bi;
  });

  var html = '';
  for (var c = 0; c < cards.length; c++) html += cards[c].html;
  box.innerHTML = html;
}

function drawStatusBar(s) {
  var bar = document.getElementById('status-bar');
  var dotClass = s.node_online ? 'online' : 'offline';
  var nodeText = s.node_online ? 'Node online' : 'Node offline';
  var rssiText = s.node_online ? signalLabel(s.rssi) + ' (' + s.rssi + ' dBm)' : '--';

  var syncText = s.config_synced ? 'Synced' : 'Pending';
  var syncClass = s.config_synced ? 'sync-ok' : 'sync-pending';

  var ageItem = '';
  if (s.reading_age_s !== undefined) {
    var ageStr = formatAge(s.reading_age_s);
    var ageCls = s.reading_age_s > 600 ? ' age-stale' : '';
    ageItem = '<div class="status-item' + ageCls + '">' + ICO.wifi() + ' ' + ageStr + '</div>';
  }

  bar.innerHTML =
    '<div class="status-item"><span class="status-dot ' + dotClass + '"></span>' + nodeText + '</div>' +
    '<div class="spacer"></div>' +
    '<div class="status-item"><span class="' + syncClass + '">' + syncText + '</span></div>' +
    ageItem +
    '<div class="status-item">' + ICO.signal() + ' ' + rssiText + '</div>' +
    '<div class="status-item">' + ICO.clock() + ' ' + formatUptime(s.uptime_s) + '</div>';
}

// =====================================================================
//  Phase 11: Trend engine, sparklines, threshold warnings, stats
// =====================================================================
function computeTrend(key) {
  if (!chartData || chartData.length < 3) return { dir: 'stable', arrow: '', delta: 0 };
  var field = key === 'temperature_c' ? 'tc' : key === 'humidity_pct' ? 'h' : null;
  if (!field) return { dir: 'stable', arrow: '', delta: 0 };
  var recent = chartData.slice(-10);
  var first = 0, last = 0, count = recent.length;
  var half = Math.floor(count / 2);
  for (var i = 0; i < half; i++) first += recent[i][field];
  for (var i = half; i < count; i++) last += recent[i][field];
  first /= half; last /= (count - half);
  var delta = last - first;
  var threshold = key === 'temperature_c' ? 0.3 : 1.5;
  if (delta > threshold) return { dir: 'rising', arrow: ICO.activity(), delta: delta, cls: 'trend-up' };
  if (delta < -threshold) return { dir: 'falling', arrow: ICO.activity(), delta: delta, cls: 'trend-down' };
  return { dir: 'stable', arrow: '', delta: 0, cls: 'trend-stable' };
}

function sparklineSvg(key) {
  if (!chartData || chartData.length < 3) return '';
  var field = key === 'temperature_c' ? 'tc' : key === 'humidity_pct' ? 'h' : null;
  if (!field) return '';
  var pts = chartData.slice(-30);
  var vMin = Infinity, vMax = -Infinity;
  for (var i = 0; i < pts.length; i++) {
    if (pts[i][field] < vMin) vMin = pts[i][field];
    if (pts[i][field] > vMax) vMax = pts[i][field];
  }
  var range = vMax - vMin || 1;
  var w = 80, h = 18;
  var coords = [];
  for (var i = 0; i < pts.length; i++) {
    var x = (i / (pts.length - 1)) * w;
    var y = h - ((pts[i][field] - vMin) / range) * (h - 2) - 1;
    coords.push(x.toFixed(1) + ',' + y.toFixed(1));
  }
  var color = key === 'temperature_c' ? '#ef4444' : '#3b82f6';
  return '<svg class="sparkline" viewBox="0 0 ' + w + ' ' + h + '" preserveAspectRatio="none">' +
    '<polyline points="' + coords.join(' ') + '" fill="none" stroke="' + color + '" stroke-width="1.5" stroke-linejoin="round"/>' +
    '</svg>';
}

function getThresholdWarning(key, value) {
  if (!configData || !configData.rules || value === '--') return '';
  var sensorIdx = key === 'temperature_c' ? 0 : key === 'humidity_pct' ? 1 : key === 'analog_pct' ? 2 : -1;
  if (sensorIdx < 0) return '';
  var numVal = parseFloat(value);
  if (isNaN(numVal)) return '';
  for (var i = 0; i < configData.rules.length; i++) {
    var r = configData.rules[i];
    if (!r.enabled || r.sensor !== sensorIdx) continue;
    var thresh = r.threshold / 10;
    var hyst = r.hysteresis / 10;
    var dist = Math.abs(numVal - thresh);
    if (r.triggered) return ' hero-warn-active';
    if (dist <= hyst * 1.5) return ' hero-warn-near';
  }
  return '';
}

function computeStats() {
  if (!chartData || chartData.length < 2) return null;
  var stats = {
    temperature_c: { min: Infinity, max: -Infinity, sum: 0, count: 0 },
    humidity_pct:  { min: Infinity, max: -Infinity, sum: 0, count: 0 }
  };
  for (var i = 0; i < chartData.length; i++) {
    var p = chartData[i];
    if (p.tc !== undefined) {
      var s = stats.temperature_c;
      if (p.tc < s.min) s.min = p.tc;
      if (p.tc > s.max) s.max = p.tc;
      s.sum += p.tc; s.count++;
    }
    if (p.h !== undefined) {
      var s = stats.humidity_pct;
      if (p.h < s.min) s.min = p.h;
      if (p.h > s.max) s.max = p.h;
      s.sum += p.h; s.count++;
    }
  }
  return stats;
}

function drawStatsRow() {
  var row = document.getElementById('stats-row');
  if (!row) return;
  var stats = computeStats();
  if (!stats) { row.style.display = 'none'; return; }
  var html = '';
  var items = [
    { key: 'temperature_c', label: 'Temp', unit: '°C', dec: 1, color: '#ef4444' },
    { key: 'humidity_pct', label: 'Humidity', unit: '%', dec: 0, color: '#3b82f6' }
  ];
  for (var i = 0; i < items.length; i++) {
    var it = items[i];
    var s = stats[it.key];
    if (!s || s.count === 0) continue;
    var avg = (s.sum / s.count).toFixed(it.dec);
    var trend = computeTrend(it.key);
    var trendHtml = trend.dir !== 'stable'
      ? '<span class="stat-trend ' + trend.cls + '">' + (trend.delta > 0 ? '+' : '') + trend.delta.toFixed(it.dec) + '</span>'
      : '<span class="stat-trend trend-stable">stable</span>';
    html += '<div class="stat-chip">' +
      '<span class="stat-dot" style="background:' + it.color + '"></span>' +
      '<span class="stat-label">' + it.label + '</span>' +
      '<span class="stat-vals">' + s.min.toFixed(it.dec) + ' / <strong>' + avg + '</strong> / ' + s.max.toFixed(it.dec) + it.unit + '</span>' +
      trendHtml +
    '</div>';
  }
  if (html) {
    row.innerHTML = html;
    row.style.display = '';
  } else {
    row.style.display = 'none';
  }
}

// =====================================================================
//  Chart — canvas-based sensor history
// =====================================================================
var chartRange = 60;  // minutes to show
var chartData = [];

var hasSensors = true;

function loadHistory() {
  if (!hasSensors) {
    document.getElementById('chart-section').style.display = 'none';
    return;
  }
  fetch('/api/history')
    .then(function(res) { return res.json(); })
    .then(function(data) {
      chartData = data;
      drawChart();
      drawStatsRow();
      updateDataCount();
    });
}

function drawChart() {
  var section = document.getElementById('chart-section');
  if (chartData.length < 2) { section.style.display = 'none'; return; }
  section.style.display = '';

  var canvas = document.getElementById('chart-canvas');
  var dpr = window.devicePixelRatio || 1;
  var rect = canvas.getBoundingClientRect();
  canvas.width = rect.width * dpr;
  canvas.height = rect.height * dpr;
  var ctx = canvas.getContext('2d');
  ctx.scale(dpr, dpr);
  var W = rect.width, H = rect.height;

  // Filter to selected time range
  var now = chartData[chartData.length - 1].t;
  var cutoff = now - chartRange * 60;
  var pts = [];
  for (var i = 0; i < chartData.length; i++) {
    if (chartData[i].t >= cutoff) pts.push(chartData[i]);
  }
  if (pts.length < 2) { section.style.display = 'none'; return; }

  // Compute bounds
  var tMin = pts[0].t, tMax = pts[pts.length - 1].t;
  var tempMin = Infinity, tempMax = -Infinity;
  var humMin = Infinity, humMax = -Infinity;
  for (var i = 0; i < pts.length; i++) {
    if (pts[i].tc < tempMin) tempMin = pts[i].tc;
    if (pts[i].tc > tempMax) tempMax = pts[i].tc;
    if (pts[i].h < humMin) humMin = pts[i].h;
    if (pts[i].h > humMax) humMax = pts[i].h;
  }
  // Add padding to ranges
  var tempPad = Math.max((tempMax - tempMin) * 0.1, 1);
  var humPad = Math.max((humMax - humMin) * 0.1, 2);
  tempMin -= tempPad; tempMax += tempPad;
  humMin -= humPad; humMax += humPad;

  var pad = { l: 36, r: 36, t: 8, b: 20 };
  var cw = W - pad.l - pad.r;
  var ch = H - pad.t - pad.b;

  // Clear
  ctx.clearRect(0, 0, W, H);

  // Grid lines
  var style = getComputedStyle(document.documentElement);
  var gridColor = style.getPropertyValue('--border').trim() || '#e5e7eb';
  var mutedColor = style.getPropertyValue('--muted').trim() || '#6b7280';
  ctx.strokeStyle = gridColor;
  ctx.lineWidth = 0.5;
  for (var g = 0; g <= 4; g++) {
    var gy = pad.t + ch * (g / 4);
    ctx.beginPath(); ctx.moveTo(pad.l, gy); ctx.lineTo(W - pad.r, gy); ctx.stroke();
  }

  // Y-axis labels (temp left, hum right)
  ctx.font = '10px system-ui, sans-serif';
  ctx.fillStyle = '#ef4444';
  ctx.textAlign = 'right';
  for (var g = 0; g <= 4; g++) {
    var val = tempMax - (tempMax - tempMin) * (g / 4);
    var gy = pad.t + ch * (g / 4);
    ctx.fillText(val.toFixed(0) + '°', pad.l - 4, gy + 3);
  }
  ctx.fillStyle = '#3b82f6';
  ctx.textAlign = 'left';
  for (var g = 0; g <= 4; g++) {
    var val = humMax - (humMax - humMin) * (g / 4);
    var gy = pad.t + ch * (g / 4);
    ctx.fillText(val.toFixed(0) + '%', W - pad.r + 4, gy + 3);
  }

  // X-axis time labels
  ctx.fillStyle = mutedColor;
  ctx.textAlign = 'center';
  var span = tMax - tMin;
  var steps = Math.min(4, pts.length - 1);
  for (var g = 0; g <= steps; g++) {
    var t = tMin + span * (g / steps);
    var ago = Math.round((tMax - t) / 60);
    var label = ago === 0 ? 'now' : ago + 'm ago';
    var gx = pad.l + cw * (g / steps);
    ctx.fillText(label, gx, H - 4);
  }

  function xPos(t) { return pad.l + cw * ((t - tMin) / (tMax - tMin)); }

  // Draw temperature line
  ctx.strokeStyle = '#ef4444';
  ctx.lineWidth = 2;
  ctx.lineJoin = 'round';
  ctx.beginPath();
  for (var i = 0; i < pts.length; i++) {
    var x = xPos(pts[i].t);
    var y = pad.t + ch * (1 - (pts[i].tc - tempMin) / (tempMax - tempMin));
    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  }
  ctx.stroke();

  // Draw humidity line
  ctx.strokeStyle = '#3b82f6';
  ctx.beginPath();
  for (var i = 0; i < pts.length; i++) {
    var x = xPos(pts[i].t);
    var y = pad.t + ch * (1 - (pts[i].h - humMin) / (humMax - humMin));
    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  }
  ctx.stroke();
}

// Range button handlers
document.getElementById('chart-range').onclick = function(e) {
  var btn = e.target.closest('.range-btn');
  if (!btn) return;
  chartRange = parseInt(btn.getAttribute('data-range'));
  var btns = document.querySelectorAll('.range-btn');
  for (var i = 0; i < btns.length; i++) btns[i].className = 'range-btn';
  btn.className = 'range-btn active';
  drawChart();
};

// Resize redraw
window.addEventListener('resize', drawChart);

// =====================================================================
//  Phase 13: Data export & history management
// =====================================================================
function updateDataCount() {
  var el = document.getElementById('data-count');
  if (!el) return;
  if (chartData && chartData.length > 0) {
    el.textContent = chartData.length + ' pts';
  } else {
    el.textContent = '';
  }
}

function exportCsv() {
  if (!chartData || chartData.length === 0) return;
  var envName = 'LoRaSense';
  var nameEl = document.getElementById('env-name');
  if (nameEl && nameEl.textContent) envName = nameEl.textContent.replace(/[^a-zA-Z0-9_-]/g, '_');

  var rows = ['timestamp,temperature_c,humidity_pct'];
  for (var i = 0; i < chartData.length; i++) {
    var p = chartData[i];
    var d = new Date(p.t * 1000);
    var ts = d.getFullYear() + '-' +
      ('0' + (d.getMonth()+1)).slice(-2) + '-' +
      ('0' + d.getDate()).slice(-2) + ' ' +
      ('0' + d.getHours()).slice(-2) + ':' +
      ('0' + d.getMinutes()).slice(-2) + ':' +
      ('0' + d.getSeconds()).slice(-2);
    var tc = p.tc !== undefined ? p.tc.toFixed(1) : '';
    var h = p.h !== undefined ? p.h.toFixed(1) : '';
    rows.push(ts + ',' + tc + ',' + h);
  }

  var csv = rows.join('\n');
  var blob = new Blob([csv], { type: 'text/csv;charset=utf-8' });
  var url = URL.createObjectURL(blob);
  var now = new Date();
  var dateStr = now.getFullYear() +
    ('0' + (now.getMonth()+1)).slice(-2) +
    ('0' + now.getDate()).slice(-2);
  var a = document.createElement('a');
  a.href = url;
  a.download = envName + '_history_' + dateStr + '.csv';
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

document.getElementById('export-csv-btn').onclick = exportCsv;

function drawActiveRules(s) {
  var box = document.getElementById('active-rules');
  box.innerHTML = '';
  if (!s.rules) return;
  for (var i = 0; i < s.rules.length; i++) {
    var r = s.rules[i];
    if (!r.triggered || !r.enabled) continue;
    var eqName = r.equipment;
    if (s.devices) {
      for (var j = 0; j < s.devices.length; j++) {
        if (s.devices[j].id === r.equipment) { eqName = s.devices[j].name; break; }
      }
    }
    var div = document.createElement('div');
    div.className = 'active-rule-bar';
    div.innerHTML = '<span class="ar-icon">' + ICO.zap() + '</span> <span class="ar-name">' +
      escHtml(r.name) + '</span> → ' + escHtml(eqName);
    box.appendChild(div);
  }
}

function drawDevices(s) {
  var devices = s.devices;
  var box = document.getElementById('devices');
  if (!devices || devices.length === 0) {
    box.innerHTML = '<div class="empty-state">No ' + terms.equipment.toLowerCase() + ' configured.<br>' +
      '<a href="#" onclick="showView(\'settings\');return false;">Add ' + terms.equipment.toLowerCase() + ' in Settings</a></div>';
    return;
  }

  // Build zone name map
  var zoneNames = {};
  if (s.zones) {
    for (var i = 0; i < s.zones.length; i++) {
      zoneNames[s.zones[i].id] = s.zones[i].name;
    }
  }

  // Group by zone
  var zones = {};
  var zoneOrder = [];
  for (var i = 0; i < devices.length; i++) {
    var d = devices[i];
    var zk = d.zone_id || '_none';
    if (!zones[zk]) { zones[zk] = []; zoneOrder.push(zk); }
    zones[zk].push(d);
  }

  box.innerHTML = '';
  for (var zi = 0; zi < zoneOrder.length; zi++) {
    var zoneId = zoneOrder[zi];
    var section = document.createElement('div');
    section.className = 'zone-section';

    if (zoneOrder.length > 1 || zoneId !== '_none') {
      var title = document.createElement('div');
      title.className = 'zone-title';
      title.textContent = zoneId === '_none' ? 'Unassigned' : ((zoneNames[zoneId] || zoneId));
      section.appendChild(title);
    }

    var devs = zones[zoneId];
    for (var i = 0; i < devs.length; i++) {
      var d = devs[i];
      var hasOutput = d.output !== null && d.output !== undefined;
      var pending = d.status === 'PENDING' || d.status === 'SENT';

      var row = document.createElement('div');
      row.className = 'device';

      var icon = document.createElement('div');
      icon.className = 'dev-icon';
      icon.innerHTML = getCatIcon(d.category);
      row.appendChild(icon);

      var info = document.createElement('div');
      info.className = 'dev-info';

      var nameDiv = document.createElement('div');
      nameDiv.className = 'dev-name';
      nameDiv.textContent = d.name;
      info.appendChild(nameDiv);

      var metaDiv = document.createElement('div');
      metaDiv.className = 'dev-meta';
      if (!hasOutput) {
        metaDiv.innerHTML = '<span class="state-off">No relay</span>';
      } else if (d.override) {
        metaDiv.innerHTML = '<span class="state-override">Manual override</span>';
      } else if (pending) {
        metaDiv.innerHTML = '<span class="state-pending">Sending…</span>';
      } else if (d.status === 'FAILED') {
        metaDiv.innerHTML = '<span class="state-failed">Failed</span>';
      } else {
        metaDiv.innerHTML = d.on ? '<span class="state-on">ON</span>' : '<span class="state-off">OFF</span>';
      }
      info.appendChild(metaDiv);
      row.appendChild(info);

      var btnGroup = document.createElement('div');
      btnGroup.className = 'dev-btns';

      var toggle = document.createElement('button');
      if (!hasOutput) {
        toggle.className = 'toggle no-relay';
      } else {
        toggle.className = 'toggle' + (d.on ? ' on' : '');
        if (pending) toggle.disabled = true;
        (function(id, on) {
          toggle.onclick = function() { setDevice(id, on ? 'off' : 'on'); };
        })(d.id, d.on);
      }
      btnGroup.appendChild(toggle);

      if (hasOutput) {
        var ovrBtn = document.createElement('button');
        ovrBtn.className = 'btn-override' + (d.override ? ' active' : '');
        ovrBtn.textContent = d.override ? '\u{1F6D1}' : '✋';
        ovrBtn.title = d.override ? 'Resume automation' : 'Manual override';
        (function(id, cur) {
          ovrBtn.onclick = function() {
            fetch('/api/override', {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify({ id: id, override: !cur })
            }).then(function() { refresh(); });
          };
        })(d.id, d.override);
        btnGroup.appendChild(ovrBtn);
      }
      row.appendChild(btnGroup);

      section.appendChild(row);
    }
    box.appendChild(section);
  }
}

function setDevice(id, state) {
  var devName = id;
  if (lastQuickDevices) {
    for (var i = 0; i < lastQuickDevices.length; i++) {
      if (lastQuickDevices[i].id === id) { devName = lastQuickDevices[i].name; break; }
    }
  }
  var body = 'id=' + encodeURIComponent(id) + '&state=' + encodeURIComponent(state);
  fetch('/api/devices', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: body
  }).then(function() {
    showToast(devName + ' turned ' + state, 'success');
    refresh();
  }).catch(function() {
    showToast('Failed to switch ' + devName, 'error');
  });
}

// =====================================================================
//  Phase 12: Connection resilience
// =====================================================================
var failCount = 0;
var reconnectTimer = null;
var reconnectCountdown = 0;
var RECONNECT_DELAYS = [3, 5, 10, 15, 30];

function getReconnectDelay() {
  var idx = Math.min(failCount - 1, RECONNECT_DELAYS.length - 1);
  return RECONNECT_DELAYS[Math.max(0, idx)];
}

function showReconnectOverlay() {
  var overlay = document.getElementById('reconnect-overlay');
  overlay.classList.add('active');
  startReconnectCountdown();
}

function hideReconnectOverlay() {
  var overlay = document.getElementById('reconnect-overlay');
  overlay.classList.remove('active');
  clearInterval(reconnectTimer);
}

function startReconnectCountdown() {
  clearInterval(reconnectTimer);
  reconnectCountdown = getReconnectDelay();
  var cdEl = document.getElementById('reconnect-countdown');
  var bar = document.getElementById('reconnect-progress');
  var total = reconnectCountdown;
  cdEl.textContent = reconnectCountdown;
  bar.style.transition = 'none';
  bar.style.width = '100%';
  bar.offsetHeight;
  bar.style.transition = 'width 1s linear';

  reconnectTimer = setInterval(function() {
    reconnectCountdown--;
    cdEl.textContent = Math.max(0, reconnectCountdown);
    bar.style.width = Math.max(0, (reconnectCountdown / total) * 100) + '%';
    if (reconnectCountdown <= 0) {
      clearInterval(reconnectTimer);
      refresh();
    }
  }, 1000);
}

document.getElementById('reconnect-now-btn').onclick = function() {
  clearInterval(reconnectTimer);
  refresh();
};

function refresh() {
  fetch('/api/state')
    .then(function(res) { return res.json(); })
    .then(function(s) {
      var wasDisconnected = failCount >= 3;
      failCount = 0;
      document.getElementById('banner').style.display = 'none';
      hideReconnectOverlay();
      if (wasDisconnected) showToast('Connection restored', 'success');
      if (s.env_name) {
        document.getElementById('env-name').textContent = s.env_name;
      }
      if (s.env_type) {
        applyTheme(s.env_type);
        currentProfile = PROFILES[s.env_type] || PROFILES.custom;
        terms = currentProfile.terms;
      }
      hasSensors = s.sensors && s.sensors.length > 0;
      drawSensorHero(s);
      drawStatusBar(s);
      drawActiveRules(s);
      drawDevices(s);
      lastQuickDevices = s.devices;
      drawQuickBar(s);
    })
    .catch(function() {
      failCount++;
      document.getElementById('banner').style.display = 'block';
      if (failCount >= 3 && currentView === 'dashboard') {
        clearInterval(pollTimer);
        showReconnectOverlay();
      }
    });
}

// =====================================================================
//  Settings
// =====================================================================
function loadConfig() {
  fetch('/api/config')
    .then(function(res) { return res.json(); })
    .then(function(cfg) {
      configData = cfg;
      renderSettings(cfg);
    });
}

function renderSettings(cfg) {
  // Environment
  document.getElementById('env-name-input').value = cfg.environment.name || '';
  document.getElementById('env-type-input').value = cfg.environment.type || 'home';
  document.getElementById('env-theme-input').value = themeOverride || 'auto';

  // Zones
  var zoneList = document.getElementById('zone-list');
  zoneList.innerHTML = '';
  for (var i = 0; i < cfg.zones.length; i++) {
    var z = cfg.zones[i];
    var div = document.createElement('div');
    div.className = 'zone-item';
    div.innerHTML = '<span>' + escHtml(z.name) + '</span>';
    var delBtn = document.createElement('button');
    delBtn.className = 'btn-danger';
    delBtn.textContent = 'Remove';
    (function(id) {
      delBtn.onclick = function() { deleteZone(id); };
    })(z.id);
    div.appendChild(delBtn);
    zoneList.appendChild(div);
  }

  // Zone dropdown for equipment form
  var eqZone = document.getElementById('eq-zone');
  eqZone.innerHTML = '<option value="">No zone</option>';
  for (var i = 0; i < cfg.zones.length; i++) {
    var opt = document.createElement('option');
    opt.value = cfg.zones[i].id;
    opt.textContent = cfg.zones[i].name;
    eqZone.appendChild(opt);
  }

  // Relay outputs for equipment form — show all with availability
  var eqOutput = document.getElementById('eq-output');
  eqOutput.innerHTML = '<option value="255">None (monitor only)</option>';
  for (var ri = 0; ri < cfg.max_outputs; ri++) {
    var opt = document.createElement('option');
    opt.value = ri;
    var owner = null;
    for (var ei = 0; ei < cfg.equipment.length; ei++) {
      if (cfg.equipment[ei].output === ri) { owner = cfg.equipment[ei]; break; }
    }
    if (owner) {
      opt.textContent = 'Relay ' + (ri + 1) + (owner.pin_name ? ' (' + owner.pin_name + ')' : '') + ' — ' + owner.name;
      opt.disabled = true;
    } else {
      opt.textContent = 'Relay ' + (ri + 1) + ' — Available';
    }
    eqOutput.appendChild(opt);
  }

  // Equipment list
  var eqList = document.getElementById('equipment-list');
  eqList.innerHTML = '';
  for (var i = 0; i < cfg.equipment.length; i++) {
    var eq = cfg.equipment[i];
    var div = document.createElement('div');
    div.className = 'eq-item';
    var outputText = eq.output !== undefined && eq.output !== null
      ? 'Relay ' + (eq.output + 1) + (eq.pin_name ? ' (' + eq.pin_name + ')' : '')
      : 'No relay';
    var nodeText = eq.node_id ? ' · ' + eq.node_id : '';
    div.innerHTML =
      '<div><div class="eq-name">' + escHtml(eq.name) + '</div>' +
      '<div class="eq-meta">' + eq.category + ' · ' + outputText + nodeText + '</div></div>';
    var delBtn = document.createElement('button');
    delBtn.className = 'btn-danger';
    delBtn.textContent = 'Remove';
    (function(id) {
      delBtn.onclick = function() { deleteEquipment(id); };
    })(eq.id);
    div.appendChild(delBtn);
    eqList.appendChild(div);
  }

  // Rules
  renderRules(cfg);
}

function escHtml(s) {
  var d = document.createElement('div');
  d.textContent = s;
  return d.innerHTML;
}

// --- Save environment ---
document.getElementById('save-env-btn').onclick = function() {
  var data = {
    name: document.getElementById('env-name-input').value,
    type: document.getElementById('env-type-input').value
  };
  var themeSel = document.getElementById('env-theme-input').value;
  setThemeOverride(themeSel === 'auto' ? '' : themeSel);
  applyTheme(data.type);
  fetch('/api/config/environment', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  }).then(function() { loadConfig(); });
};

// --- Add zone ---
document.getElementById('add-zone-btn').onclick = function() {
  var name = document.getElementById('new-zone-name').value.trim();
  if (!name) return;
  var id = 'z_' + name.toLowerCase().replace(/[^a-z0-9]/g, '_').substring(0, 12);
  fetch('/api/config/zones', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ id: id, name: name })
  }).then(function() {
    document.getElementById('new-zone-name').value = '';
    loadConfig();
  });
};

function deleteZone(id) {
  fetch('/api/config/zones?id=' + encodeURIComponent(id), { method: 'DELETE' })
    .then(function() { loadConfig(); });
}

// --- Add equipment ---
document.getElementById('add-eq-btn').onclick = function() {
  var name = document.getElementById('eq-name').value.trim();
  if (!name) return;
  var id = 'eq_' + Date.now().toString(36);
  var data = {
    id: id,
    name: name,
    category: document.getElementById('eq-category').value,
    zone_id: document.getElementById('eq-zone').value,
    node_id: 'mega-001',
    output: parseInt(document.getElementById('eq-output').value)
  };
  if (data.output === 255) delete data.output;
  fetch('/api/config/equipment', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  }).then(function(res) { return res.json().then(function(j){ return {ok: res.ok, body: j}; }); })
    .then(function(r) {
      if (r.ok) {
        document.getElementById('eq-name').value = '';
        loadConfig();
      } else {
        showEqError(r.body.error || 'Failed to add equipment');
      }
    });
};

function showEqError(msg) {
  var el = document.getElementById('eq-error');
  el.textContent = msg;
  el.style.display = '';
  setTimeout(function() { el.style.display = 'none'; }, 5000);
}

function deleteEquipment(id) {
  fetch('/api/config/equipment?id=' + encodeURIComponent(id), { method: 'DELETE' })
    .then(function() { loadConfig(); });
}

// =====================================================================
//  Automation Rules
// =====================================================================
function renderRules(cfg) {
  var rules = cfg.rules || [];
  var ruleList = document.getElementById('rules-list');
  ruleList.innerHTML = '';

  if (rules.length === 0) {
    ruleList.innerHTML = '<div class="empty-hint">No automation rules yet.</div>';
  }

  var sensorNames = ['Temperature', 'Humidity', 'Analog'];
  var condNames   = ['above', 'below'];
  var unitNames   = ['°C', '%', '%'];

  for (var i = 0; i < rules.length; i++) {
    var r = rules[i];
    var div = document.createElement('div');
    div.className = 'rule-item' + (r.triggered ? ' triggered' : '') + (!r.enabled ? ' disabled' : '');

    var eqName = r.equipment;
    for (var j = 0; j < cfg.equipment.length; j++) {
      if (cfg.equipment[j].id === r.equipment) { eqName = cfg.equipment[j].name; break; }
    }

    var thresh = (r.threshold / 10).toFixed(1);
    var hyst = (r.hysteresis / 10).toFixed(1);
    var desc = sensorNames[r.sensor] + ' ' + condNames[r.condition] + ' ' +
              thresh + unitNames[r.sensor] + ' (±' + hyst + ')';
    var extras = [];
    if (r.duration_s) extras.push('hold ' + r.duration_s + 's');
    if (r.cooldown_s) extras.push('cooldown ' + r.cooldown_s + 's');
    var extraText = extras.length ? ' [' + extras.join(', ') + ']' : '';

    var faultBadge = r.fault ? ' <span class="rule-fault">FAULT</span>' : '';
    var activeBadge = r.triggered ? ' <span class="rule-active">ACTIVE</span>' : '';

    div.innerHTML =
      '<div class="rule-info">' +
        '<div class="rule-name">' + escHtml(r.name) + activeBadge + faultBadge + '</div>' +
        '<div class="rule-desc">' + desc + extraText + ' → ' + escHtml(eqName) + '</div>' +
      '</div>';

    var toggleBtn = document.createElement('button');
    toggleBtn.className = 'btn-small' + (r.enabled ? '' : ' btn-off');
    toggleBtn.textContent = r.enabled ? 'Enabled' : 'Disabled';
    (function(rule) {
      toggleBtn.onclick = function() { toggleRule(rule); };
    })(r);
    div.appendChild(toggleBtn);

    var delBtn = document.createElement('button');
    delBtn.className = 'btn-danger';
    delBtn.textContent = 'Remove';
    (function(id) {
      delBtn.onclick = function() { deleteRule(id); };
    })(r.id);
    div.appendChild(delBtn);

    ruleList.appendChild(div);
  }

  // Populate equipment dropdown for the add-rule form
  var ruleEq = document.getElementById('rule-equipment');
  ruleEq.innerHTML = '';
  for (var i = 0; i < cfg.equipment.length; i++) {
    var eq = cfg.equipment[i];
    if (eq.output === undefined || eq.output === null) continue;
    var opt = document.createElement('option');
    opt.value = eq.id;
    opt.textContent = eq.name;
    ruleEq.appendChild(opt);
  }

  // Render automation presets for the current environment
  var presetBox = document.getElementById('preset-suggestions');
  if (presetBox && currentProfile.presets && currentProfile.presets.length > 0) {
    var existingNames = {};
    for (var i = 0; i < rules.length; i++) existingNames[rules[i].name] = true;
    var presetsHtml = '';
    for (var p = 0; p < currentProfile.presets.length; p++) {
      var pr = currentProfile.presets[p];
      if (existingNames[pr.name]) continue;
      presetsHtml += '<div class="preset-card">' +
        '<div class="preset-info"><div class="preset-name">' + escHtml(pr.name) + '</div>' +
        '<div class="preset-desc">' + escHtml(pr.desc) + '</div></div>' +
        '<button class="btn-small" data-preset="' + p + '">Use</button></div>';
    }
    if (presetsHtml) {
      presetBox.innerHTML = '<div class="preset-label">' + ICO.zap() + ' Suggested for ' +
        escHtml(configData && configData.environment ? configData.environment.type || '' : '') +
        '</div>' + presetsHtml;
      presetBox.style.display = '';
    } else {
      presetBox.style.display = 'none';
    }
  } else if (presetBox) {
    presetBox.style.display = 'none';
  }
}

document.getElementById('rule-sensor').onchange = function() {
  var hints = ['30.0', '70.0', '50.0'];
  var units = ['°C', '%RH', '%'];
  var idx = parseInt(this.value) || 0;
  document.getElementById('rule-threshold').placeholder = hints[idx] || '30.0';
};

document.getElementById('add-rule-btn').onclick = function() {
  var name = document.getElementById('rule-name').value.trim();
  if (!name) return;
  var eqId = document.getElementById('rule-equipment').value;
  if (!eqId) return;

  var id = 'r_' + Date.now().toString(36);
  var data = {
    id: id,
    name: name,
    sensor: parseInt(document.getElementById('rule-sensor').value),
    condition: parseInt(document.getElementById('rule-condition').value),
    threshold: Math.round(parseFloat(document.getElementById('rule-threshold').value) * 10),
    hysteresis: Math.round(parseFloat(document.getElementById('rule-hysteresis').value) * 10),
    equipment: eqId,
    enabled: true,
    duration_s: parseInt(document.getElementById('rule-duration').value) || 0,
    cooldown_s: parseInt(document.getElementById('rule-cooldown').value) || 0
  };

  fetch('/api/rules', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  }).then(function(res) { return res.json(); })
    .then(function(r) {
      if (r.ok) {
        document.getElementById('rule-name').value = '';
        document.getElementById('rule-threshold').value = '';
        loadConfig();
      }
    });
};

// Preset suggestion click handler
var presetBox = document.getElementById('preset-suggestions');
if (presetBox) {
  presetBox.onclick = function(e) {
    var btn = e.target.closest('[data-preset]');
    if (!btn) return;
    var idx = parseInt(btn.getAttribute('data-preset'));
    var pr = currentProfile.presets[idx];
    if (!pr) return;
    // Find first equipment matching the preset category
    var eqId = '';
    if (configData && configData.equipment) {
      for (var i = 0; i < configData.equipment.length; i++) {
        var eq = configData.equipment[i];
        if (eq.category === pr.cat && eq.output !== undefined && eq.output !== null) {
          eqId = eq.id; break;
        }
      }
      if (!eqId) {
        for (var i = 0; i < configData.equipment.length; i++) {
          if (configData.equipment[i].output !== undefined && configData.equipment[i].output !== null) {
            eqId = configData.equipment[i].id; break;
          }
        }
      }
    }
    if (!eqId) return;
    document.getElementById('rule-name').value = pr.name;
    document.getElementById('rule-sensor').value = pr.sensor;
    document.getElementById('rule-condition').value = pr.cond;
    document.getElementById('rule-threshold').value = (pr.thresh / 10).toFixed(1);
    document.getElementById('rule-hysteresis').value = (pr.hyst / 10).toFixed(1);
    document.getElementById('rule-equipment').value = eqId;
  };
}

function toggleRule(rule) {
  var data = {
    id: rule.id,
    name: rule.name,
    sensor: rule.sensor,
    condition: rule.condition,
    threshold: rule.threshold,
    hysteresis: rule.hysteresis,
    equipment: rule.equipment,
    enabled: !rule.enabled
  };
  fetch('/api/rules', {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  }).then(function() { loadConfig(); });
}

function deleteRule(id) {
  fetch('/api/rules?id=' + encodeURIComponent(id), { method: 'DELETE' })
    .then(function() { loadConfig(); });
}

// =====================================================================
//  Action log
// =====================================================================
function loadActionLog() {
  fetch('/api/action-log')
    .then(function(res) { return res.json(); })
    .then(function(entries) {
      var list = document.getElementById('action-log-list');
      if (!entries || entries.length === 0) {
        list.innerHTML = '<div class="empty-hint">No actions recorded yet.</div>';
        return;
      }
      var html = '<div class="action-log">';
      for (var i = entries.length - 1; i >= 0; i--) {
        var e = entries[i];
        var cls = 'log-' + (e.action === 'TRIGGER' ? 'trigger' :
                   e.action === 'RELEASE' ? 'release' :
                   e.action === 'FAULT_OFF' ? 'fault' : 'info');
        html += '<div class="log-entry ' + cls + '">' +
          '<span class="log-action">' + e.action + '</span> ' +
          '<span class="log-rule">' + escHtml(e.rule) + '</span> → ' +
          escHtml(e.equipment) +
          (e.value !== '0.0' ? ' (' + e.value + ')' : '') +
          '<span class="log-time">' + formatUptime(e.ts) + '</span>' +
        '</div>';
      }
      html += '</div>';
      list.innerHTML = html;
    });
}
document.getElementById('load-action-log-btn').onclick = loadActionLog;

// =====================================================================
//  Onboarding Wizard
// =====================================================================
var WIZ_STEPS = 7;
var wizStep = 1;
var wizData = {
  env_name: 'My Home',
  env_type: 'home',
  zones: [],
  equipment: [],
  sensors: []
};
var wizRelayUsed = {};
var wizPinUsed = {};
var wizDrivers = [];

// Environment profile templates — suggestions only, not auto-created
var ENV_PROFILES = {
  home:       { sensors: ['DHT11 — Room temperature & humidity'], equipment: ['Lights', 'Fan'] },
  farm:       { sensors: ['DHT11 — Ambient monitoring', 'Analog — Soil moisture'], equipment: ['Irrigation pump', 'Ventilation fan'] },
  greenhouse: { sensors: ['DHT11 — Climate monitoring', 'Analog — Soil moisture'], equipment: ['Exhaust fan', 'Water pump', 'Grow lights'] },
  industrial: { sensors: ['DHT22 — Precision monitoring', 'Analog — Process sensor'], equipment: ['Cooling system', 'Alarm relay'] },
  lab:        { sensors: ['DHT22 — Lab environment', 'Analog — Instrument input'], equipment: ['Fume hood fan', 'Equipment power'] },
  custom:     { sensors: ['Add sensors for your use case'], equipment: ['Add equipment as needed'] }
};

function showWizard() {
  document.getElementById('view-dashboard').style.display = 'none';
  document.getElementById('view-settings').style.display = 'none';
  document.getElementById('view-wizard').style.display = '';
  document.querySelector('header').style.display = 'none';
  clearInterval(pollTimer);
  // Load driver data for sensor step
  fetch('/api/hw/drivers').then(function(r){return r.json();}).then(function(data){
    wizDrivers = Array.isArray(data) ? data : (data.drivers || []);
  });
  fetch('/api/hw/pins').then(function(r){return r.json();}).then(function(pins){
    window._wizPinData = pins;
  });
  wizGoTo(1);
}

function wizGoTo(step) {
  wizStep = step;
  for (var i = 1; i <= WIZ_STEPS; i++) {
    document.getElementById('wiz-' + i).style.display = (i === step) ? '' : 'none';
  }
  document.getElementById('wizard-bar').style.width = Math.round(step / WIZ_STEPS * 100) + '%';
}

// Step 1: name
document.getElementById('wiz-next-1').onclick = function() {
  var name = document.getElementById('wiz-env-name').value.trim();
  if (!name) return;
  wizData.env_name = name;
  wizGoTo(2);
};

// Step 2: type + profile hint
function updateProfileHint() {
  var hint = document.getElementById('wiz-profile-hint');
  var prof = PROFILES[wizData.env_type];
  var envP = ENV_PROFILES[wizData.env_type];
  if (!prof || !envP) { hint.innerHTML = ''; return; }
  var presetHtml = '';
  if (prof.presets && prof.presets.length) {
    presetHtml = '<br><span style="color:var(--muted)">Automation ideas: ' +
      prof.presets.map(function(p){ return p.name; }).join(', ') + '</span>';
  }
  hint.innerHTML = '<strong>Suggested for ' + wizData.env_type + ':</strong><br>' +
    '<span style="color:var(--muted)">Sensors: ' + envP.sensors.join(', ') + '<br>' +
    'Equipment: ' + envP.equipment.join(', ') + '</span>' + presetHtml + '<br>' +
    '<small>These are suggestions — you choose what to add in the next steps.</small>';
}

var typeGrid = document.getElementById('wiz-type-grid');
typeGrid.onclick = function(e) {
  var btn = e.target.closest('.wiz-type-btn');
  if (!btn) return;
  var btns = typeGrid.querySelectorAll('.wiz-type-btn');
  for (var i = 0; i < btns.length; i++) btns[i].className = 'wiz-type-btn';
  btn.className = 'wiz-type-btn selected';
  wizData.env_type = btn.getAttribute('data-type');
  updateProfileHint();
};
document.getElementById('wiz-back-2').onclick = function() { wizGoTo(1); };
document.getElementById('wiz-next-2').onclick = function() {
  updateProfileHint();
  var prof = PROFILES[wizData.env_type] || PROFILES.custom;
  currentProfile = prof;
  terms = prof.terms;
  document.getElementById('wiz-zone-name').placeholder = prof.terms.wizZonePlaceholder || 'e.g. Main area';
  wizGoTo(3);
};

// Step 3: zones
function renderWizZones() {
  var list = document.getElementById('wiz-zone-list');
  list.innerHTML = '';
  for (var i = 0; i < wizData.zones.length; i++) {
    var tag = document.createElement('span');
    tag.className = 'wiz-zone-tag';
    tag.innerHTML = escHtml(wizData.zones[i].name) + ' ';
    var del = document.createElement('button');
    del.textContent = '×';
    (function(idx) {
      del.onclick = function() {
        wizData.zones.splice(idx, 1);
        renderWizZones();
      };
    })(i);
    tag.appendChild(del);
    list.appendChild(tag);
  }
}

document.getElementById('wiz-add-zone').onclick = function() {
  var input = document.getElementById('wiz-zone-name');
  var name = input.value.trim();
  if (!name) return;
  var id = 'z_' + name.toLowerCase().replace(/[^a-z0-9]/g, '_').substring(0, 12);
  wizData.zones.push({ id: id, name: name });
  input.value = '';
  renderWizZones();
};

document.getElementById('wiz-back-3').onclick = function() { wizGoTo(2); };
document.getElementById('wiz-next-3').onclick = function() {
  if (wizData.zones.length === 0) {
    wizData.zones.push({ id: 'z_main', name: 'Main' });
    renderWizZones();
  }
  populateWizZoneDropdowns();
  updateWizRelayDropdown();
  renderWizEquipment();
  wizGoTo(4);
};

function populateWizZoneDropdowns() {
  var sels = [document.getElementById('wiz-eq-zone'), document.getElementById('wiz-sensor-zone')];
  for (var s = 0; s < sels.length; s++) {
    if (!sels[s]) continue;
    sels[s].innerHTML = '<option value="">No zone</option>';
    for (var i = 0; i < wizData.zones.length; i++) {
      var opt = document.createElement('option');
      opt.value = wizData.zones[i].id;
      opt.textContent = wizData.zones[i].name;
      sels[s].appendChild(opt);
    }
  }
}

// Step 4: equipment
function updateWizRelayDropdown() {
  var sel = document.getElementById('wiz-eq-relay');
  sel.innerHTML = '<option value="255">None</option>';
  for (var r = 0; r < 4; r++) {
    if (!wizRelayUsed[r]) {
      var opt = document.createElement('option');
      opt.value = r;
      opt.textContent = 'Relay ' + (r + 1);
      sel.appendChild(opt);
    }
  }
}

function renderWizEquipment() {
  var list = document.getElementById('wiz-eq-list');
  list.innerHTML = '';
  for (var i = 0; i < wizData.equipment.length; i++) {
    var eq = wizData.equipment[i];
    var div = document.createElement('div');
    div.className = 'wiz-eq-item';
    var relayText = eq.output !== 255 ? 'Relay ' + (eq.output + 1) : 'No relay';
    div.innerHTML =
      '<div><div style="font-weight:500">' + escHtml(eq.name) + '</div>' +
      '<div class="wiz-eq-meta">' + eq.category + ' · ' + relayText + '</div></div>';
    var del = document.createElement('button');
    del.className = 'btn-danger';
    del.textContent = 'Remove';
    (function(idx) {
      del.onclick = function() {
        var removed = wizData.equipment.splice(idx, 1)[0];
        if (removed.output !== 255) delete wizRelayUsed[removed.output];
        updateWizRelayDropdown();
        renderWizEquipment();
      };
    })(i);
    div.appendChild(del);
    list.appendChild(div);
  }
}

document.getElementById('wiz-add-eq').onclick = function() {
  var name = document.getElementById('wiz-eq-name').value.trim();
  if (!name) return;
  var relay = parseInt(document.getElementById('wiz-eq-relay').value);
  var id = 'eq_' + Date.now().toString(36);
  var eq = {
    id: id,
    name: name,
    category: document.getElementById('wiz-eq-cat').value,
    zone_id: document.getElementById('wiz-eq-zone').value,
    output: relay
  };
  wizData.equipment.push(eq);
  if (relay !== 255) wizRelayUsed[relay] = true;
  document.getElementById('wiz-eq-name').value = '';
  updateWizRelayDropdown();
  renderWizEquipment();
};

document.getElementById('wiz-back-4').onclick = function() { wizGoTo(3); };
document.getElementById('wiz-next-4').onclick = function() {
  populateWizSensorForm();
  renderWizSensors();
  wizGoTo(5);
};

// Step 5: sensors
function populateWizSensorForm() {
  var sel = document.getElementById('wiz-sensor-type');
  sel.innerHTML = '';
  for (var i = 0; i < wizDrivers.length; i++) {
    var d = wizDrivers[i];
    if (!d.supported) continue;
    var opt = document.createElement('option');
    opt.value = d.type;
    opt.textContent = d.name;
    sel.appendChild(opt);
  }
  updateWizSensorPins();
}

function updateWizSensorPins() {
  var pins = window._wizPinData || [];
  var sel = document.getElementById('wiz-sensor-pin');
  sel.innerHTML = '';
  var typeVal = parseInt(document.getElementById('wiz-sensor-type').value);
  var driver = null;
  for (var d = 0; d < wizDrivers.length; d++) {
    if (wizDrivers[d].type === typeVal) { driver = wizDrivers[d]; break; }
  }
  var capFilter = driver ? (CAP_NAMES[driver.pin_req] || '') : 'DI';
  var info = document.getElementById('wiz-sensor-pin-info');
  if (info) {
    info.textContent = capFilter ? 'Requires: ' + capFilter + ' capable pin' : '';
    if (driver && driver.pin_req === 0) {
      info.textContent = 'Bus device — no pin needed';
      sel.innerHTML = '<option value="255">None (bus)</option>';
      sel.style.display = 'none';
      return;
    }
    sel.style.display = '';
  }
  for (var i = 0; i < pins.length; i++) {
    var p = pins[i];
    if (p.state !== 0) continue;
    if (wizPinUsed[p.pin]) continue;
    if (capFilter && p.caps.indexOf(capFilter) < 0) continue;
    var opt = document.createElement('option');
    opt.value = p.pin;
    opt.textContent = p.name + ' (' + p.caps + ')';
    sel.appendChild(opt);
  }
}

var wizSensorTypeEl = document.getElementById('wiz-sensor-type');
if (wizSensorTypeEl) wizSensorTypeEl.onchange = updateWizSensorPins;

function renderWizSensors() {
  var list = document.getElementById('wiz-sensor-list');
  list.innerHTML = '';
  for (var i = 0; i < wizData.sensors.length; i++) {
    var s = wizData.sensors[i];
    var div = document.createElement('div');
    div.className = 'wiz-eq-item';
    var pinText = s.pin !== 255 ? 'Pin ' + s.pin_name : 'Bus';
    div.innerHTML =
      '<div><div style="font-weight:500">' + escHtml(s.name) + '</div>' +
      '<div class="wiz-eq-meta">' + escHtml(s.driver_name) + ' · ' + pinText + '</div></div>';
    var del = document.createElement('button');
    del.className = 'btn-danger';
    del.textContent = 'Remove';
    (function(idx) {
      del.onclick = function() {
        var removed = wizData.sensors.splice(idx, 1)[0];
        if (removed.pin !== 255) delete wizPinUsed[removed.pin];
        updateWizSensorPins();
        renderWizSensors();
      };
    })(i);
    div.appendChild(del);
    list.appendChild(div);
  }
}

document.getElementById('wiz-add-sensor').onclick = function() {
  var name = document.getElementById('wiz-sensor-name').value.trim();
  if (!name) return;
  var type = parseInt(document.getElementById('wiz-sensor-type').value);
  var pin = parseInt(document.getElementById('wiz-sensor-pin').value);
  var driverName = '';
  for (var d = 0; d < wizDrivers.length; d++) {
    if (wizDrivers[d].type === type) { driverName = wizDrivers[d].name; break; }
  }
  var pinName = '';
  if (pin !== 255 && window._wizPinData) {
    for (var i = 0; i < window._wizPinData.length; i++) {
      if (window._wizPinData[i].pin === pin) { pinName = window._wizPinData[i].name; break; }
    }
  }
  var id = 'sen_' + Date.now().toString(36);
  var sensor = {
    id: id, name: name, type: type, pin: pin,
    zone_id: document.getElementById('wiz-sensor-zone').value,
    driver_name: driverName, pin_name: pinName
  };
  wizData.sensors.push(sensor);
  if (pin !== 255) wizPinUsed[pin] = true;
  document.getElementById('wiz-sensor-name').value = '';
  updateWizSensorPins();
  renderWizSensors();
};

document.getElementById('wiz-back-5').onclick = function() { wizGoTo(4); };
document.getElementById('wiz-next-5').onclick = function() {
  renderWizSummary();
  wizGoTo(6);
};

// Step 6: summary + validation
function renderWizSummary() {
  var html = '<div class="wiz-summary-section">' +
    '<h4>' + escHtml(wizData.env_name) + ' <small style="color:var(--muted)">(' + wizData.env_type + ')</small></h4>' +
    '<table style="width:100%;font-size:13px;border-collapse:collapse">';
  html += '<tr><td style="padding:4px 8px;font-weight:500">Zones</td><td>' +
    (wizData.zones.length ? wizData.zones.map(function(z){return escHtml(z.name);}).join(', ') : '<em>None</em>') + '</td></tr>';
  html += '<tr><td style="padding:4px 8px;font-weight:500">Equipment</td><td>' +
    (wizData.equipment.length ? wizData.equipment.map(function(e){
      var relay = e.output !== 255 ? ' (Relay ' + (e.output+1) + ')' : '';
      return escHtml(e.name) + relay;
    }).join(', ') : '<em>None</em>') + '</td></tr>';
  html += '<tr><td style="padding:4px 8px;font-weight:500">Sensors</td><td>' +
    (wizData.sensors.length ? wizData.sensors.map(function(s){
      return escHtml(s.name) + ' (' + escHtml(s.driver_name) + ')';
    }).join(', ') : '<em>None</em>') + '</td></tr>';
  html += '</table></div>';

  document.getElementById('wiz-summary').innerHTML = html;

  // Validation
  var issues = [];
  if (wizData.zones.length === 0) issues.push('No zones defined');
  if (wizData.sensors.length === 0) issues.push('No sensors added');
  var valEl = document.getElementById('wiz-validation');
  if (issues.length) {
    valEl.textContent = 'Warning: ' + issues.join('. ') + '. You can still finish, but the system won\'t be fully functional.';
    valEl.style.display = '';
  } else {
    valEl.style.display = 'none';
  }
}

document.getElementById('wiz-back-6').onclick = function() { wizGoTo(5); };

document.getElementById('wiz-finish').onclick = function() {
  var payload = {
    env_name: wizData.env_name,
    env_type: wizData.env_type,
    zones: wizData.zones,
    equipment: [],
    sensors: []
  };
  for (var i = 0; i < wizData.equipment.length; i++) {
    var eq = wizData.equipment[i];
    var obj = { id: eq.id, name: eq.name, category: eq.category, zone_id: eq.zone_id };
    if (eq.output !== 255) obj.output = eq.output;
    payload.equipment.push(obj);
  }
  for (var i = 0; i < wizData.sensors.length; i++) {
    var s = wizData.sensors[i];
    payload.sensors.push({ id: s.id, name: s.name, type: s.type, pin: s.pin, zone_id: s.zone_id });
  }

  fetch('/api/onboarding', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  }).then(function(res) { return res.json(); })
    .then(function(r) {
      if (r.ok) {
        var csEl = document.getElementById('wiz-commission-status');
        if (csEl) csEl.innerHTML = '<strong>Status:</strong> Configured ✔';
        wizGoTo(7);
      }
    });
};

document.getElementById('wiz-go-dashboard').onclick = function() {
  document.querySelector('header').style.display = '';
  document.getElementById('view-wizard').style.display = 'none';
  showView('dashboard');
};

// =====================================================================
//  Start — check if onboarding needed
// =====================================================================
// Show loading skeleton while first fetch runs
document.getElementById('sensor-hero').innerHTML =
  '<div class="hero-card skeleton"><div class="hero-value">--</div><div class="hero-label">Loading…</div></div>' +
  '<div class="hero-card skeleton"><div class="hero-value">--</div><div class="hero-label">Loading…</div></div>';

fetch('/api/state')
  .then(function(res) { return res.json(); })
  .then(function(s) {
    if (!s.configured) {
      showWizard();
    } else {
      refresh();
      loadHistory();
      pollTimer = setInterval(refresh, 1000);
      historyTimer = setInterval(loadHistory, 30000);
    }
  })
  .catch(function() {
    refresh();
    loadHistory();
    pollTimer = setInterval(refresh, 1000);
    historyTimer = setInterval(loadHistory, 30000);
  });

// =====================================================================
//  Hardware Studio
// =====================================================================
document.getElementById('open-studio-btn').onclick = function() { showView('studio'); };
document.getElementById('studio-back-btn').onclick = function() { showView('settings'); };

var PIN_STATES = ['Available', 'System', 'Radio', 'Assigned'];
var PIN_CSS    = ['avail',     'sys',    'radio', 'used'];

function loadStudio() {
  loadNodes();
  loadRelays();
  loadPins();
  loadSensors();
  loadDrivers();
}

function loadNodes() {
  fetch('/api/hw/nodes').then(function(r){return r.json();}).then(function(nodes){
    var el = document.getElementById('node-list');
    if (!nodes.length) { el.innerHTML = '<p style="color:var(--muted)">No controllers registered.</p>'; return; }
    el.innerHTML = nodes.map(function(n){
      return '<div class="node-card">' +
        '<h4>' + esc(n.name) + '</h4>' +
        '<div class="node-meta">' +
          '<span>ID: ' + esc(n.id) + '</span>' +
          '<span>Board: ' + esc(n.board) + '</span>' +
          '<span>Addr: 0x' + ('0'+n.address.toString(16)).slice(-2).toUpperCase() + '</span>' +
          '<span>FW: ' + esc(n.fw) + '</span>' +
          '<span>Relays: ' + n.relays + '</span>' +
          '<span>Sensors: ' + n.sensors + '</span>' +
          '<span>Free pins: ' + n.free_pins + '</span>' +
          '<span>' + (n.online ? '&#x1F7E2; Online' : '&#x1F534; Offline') + '</span>' +
        '</div></div>';
    }).join('');
  });
}

function loadRelays() {
  fetch('/api/hw/relays').then(function(r){return r.json();}).then(function(relays){
    var el = document.getElementById('relay-list');
    el.innerHTML = relays.map(function(r){
      var status = r.assigned
        ? '<span class="driver-badge ok">' + esc(r.equipment) + '</span>'
        : '<span class="driver-badge na">Unassigned</span>';
      var stateClass = r.state === 'ON' ? 'ok' : 'na';
      return '<div class="driver-card">' +
        '<div class="driver-info"><h4>' + esc(r.label) + ' <small style="color:var(--muted)">(' + esc(r.pin) + ')</small></h4>' +
        '<p>' + (r.assigned ? esc(r.equipment) : 'Available for assignment') + '</p></div>' +
        '<span class="driver-badge ' + stateClass + '">' + r.state + '</span>' +
        '</div>';
    }).join('');
  });
}

function loadPins() {
  fetch('/api/hw/pins').then(function(r){return r.json();}).then(function(pins){
    var tbody = document.getElementById('pin-tbody');
    tbody.innerHTML = '';
    window._pinData = pins;
    renderPinTable(pins);
  });
}

function renderPinTable(pins) {
  var search = (document.getElementById('pin-search').value || '').toLowerCase();
  var filter = document.getElementById('pin-filter').value;
  var tbody  = document.getElementById('pin-tbody');
  var html   = '';
  for (var i = 0; i < pins.length; i++) {
    var p = pins[i];
    if (search && p.name.toLowerCase().indexOf(search) < 0 && p.used.toLowerCase().indexOf(search) < 0) continue;
    if (filter === 'available' && p.state !== 0) continue;
    if (filter === 'assigned' && p.state !== 3) continue;
    if (filter === 'reserved' && p.state !== 1 && p.state !== 2) continue;
    var caps = p.caps.split(',');
    var capHtml = caps.map(function(c){ return c ? '<span class="pin-cap-tag">' + c + '</span>' : ''; }).join('');
    var stClass = PIN_CSS[p.state] || '';
    var rowClass = p.state === 0 ? 'pin-available' : (p.state === 3 ? 'pin-assigned' : 'pin-reserved');
    html += '<tr class="' + rowClass + '">' +
      '<td><strong>' + esc(p.name) + '</strong></td>' +
      '<td>' + capHtml + '</td>' +
      '<td><span class="pin-state-badge ' + stClass + '">' + PIN_STATES[p.state] + '</span></td>' +
      '<td>' + esc(p.used) + '</td></tr>';
  }
  tbody.innerHTML = html || '<tr><td colspan="4" style="text-align:center;color:var(--muted)">No matching pins</td></tr>';
}

document.getElementById('pin-search').oninput = function() { if (window._pinData) renderPinTable(window._pinData); };
document.getElementById('pin-filter').onchange = function() { if (window._pinData) renderPinTable(window._pinData); };

var BUS_NAMES = {0: 'None', 1: 'GPIO', 2: 'I2C', 3: 'Analog'};

function loadDrivers() {
  fetch('/api/hw/drivers').then(function(r){return r.json();}).then(function(data){
    var drivers = Array.isArray(data) ? data : (data.drivers || []);
    window._driverData = drivers;

    var supported = drivers.filter(function(d){ return d.supported; });
    var planned   = drivers.filter(function(d){ return !d.supported; });

    var el = document.getElementById('driver-list');
    var html = '';

    if (supported.length) {
      html += '<div class="driver-section-label">Ready to use — add instances from the sensor form above</div>';
      html += supported.map(function(d){ return renderDriverCard(d); }).join('');
    }

    if (planned.length) {
      html += '<div class="driver-section-label" style="margin-top:16px">' +
        'Planned — requires firmware and hardware work before commissioning</div>';
      html += planned.map(function(d){ return renderDriverCard(d); }).join('');
    }

    el.innerHTML = html;
    updateSensorPinOptions();
  });
}

function renderDriverCard(d) {
  var meta = d.quantity ? '<small style="color:var(--muted)"> · ' + esc(d.quantity) +
    (d.unit ? ' (' + esc(d.unit) + ')' : '') +
    (d.max_per_node ? ' · max ' + d.max_per_node + '/node' : '') + '</small>' : '';

  var details = '<div class="driver-details">';
  details += '<span>Bus: ' + (BUS_NAMES[d.bus] || '?') + '</span>';
  if (d.fw_min) details += '<span>Min FW: v' + esc(d.fw_min) + '</span>';
  if (d.driver_ver) details += '<span>Def v' + d.driver_ver + '</span>';
  if (d.range_min !== undefined) {
    details += '<span>Range: ' + (d.range_min/10) + ' – ' + (d.range_max/10) +
      (d.unit ? ' ' + esc(d.unit) : '') + '</span>';
  }
  details += '</div>';

  var reqHtml = '';
  if (!d.supported) {
    reqHtml = '<div class="driver-requirements">';
    if (d.hw_req) reqHtml += '<div class="req-item"><strong>Hardware:</strong> ' + esc(d.hw_req) + '</div>';
    if (d.dev_note) reqHtml += '<div class="req-item"><strong>Firmware:</strong> ' + esc(d.dev_note) + '</div>';
    reqHtml += '</div>';
  } else if (d.hw_req) {
    reqHtml = '<div class="driver-hw-note">' + esc(d.hw_req) + '</div>';
  }

  return '<div class="driver-card' + (d.supported ? '' : ' driver-planned') + '">' +
    '<div class="driver-info"><h4>' + esc(d.name) + meta + '</h4>' +
    '<p>' + esc(d.desc) + '</p>' + details + reqHtml + '</div>' +
    '<span class="driver-badge ' + (d.supported ? 'ok' : 'na') + '">' +
      (d.supported ? 'Supported' : 'Engineering required') +
    '</span></div>';
}

function loadSensors() {
  // Load configured sensors
  fetch('/api/config/sensors').then(function(r){return r.json();}).then(function(sensors){
    var el = document.getElementById('sensor-list');
    if (!sensors.length) {
      el.innerHTML = '<p style="color:var(--muted)">No sensors configured.</p>';
    } else {
      el.innerHTML = sensors.map(function(s){
        var pinText = s.pin_name ? ' on ' + s.pin_name : '';
        var statusBadge = s.supported
          ? '<span class="driver-badge ok">Active</span>'
          : '<span class="driver-badge na">Awaiting firmware</span>';
        return '<div class="driver-card">' +
          '<div class="driver-info"><h4>' + esc(s.name) + '</h4>' +
          '<p>' + esc(s.driver || 'Unknown') + pinText + ' · ' + esc(s.node_id) + '</p></div>' +
          '<div style="display:flex;gap:8px;align-items:center">' + statusBadge +
          ' <button class="btn-danger" onclick="deleteSensor(\'' + s.id + '\')" style="font-size:12px;padding:4px 8px">Remove</button>' +
          '</div></div>';
      }).join('');
    }
  });

  // Populate sensor type dropdown (supported only)
  fetch('/api/hw/drivers').then(function(r){return r.json();}).then(function(data){
    var drivers = Array.isArray(data) ? data : (data.drivers || []);
    window._driverData = drivers;
    var sel = document.getElementById('sensor-type');
    sel.innerHTML = '';
    for (var i = 0; i < drivers.length; i++) {
      var d = drivers[i];
      var opt = document.createElement('option');
      opt.value = d.type;
      opt.textContent = d.name + (d.supported ? '' : ' (not yet supported)');
      opt.disabled = !d.supported;
      sel.appendChild(opt);
    }
    updateSensorPinOptions();
  });

  // Populate zone dropdown
  if (configData && configData.zones) {
    var zsel = document.getElementById('sensor-zone');
    zsel.innerHTML = '<option value="">No zone</option>';
    for (var i = 0; i < configData.zones.length; i++) {
      var opt = document.createElement('option');
      opt.value = configData.zones[i].id;
      opt.textContent = configData.zones[i].name;
      zsel.appendChild(opt);
    }
  }
}

var CAP_NAMES = {1: 'DI', 2: 'DO', 4: 'AI', 8: 'PWM', 16: 'INT'};

function updateSensorPinOptions() {
  if (!window._pinData || !window._driverData) return;
  var sel = document.getElementById('sensor-pin');
  sel.innerHTML = '';
  var pins = window._pinData;
  var typeVal = parseInt(document.getElementById('sensor-type').value);
  var driver = null;
  for (var d = 0; d < window._driverData.length; d++) {
    if (window._driverData[d].type === typeVal) { driver = window._driverData[d]; break; }
  }
  var capFilter = driver ? (CAP_NAMES[driver.pin_req] || '') : 'DI';
  var pinInfo = document.getElementById('sensor-pin-info');
  if (pinInfo) {
    if (driver && driver.pin_req === 0) {
      pinInfo.textContent = 'Bus device — no pin assignment needed';
      sel.style.display = 'none';
      sel.innerHTML = '<option value="255">None (bus)</option>';
    } else {
      pinInfo.textContent = capFilter ? 'Requires: ' + capFilter + ' capable pin' : '';
      sel.style.display = '';
    }
  }
  if (driver && driver.pin_req === 0) return;
  for (var i = 0; i < pins.length; i++) {
    var p = pins[i];
    if (p.state !== 0) continue;
    if (capFilter && p.caps.indexOf(capFilter) < 0) continue;
    var opt = document.createElement('option');
    opt.value = p.pin;
    opt.textContent = p.name + ' (' + p.caps + ')';
    sel.appendChild(opt);
  }
}

document.getElementById('sensor-type').onchange = updateSensorPinOptions;

document.getElementById('add-sensor-btn').onclick = function() {
  var name = document.getElementById('sensor-name').value.trim();
  if (!name) return;
  var id = 'sen_' + Date.now().toString(36);
  var data = {
    id: id,
    name: name,
    type: parseInt(document.getElementById('sensor-type').value),
    node_id: 'mega-001',
    pin: parseInt(document.getElementById('sensor-pin').value),
    zone_id: document.getElementById('sensor-zone').value,
    enabled: true
  };
  fetch('/api/config/sensors', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  }).then(function(res) { return res.json().then(function(j){ return {ok: res.ok, body: j}; }); })
    .then(function(r) {
      if (r.ok) {
        document.getElementById('sensor-name').value = '';
        loadStudio();
      } else {
        var el = document.getElementById('sensor-error');
        el.textContent = r.body.error || 'Failed to add sensor';
        el.style.display = '';
        setTimeout(function() { el.style.display = 'none'; }, 5000);
      }
    });
};

function deleteSensor(id) {
  fetch('/api/config/sensors?id=' + encodeURIComponent(id), { method: 'DELETE' })
    .then(function() { loadStudio(); });
}

function esc(s) {
  if (!s) return '';
  return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

// =====================================================================
//  Phase 9: Event log
// =====================================================================
function loadEvents() {
  var cat = document.getElementById('event-filter').value;
  var url = '/api/events?limit=50';
  if (cat) url += '&cat=' + cat;
  fetch(url).then(function(r) { return r.json(); }).then(function(data) {
    var el = document.getElementById('event-log-list');
    if (!data.events || data.events.length === 0) {
      el.innerHTML = '<p style="color:var(--muted);font-size:12px">No events recorded.</p>';
      return;
    }
    var html = '';
    data.events.forEach(function(e) {
      var mins = Math.floor(e.t / 60);
      var hrs = Math.floor(mins / 60);
      var timeStr = hrs > 0 ? (hrs + 'h ' + (mins % 60) + 'm') : (mins + 'm');
      html += '<div class="event-row">';
      html += '<span class="event-time">' + timeStr + '</span>';
      html += '<span class="event-cat">' + esc(e.cat) + '</span>';
      html += '<span class="event-msg">' + esc(e.msg) + '</span>';
      html += '</div>';
    });
    el.innerHTML = html;
  });
}

document.getElementById('load-events-btn').onclick = loadEvents;
document.getElementById('clear-events-btn').onclick = function() {
  if (!confirm('Clear all event logs?')) return;
  fetch('/api/events', { method: 'DELETE' }).then(function() { loadEvents(); });
};
document.getElementById('event-filter').onchange = loadEvents;

// =====================================================================
//  Phase 9: System info
// =====================================================================
function loadSystemInfo() {
  fetch('/api/system').then(function(r) { return r.json(); }).then(function(d) {
    var upH = Math.floor(d.uptime_s / 3600);
    var upM = Math.floor((d.uptime_s % 3600) / 60);
    var flashPct = Math.round(d.flash_used / d.flash_total * 100);
    var heapKB = Math.round(d.free_heap / 1024);
    var html = '<div class="sys-grid">';
    html += '<div class="sys-item"><div class="sys-label">Firmware</div><div class="sys-value">' + esc(d.firmware) + '</div></div>';
    html += '<div class="sys-item"><div class="sys-label">Uptime</div><div class="sys-value">' + upH + 'h ' + upM + 'm</div></div>';
    html += '<div class="sys-item"><div class="sys-label">Flash</div><div class="sys-value">' + flashPct + '%</div></div>';
    html += '<div class="sys-item"><div class="sys-label">Free Heap</div><div class="sys-value">' + heapKB + ' KB</div></div>';
    html += '<div class="sys-item"><div class="sys-label">Sensors</div><div class="sys-value">' + d.sensors + '</div></div>';
    html += '<div class="sys-item"><div class="sys-label">Equipment</div><div class="sys-value">' + d.equipment + '</div></div>';
    html += '<div class="sys-item"><div class="sys-label">Rules</div><div class="sys-value">' + d.rules + '</div></div>';
    html += '<div class="sys-item"><div class="sys-label">Auth</div><div class="sys-value">' + (d.auth_enabled ? 'On' : 'Off') + '</div></div>';
    html += '</div>';
    document.getElementById('system-info').innerHTML = html;

    // Show auth section for technicians (role 2) or when auth is off
    if (d.my_role >= 2 || !d.auth_enabled) {
      document.getElementById('auth-section').style.display = '';
      loadAuth();
    }
  });
}
document.getElementById('load-system-btn').onclick = loadSystemInfo;

// =====================================================================
//  Phase 9: Backup & Restore
// =====================================================================
document.getElementById('backup-btn').onclick = function() {
  document.getElementById('backup-status').textContent = 'Downloading...';
  fetch('/api/config/backup').then(function(r) { return r.json(); }).then(function(data) {
    var blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
    var a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'lorasense-backup.json';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(a.href);
    document.getElementById('backup-status').textContent = 'Backup downloaded.';
  });
};

document.getElementById('restore-file').onchange = function(ev) {
  var file = ev.target.files[0];
  if (!file) return;
  if (!confirm('Restore configuration from ' + file.name + '? The gateway will reboot.')) {
    ev.target.value = '';
    return;
  }
  var reader = new FileReader();
  reader.onload = function(e) {
    document.getElementById('backup-status').textContent = 'Restoring...';
    fetch('/api/config/restore', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: e.target.result
    }).then(function(r) { return r.json(); }).then(function(d) {
      if (d.ok) {
        document.getElementById('backup-status').textContent = 'Restored ' + d.restored + ' configs. Rebooting...';
      } else {
        document.getElementById('backup-status').textContent = 'Error: ' + (d.error || 'restore failed');
      }
    });
  };
  reader.readAsText(file);
  ev.target.value = '';
};

// =====================================================================
//  Phase 9: Auth management (technician only)
// =====================================================================
function loadAuth() {
  fetch('/api/auth').then(function(r) {
    if (r.status === 403 || r.status === 401) {
      document.getElementById('auth-section').style.display = 'none';
      return null;
    }
    return r.json();
  }).then(function(d) {
    if (!d) return;
    document.getElementById('auth-enabled').checked = d.enabled;
    var html = '';
    if (d.users) {
      d.users.forEach(function(u) {
        var roleLabel = u.role >= 2 ? 'Technician' : 'User';
        html += '<div class="auth-user-row">';
        html += '<strong>' + esc(u.username) + '</strong>';
        html += '<span style="color:var(--muted)">' + roleLabel + '</span>';
        html += '<span style="color:var(--muted)">' + (u.active ? 'Active' : 'Disabled') + '</span>';
        html += '</div>';
      });
    }
    if (!html) html = '<p style="color:var(--muted);font-size:12px">No users configured.</p>';
    document.getElementById('auth-users').innerHTML = html;
  });
}

document.getElementById('save-auth-btn').onclick = function() {
  var payload = { enabled: document.getElementById('auth-enabled').checked };
  fetch('/api/auth', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  }).then(function(r) { return r.json(); }).then(function(d) {
    if (d.ok) loadSystemInfo();
  });
};

// =====================================================================
//  Phase 12: Pull-to-refresh gesture
// =====================================================================
(function() {
  var pullEl = document.getElementById('pull-indicator');
  var startY = 0;
  var pulling = false;
  var threshold = 60;

  document.addEventListener('touchstart', function(e) {
    if (currentView !== 'dashboard') return;
    if (window.scrollY > 5) return;
    startY = e.touches[0].clientY;
    pulling = true;
  }, { passive: true });

  document.addEventListener('touchmove', function(e) {
    if (!pulling) return;
    var dy = e.touches[0].clientY - startY;
    if (dy > 0 && dy < 120) {
      pullEl.classList.add('pulling');
      var pct = Math.min(dy / threshold, 1);
      pullEl.querySelector('.pull-spinner').style.transform = 'rotate(' + (pct * 180) + 'deg)';
    } else {
      pullEl.classList.remove('pulling');
    }
  }, { passive: true });

  document.addEventListener('touchend', function() {
    if (!pulling) return;
    pulling = false;
    var wasPulling = pullEl.classList.contains('pulling');
    if (wasPulling) {
      pullEl.classList.remove('pulling');
      pullEl.classList.add('refreshing');
      refresh();
      loadHistory();
      setTimeout(function() {
        pullEl.classList.remove('refreshing');
      }, 800);
    }
  }, { passive: true });
})();

// =====================================================================
//  Phase 14: Collapsible sections
// =====================================================================
(function() {
  var STORAGE_KEY = 'ls_collapsed';
  var collapsed = {};
  try {
    var raw = localStorage.getItem(STORAGE_KEY);
    if (raw) collapsed = JSON.parse(raw);
  } catch(e) {}

  function saveCollapsed() {
    try { localStorage.setItem(STORAGE_KEY, JSON.stringify(collapsed)); } catch(e) {}
  }

  function applyState(sectId) {
    var wrap = document.getElementById(sectId);
    if (!wrap) return;
    if (collapsed[sectId]) {
      wrap.classList.add('collapsed');
    } else {
      wrap.classList.remove('collapsed');
    }
  }

  var sects = document.querySelectorAll('.sect-hdr');
  for (var i = 0; i < sects.length; i++) {
    (function(hdr) {
      var sectId = hdr.getAttribute('data-sect');
      applyState(sectId);
      hdr.onclick = function() {
        collapsed[sectId] = !collapsed[sectId];
        applyState(sectId);
        saveCollapsed();
        if (!collapsed[sectId] && sectId === 'sect-chart') drawChart();
      };
    })(sects[i]);
  }
})();

// =====================================================================
//  Phase 14: Quick controls bar
// =====================================================================
var lastQuickDevices = null;

function drawQuickBar(s) {
  var bar = document.getElementById('quick-bar');
  if (!bar) return;
  var devices = s.devices;
  if (!devices || devices.length === 0) {
    bar.classList.remove('visible');
    return;
  }
  var relayDevices = [];
  var anyOn = false;
  for (var i = 0; i < devices.length; i++) {
    var d = devices[i];
    if (d.output === null || d.output === undefined) continue;
    relayDevices.push(d);
    if (d.on) anyOn = true;
  }
  if (relayDevices.length === 0) {
    bar.classList.remove('visible');
    return;
  }

  if (currentView !== 'dashboard') {
    bar.classList.remove('visible');
    return;
  }

  var html = '';
  if (anyOn) {
    html += '<button class="quick-pill qp-alloff" id="qb-alloff" title="Turn all off">' +
      '<span class="qp-dot"></span>All off</button>';
  }
  for (var i = 0; i < relayDevices.length; i++) {
    var d = relayDevices[i];
    var cls = d.on ? ' qp-on' : '';
    html += '<button class="quick-pill' + cls + '" data-qb-id="' + escHtml(d.id) + '" data-qb-on="' + (d.on ? '1' : '0') + '">' +
      '<span class="qp-dot"></span>' + escHtml(d.name) + '</button>';
  }

  bar.innerHTML = html;
  bar.classList.add('visible');
}

document.getElementById('quick-bar').onclick = function(e) {
  var pill = e.target.closest('.quick-pill');
  if (!pill) return;
  if (pill.id === 'qb-alloff') {
    allOff();
    return;
  }
  var devId = pill.getAttribute('data-qb-id');
  var isOn = pill.getAttribute('data-qb-on') === '1';
  if (devId) setDevice(devId, isOn ? 'off' : 'on');
};

function allOff() {
  fetch('/api/all-off', { method: 'POST' })
    .then(function() { showToast('All devices turned off', 'success'); refresh(); })
    .catch(function() {
      showToast('All-off failed, turning off individually', 'warning');
      if (lastQuickDevices) {
        for (var i = 0; i < lastQuickDevices.length; i++) {
          var d = lastQuickDevices[i];
          if (d.on && d.output !== null && d.output !== undefined) {
            setDevice(d.id, 'off');
          }
        }
      }
    });
}

// =====================================================================
//  Phase 15: Toast notifications
// =====================================================================
var TOAST_MAX = 3;
var TOAST_DURATION = 3500;
var TOAST_ICONS = {
  success: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><polyline points="20 6 9 17 4 12"/></svg>',
  error:   '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/></svg>',
  warning: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/></svg>',
  info:    '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/></svg>'
};

function showToast(message, type) {
  type = type || 'info';
  var container = document.getElementById('toast-container');
  if (!container) return;

  var toast = document.createElement('div');
  toast.className = 'toast toast-' + type;
  toast.innerHTML = '<span class="toast-icon">' + (TOAST_ICONS[type] || TOAST_ICONS.info) + '</span>' +
    '<span class="toast-msg">' + escHtml(message) + '</span>';

  container.appendChild(toast);

  // Limit visible toasts
  var toasts = container.querySelectorAll('.toast:not(.toast-out)');
  while (toasts.length > TOAST_MAX) {
    dismissToast(toasts[0]);
    toasts = container.querySelectorAll('.toast:not(.toast-out)');
  }

  // Auto-dismiss
  var timer = setTimeout(function() { dismissToast(toast); }, TOAST_DURATION);

  // Click to dismiss
  toast.onclick = function() {
    clearTimeout(timer);
    dismissToast(toast);
  };

  // Swipe to dismiss (mobile)
  var startX = 0;
  toast.addEventListener('touchstart', function(e) {
    startX = e.touches[0].clientX;
  }, { passive: true });
  toast.addEventListener('touchend', function(e) {
    var dx = e.changedTouches[0].clientX - startX;
    if (dx > 60) {
      clearTimeout(timer);
      dismissToast(toast);
    }
  }, { passive: true });
}

function dismissToast(toast) {
  if (!toast || toast.classList.contains('toast-out')) return;
  toast.classList.add('toast-out');
  setTimeout(function() {
    if (toast.parentNode) toast.parentNode.removeChild(toast);
  }, 300);
}
