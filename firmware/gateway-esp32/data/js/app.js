// LoRaSense dashboard application
// Polls /api/state every second, redraws UI
// Only shows ON after the node has confirmed it

var pollTimer = null;
var lastState = null;

function formatUptime(sec) {
  if (sec < 60) return sec + 's';
  if (sec < 3600) return Math.floor(sec / 60) + 'm';
  return Math.floor(sec / 3600) + 'h ' + Math.floor((sec % 3600) / 60) + 'm';
}

function drawStats(s) {
  var box = document.getElementById('stats');
  var tempClass = '';
  var tempVal = '--';
  if (s.sensor_flags !== undefined) {
    if (s.sensor_flags & 1) {
      tempVal = s.temperature_c.toFixed(1) + ' °C';
    } else {
      tempVal = 'No sensor';
      tempClass = ' offline';
    }
  } else {
    tempVal = s.temperature_c.toFixed(1) + ' °C';
  }

  var nodeClass = s.node_online ? ' online' : ' offline';
  var nodeText = s.node_online ? 'Online' : 'Offline';

  box.innerHTML =
    '<div class="stat"><div class="label">Temperature</div><div class="value' + tempClass + '">' + tempVal + '</div></div>' +
    '<div class="stat"><div class="label">Node</div><div class="value' + nodeClass + '">' + nodeText + '</div></div>' +
    '<div class="stat"><div class="label">Uptime</div><div class="value">' + formatUptime(s.uptime_s) + '</div></div>';
}

function drawDevices(devices) {
  var box = document.getElementById('devices');
  if (!devices || devices.length === 0) {
    box.innerHTML = '<div class="empty-state">No equipment configured</div>';
    return;
  }

  box.innerHTML = '';
  for (var i = 0; i < devices.length; i++) {
    var d = devices[i];
    var pending = d.status === 'PENDING';
    var stateText, stateClass;

    if (pending) {
      stateText = 'Waiting for node…';
      stateClass = 'state-pending';
    } else if (d.status === 'FAILED') {
      stateText = 'Failed – no reply';
      stateClass = 'state-failed';
    } else {
      stateText = d.on ? 'ON' : 'OFF';
      stateClass = d.on ? 'state-on' : 'state-off';
    }

    var row = document.createElement('div');
    row.className = 'device';
    row.innerHTML =
      '<div class="info">' +
        '<div class="name"></div>' +
        '<div class="' + stateClass + '">' + stateText + '</div>' +
      '</div>' +
      '<button class="' + (d.on ? 'turn-off' : '') + '"' +
        (pending ? ' disabled' : '') + '>' +
        (d.on ? 'Turn off' : 'Turn on') +
      '</button>';
    row.querySelector('.name').textContent = d.name;
    (function(id, on) {
      row.querySelector('button').onclick = function() {
        setDevice(id, on ? 'off' : 'on');
      };
    })(d.id, d.on);
    box.appendChild(row);
  }
}

function setDevice(id, state) {
  var body = 'id=' + encodeURIComponent(id) + '&state=' + encodeURIComponent(state);
  fetch('/api/devices', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: body
  }).then(function() {
    refresh();
  });
}

function refresh() {
  fetch('/api/state')
    .then(function(res) { return res.json(); })
    .then(function(s) {
      lastState = s;
      document.getElementById('banner').style.display = 'none';
      drawStats(s);
      drawDevices(s.devices);
    })
    .catch(function() {
      document.getElementById('banner').style.display = 'block';
    });
}

refresh();
pollTimer = setInterval(refresh, 1000);
