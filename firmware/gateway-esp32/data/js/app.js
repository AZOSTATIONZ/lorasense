// LoRaSense dashboard + settings application
// Polls /api/state for dashboard, /api/config for settings

var pollTimer = null;
var historyTimer = null;
var currentView = 'dashboard';
var configData = null;

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
  ]}
};

function drawSensorHero(s) {
  var box = document.getElementById('sensor-hero');
  var sensors = s.sensors;

  if (!sensors || sensors.length === 0) {
    box.innerHTML = '<div class="empty-state">No sensors configured.<br>' +
      '<a href="#" onclick="showView(\'studio\');return false;">Add a sensor in Hardware Studio</a></div>';
    return;
  }

  var html = '';
  for (var i = 0; i < sensors.length; i++) {
    var sen = sensors[i];
    var meta = sensorMeta[sen.type];
    if (!meta) {
      html += '<div class="hero-card">' +
        '<div class="hero-icon">📡</div>' +
        '<div class="hero-value">--</div>' +
        '<div class="hero-label">' + escHtml(sen.name) + '</div>' +
        '<div class="hero-sub">Unsupported driver</div>' +
      '</div>';
      continue;
    }

    var readings = meta.readings;
    for (var r = 0; r < readings.length; r++) {
      var rd = readings[r];
      var val = '--';
      var sub = '';
      if (!sen.connected) {
        sub = '<div class="hero-sub disconnected">Disconnected</div>';
      } else if (sen.readings && sen.readings[rd.key] !== undefined) {
        val = parseFloat(sen.readings[rd.key]).toFixed(rd.decimals);
      }
      html += '<div class="hero-card' + (!sen.connected ? ' stale' : '') + '">' +
        '<div class="hero-icon">' + rd.icon + '</div>' +
        '<div class="hero-value">' + val + '<span style="font-size:18px;font-weight:400">' + rd.unit + '</span></div>' +
        '<div class="hero-label">' + rd.label + '</div>' +
        sub +
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

  bar.innerHTML =
    '<div class="status-item"><span class="status-dot ' + dotClass + '"></span>' + nodeText + '</div>' +
    '<div class="spacer"></div>' +
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
    box.innerHTML = '<div class="empty-state">No equipment configured.<br>' +
      '<a href="#" onclick="showView(\'settings\');return false;">Add equipment in Settings</a></div>';
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
      title.textContent = zoneId === '_none' ? 'Unassigned' : (zoneNames[zoneId] || zoneId);
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
      } else if (pending) {
        metaDiv.innerHTML = '<span class="state-pending">Sending…</span>';
      } else if (d.status === 'FAILED') {
        metaDiv.innerHTML = '<span class="state-failed">Failed</span>';
      } else {
        metaDiv.innerHTML = d.on ? '<span class="state-on">ON</span>' : '<span class="state-off">OFF</span>';
      }
      info.appendChild(metaDiv);
      row.appendChild(info);

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
      row.appendChild(toggle);

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

  var sensorNames = ['Temperature', 'Humidity'];
  var condNames   = ['above', 'below'];
  var unitNames   = ['°C', '%'];

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

    div.innerHTML =
      '<div class="rule-info">' +
        '<div class="rule-name">' + escHtml(r.name) + (r.triggered ? ' <span class="rule-active">ACTIVE</span>' : '') + '</div>' +
        '<div class="rule-desc">' + desc + ' → ' + escHtml(eqName) + '</div>' +
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
    enabled: true
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
//  Onboarding Wizard
// =====================================================================
var wizStep = 1;
var wizData = {
  env_name: 'My Home',
  env_type: 'home',
  zones: [],
  equipment: []
};
var wizRelayUsed = {};

function showWizard() {
  document.getElementById('view-dashboard').style.display = 'none';
  document.getElementById('view-settings').style.display = 'none';
  document.getElementById('view-wizard').style.display = '';
  document.querySelector('header').style.display = 'none';
  clearInterval(pollTimer);
  wizGoTo(1);
}

function wizGoTo(step) {
  wizStep = step;
  for (var i = 1; i <= 5; i++) {
    document.getElementById('wiz-' + i).style.display = (i === step) ? '' : 'none';
  }
  document.getElementById('wizard-bar').style.width = (step * 20) + '%';
}

// Step 1: name
document.getElementById('wiz-next-1').onclick = function() {
  var name = document.getElementById('wiz-env-name').value.trim();
  if (!name) return;
  wizData.env_name = name;
  wizGoTo(2);
};

// Step 2: type
var typeGrid = document.getElementById('wiz-type-grid');
typeGrid.onclick = function(e) {
  var btn = e.target.closest('.wiz-type-btn');
  if (!btn) return;
  var btns = typeGrid.querySelectorAll('.wiz-type-btn');
  for (var i = 0; i < btns.length; i++) btns[i].className = 'wiz-type-btn';
  btn.className = 'wiz-type-btn selected';
  wizData.env_type = btn.getAttribute('data-type');
};
document.getElementById('wiz-back-2').onclick = function() { wizGoTo(1); };
document.getElementById('wiz-next-2').onclick = function() { wizGoTo(3); };

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
  // Populate zone dropdown for equipment step
  var sel = document.getElementById('wiz-eq-zone');
  sel.innerHTML = '';
  for (var i = 0; i < wizData.zones.length; i++) {
    var opt = document.createElement('option');
    opt.value = wizData.zones[i].id;
    opt.textContent = wizData.zones[i].name;
    sel.appendChild(opt);
  }
  updateWizRelayDropdown();
  renderWizEquipment();
  wizGoTo(4);
};

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

document.getElementById('wiz-finish').onclick = function() {
  var payload = {
    env_name: wizData.env_name,
    env_type: wizData.env_type,
    zones: wizData.zones,
    equipment: []
  };
  for (var i = 0; i < wizData.equipment.length; i++) {
    var eq = wizData.equipment[i];
    var obj = { id: eq.id, name: eq.name, category: eq.category, zone_id: eq.zone_id };
    if (eq.output !== 255) obj.output = eq.output;
    payload.equipment.push(obj);
  }

  fetch('/api/onboarding', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  }).then(function(res) { return res.json(); })
    .then(function(r) {
      if (r.ok) wizGoTo(5);
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
  fetch('/api/hw/drivers').then(function(r){return r.json();}).then(function(drivers){
    var el = document.getElementById('driver-list');
    el.innerHTML = drivers.map(function(d){
      return '<div class="driver-card">' +
        '<div class="driver-info"><h4>' + esc(d.name) + '</h4><p>' + esc(d.desc) + '</p></div>' +
        '<span class="driver-badge ' + (d.supported ? 'ok' : 'na') + '">' +
          (d.supported ? 'Supported' : 'Not yet') +
        '</span></div>';
    }).join('');
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
  fetch('/api/hw/drivers').then(function(r){return r.json();}).then(function(drivers){
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

function updateSensorPinOptions() {
  if (!window._pinData) return;
  var sel = document.getElementById('sensor-pin');
  sel.innerHTML = '';
  var pins = window._pinData;
  for (var i = 0; i < pins.length; i++) {
    var p = pins[i];
    if (p.state !== 0) continue;
    if (p.caps.indexOf('DI') < 0) continue;
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
