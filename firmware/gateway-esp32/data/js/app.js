// LoRaSense dashboard + settings application
// Polls /api/state for dashboard, /api/config for settings

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

// Profile terminology per environment type
var PROFILE_TERMS = {
  home:       { zone: 'Room', zones: 'Rooms', sensor: 'Sensor', equipment: 'Device' },
  farm:       { zone: 'Field', zones: 'Fields', sensor: 'Sensor', equipment: 'Equipment' },
  greenhouse: { zone: 'Bay', zones: 'Bays', sensor: 'Sensor', equipment: 'Equipment' },
  industrial: { zone: 'Area', zones: 'Areas', sensor: 'Instrument', equipment: 'Asset' },
  lab:        { zone: 'Station', zones: 'Stations', sensor: 'Instrument', equipment: 'Apparatus' },
  custom:     { zone: 'Zone', zones: 'Zones', sensor: 'Sensor', equipment: 'Equipment' }
};
var terms = PROFILE_TERMS.home;

// =====================================================================
//  Navigation
// =====================================================================
document.getElementById('settings-btn').onclick = function() { showView('settings'); };
document.getElementById('back-btn').onclick = function() { showView('dashboard'); };

function showView(view) {
  currentView = view;
  document.getElementById('view-dashboard').style.display = view === 'dashboard' ? '' : 'none';
  document.getElementById('view-settings').style.display = view === 'settings' ? '' : 'none';
  document.getElementById('view-studio').style.display = view === 'studio' ? '' : 'none';

  if (view === 'settings') {
    clearInterval(pollTimer);
    clearInterval(historyTimer);
    loadConfig();
    loadActionLog();
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

var categoryIcons = {
  lighting: '☀', fan: '❁', pump: '⬆', socket: '⚡',
  irrigation: '\u{1F4A7}', heating: '\u{1F525}', other: '⚙'
};

function signalLabel(rssi) {
  if (rssi >= -60) return 'Strong';
  if (rssi >= -80) return 'Good';
  if (rssi >= -100) return 'Weak';
  return 'No signal';
}

var sensorMeta = {
  1: { name: 'DHT11', readings: [
    { key: 'temperature_c', label: 'Temperature', unit: ' °C', icon: '\u{1F321}', decimals: 1 },
    { key: 'humidity_pct',  label: 'Humidity',    unit: ' %',  icon: '\u{1F4A7}', decimals: 0 }
  ]},
  2: { name: 'DHT22', readings: [
    { key: 'temperature_c', label: 'Temperature', unit: ' °C', icon: '\u{1F321}', decimals: 1 },
    { key: 'humidity_pct',  label: 'Humidity',    unit: ' %',  icon: '\u{1F4A7}', decimals: 0 }
  ]},
  5: { name: 'Analog', readings: [
    { key: 'analog_pct', label: 'Analog', unit: ' %', icon: '\u{1F4CA}', decimals: 1 }
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

  var html = '';
  for (var i = 0; i < sensors.length; i++) {
    var sen = sensors[i];
    var meta = sensorMeta[sen.type];
    if (!meta) {
      html += '<div class="hero-card hero-error">' +
        '<div class="hero-icon">\u{1F4E1}</div>' +
        '<div class="hero-value">--</div>' +
        '<div class="hero-label">' + escHtml(sen.name) + '</div>' +
        '<div class="hero-sub disconnected">Unsupported type ' + sen.type + '</div>' +
      '</div>';
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
        statusHtml = '<div class="hero-sub disconnected">\u{26A0} Node offline</div>';
      } else if (quality === 'invalid') {
        statusHtml = '<div class="hero-sub disconnected">\u{26A0} Out of range</div>';
      } else if (quality === 'missing') {
        statusHtml = '<div class="hero-sub disconnected">Awaiting data</div>';
      }

      if (sen.readings && sen.readings[rd.key] !== undefined) {
        val = parseFloat(sen.readings[rd.key]).toFixed(rd.decimals);
      }

      var unitLabel = rd.unit;
      if (sen.unit && readings.length === 1) unitLabel = ' ' + sen.unit;

      html += '<div class="hero-card' + qClass + '">' +
        '<div class="hero-top">' +
          '<div class="hero-icon">' + rd.icon + '</div>' +
          qualityBadge(quality) +
        '</div>' +
        '<div class="hero-value">' + val + '<span class="hero-unit">' + escHtml(unitLabel) + '</span></div>' +
        '<div class="hero-label">' + escHtml(sensorLabel) + ' — ' + rd.label + '</div>' +
        (ageText ? '<div class="hero-age">' + ageText + '</div>' : '') +
        statusHtml +
      '</div>';
    }
  }
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
    ageItem = '<div class="status-item' + ageCls + '">\u{1F4E1} ' + ageStr + '</div>';
  }

  bar.innerHTML =
    '<div class="status-item"><span class="status-dot ' + dotClass + '"></span>' + nodeText + '</div>' +
    '<div class="spacer"></div>' +
    '<div class="status-item"><span class="' + syncClass + '">' + syncText + '</span></div>' +
    ageItem +
    '<div class="status-item">\u{1F4F6} ' + rssiText + '</div>' +
    '<div class="status-item">⏱ ' + formatUptime(s.uptime_s) + '</div>';
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
    div.innerHTML = '<span class="ar-icon">⚡</span> <span class="ar-name">' +
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
      icon.textContent = categoryIcons[d.category] || '⚙';
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
  var body = 'id=' + encodeURIComponent(id) + '&state=' + encodeURIComponent(state);
  fetch('/api/devices', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: body
  }).then(function() { refresh(); });
}

function refresh() {
  fetch('/api/state')
    .then(function(res) { return res.json(); })
    .then(function(s) {
      document.getElementById('banner').style.display = 'none';
      if (s.env_name) {
        document.getElementById('env-name').textContent = s.env_name;
      }
      if (s.env_type) {
        applyTheme(s.env_type);
        terms = PROFILE_TERMS[s.env_type] || PROFILE_TERMS.custom;
      }
      hasSensors = s.sensors && s.sensors.length > 0;
      drawSensorHero(s);
      drawStatusBar(s);
      drawActiveRules(s);
      drawDevices(s);
    })
    .catch(function() {
      document.getElementById('banner').style.display = 'block';
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
  var profile = ENV_PROFILES[wizData.env_type];
  if (!profile) { hint.innerHTML = ''; return; }
  hint.innerHTML = '<strong>Suggested for ' + wizData.env_type + ':</strong><br>' +
    '<span style="color:var(--muted)">Sensors: ' + profile.sensors.join(', ') + '<br>' +
    'Equipment: ' + profile.equipment.join(', ') + '</span><br>' +
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
document.getElementById('wiz-next-2').onclick = function() { updateProfileHint(); wizGoTo(3); };

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

function loadDrivers() {
  fetch('/api/hw/drivers').then(function(r){return r.json();}).then(function(data){
    var drivers = Array.isArray(data) ? data : (data.drivers || []);
    window._driverData = drivers;
    var el = document.getElementById('driver-list');
    el.innerHTML = drivers.map(function(d){
      var meta = d.quantity ? '<small style="color:var(--muted)"> · ' + esc(d.quantity) +
        (d.unit ? ' (' + esc(d.unit) + ')' : '') +
        (d.max_per_node ? ' · max ' + d.max_per_node + '/node' : '') + '</small>' : '';
      return '<div class="driver-card">' +
        '<div class="driver-info"><h4>' + esc(d.name) + meta + '</h4><p>' + esc(d.desc) + '</p></div>' +
        '<span class="driver-badge ' + (d.supported ? 'ok' : 'na') + '">' +
          (d.supported ? 'Supported' : 'Not yet') +
        '</span></div>';
    }).join('');
    updateSensorPinOptions();
  });
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
