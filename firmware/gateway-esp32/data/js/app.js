// LoRaSense dashboard + settings application
// Polls /api/state for dashboard, /api/config for settings

var pollTimer = null;
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

  if (view === 'settings') {
    clearInterval(pollTimer);
    loadConfig();
  } else {
    refresh();
    pollTimer = setInterval(refresh, 1000);
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

function drawStats(s) {
  var box = document.getElementById('stats');
  var tempVal = s.temperature_c !== undefined ? parseFloat(s.temperature_c).toFixed(1) + ' °C' : '--';
  var humVal = s.humidity_pct !== undefined ? parseFloat(s.humidity_pct).toFixed(1) + ' %' : '--';
  var nodeClass = s.node_online ? ' online' : ' offline';
  var nodeText = s.node_online ? 'Online' : 'Offline';

  box.innerHTML =
    '<div class="stat"><div class="label">Temperature</div><div class="value">' + tempVal + '</div></div>' +
    '<div class="stat"><div class="label">Humidity</div><div class="value">' + humVal + '</div></div>' +
    '<div class="stat"><div class="label">Node</div><div class="value' + nodeClass + '">' + nodeText + '</div></div>' +
    '<div class="stat"><div class="label">Uptime</div><div class="value">' + formatUptime(s.uptime_s) + '</div></div>';
}

function drawDevices(devices) {
  var box = document.getElementById('devices');
  if (!devices || devices.length === 0) {
    box.innerHTML = '<div class="empty-state">No equipment configured.<br>Tap ⚙ to add equipment.</div>';
    return;
  }

  box.innerHTML = '';
  for (var i = 0; i < devices.length; i++) {
    var d = devices[i];
    var hasOutput = d.output !== null && d.output !== undefined;
    var pending = d.status === 'PENDING';
    var stateText, stateClass;

    if (!hasOutput) {
      stateText = 'No relay';
      stateClass = 'state-off';
    } else if (pending) {
      stateText = 'Waiting…';
      stateClass = 'state-pending';
    } else if (d.status === 'FAILED') {
      stateText = 'Failed';
      stateClass = 'state-failed';
    } else {
      stateText = d.on ? 'ON' : 'OFF';
      stateClass = d.on ? 'state-on' : 'state-off';
    }

    var row = document.createElement('div');
    row.className = 'device';

    var btnHtml = '';
    if (hasOutput) {
      btnHtml = '<button class="' + (d.on ? 'turn-off' : '') + '"' +
        (pending ? ' disabled' : '') + '>' +
        (d.on ? 'Turn off' : 'Turn on') + '</button>';
    }

    row.innerHTML =
      '<div class="info">' +
        '<div class="name"></div>' +
        '<div class="meta">' + d.category + (d.zone_id ? ' · ' + d.zone_id : '') + '</div>' +
        '<div class="' + stateClass + '">' + stateText + '</div>' +
      '</div>' + btnHtml;

    row.querySelector('.name').textContent = d.name;

    if (hasOutput) {
      (function(id, on) {
        row.querySelector('button').onclick = function() {
          setDevice(id, on ? 'off' : 'on');
        };
      })(d.id, d.on);
    }
    box.appendChild(row);
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
      drawStats(s);
      drawDevices(s.devices);
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

  // Available outputs for equipment form
  var eqOutput = document.getElementById('eq-output');
  eqOutput.innerHTML = '<option value="255">None (monitor only)</option>';
  for (var i = 0; i < cfg.available_outputs.length; i++) {
    var opt = document.createElement('option');
    opt.value = cfg.available_outputs[i];
    opt.textContent = 'Relay ' + (cfg.available_outputs[i] + 1);
    eqOutput.appendChild(opt);
  }

  // Equipment list
  var eqList = document.getElementById('equipment-list');
  eqList.innerHTML = '';
  for (var i = 0; i < cfg.equipment.length; i++) {
    var eq = cfg.equipment[i];
    var div = document.createElement('div');
    div.className = 'eq-item';
    var outputText = eq.output !== undefined && eq.output !== null ? 'Relay ' + (eq.output + 1) : 'No relay';
    div.innerHTML =
      '<div><div class="eq-name">' + escHtml(eq.name) + '</div>' +
      '<div class="eq-meta">' + eq.category + ' · ' + outputText + '</div></div>';
    var delBtn = document.createElement('button');
    delBtn.className = 'btn-danger';
    delBtn.textContent = 'Remove';
    (function(id) {
      delBtn.onclick = function() { deleteEquipment(id); };
    })(eq.id);
    div.appendChild(delBtn);
    eqList.appendChild(div);
  }
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
    output: parseInt(document.getElementById('eq-output').value)
  };
  if (data.output === 255) delete data.output;
  fetch('/api/config/equipment', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  }).then(function(res) { return res.json(); })
    .then(function(r) {
      if (r.ok) {
        document.getElementById('eq-name').value = '';
        loadConfig();
      }
    });
};

function deleteEquipment(id) {
  fetch('/api/config/equipment?id=' + encodeURIComponent(id), { method: 'DELETE' })
    .then(function() { loadConfig(); });
}

// =====================================================================
//  Start
// =====================================================================
refresh();
pollTimer = setInterval(refresh, 1000);
