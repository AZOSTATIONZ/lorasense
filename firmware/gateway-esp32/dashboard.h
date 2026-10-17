// The dashboard web page, stored in the ESP32's flash as one string.
// Everything (HTML + CSS + JavaScript) is inside - no internet, no CDN,
// because the phone has no internet while it is on the ESP32's network.
//
// In a later step we move this into separate files (web/index.html,
// app.js, styles.css) stored in LittleFS so it is easier to edit.

#pragma once

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Home Control</title>
<style>
  :root {
    --bg: #f4f5f7; --card: #ffffff; --text: #1c1f24; --muted: #6b7280;
    --on: #16a34a; --off: #9ca3af; --pending: #d97706; --fail: #dc2626;
    --border: #e5e7eb;
  }
  @media (prefers-color-scheme: dark) {
    :root { --bg: #111418; --card: #1b1f25; --text: #e8eaed; --muted: #9aa0a6; --border: #2a2f36; }
  }
  * { box-sizing: border-box; }
  body { margin: 0; font-family: system-ui, -apple-system, Segoe UI, Roboto, sans-serif;
         background: var(--bg); color: var(--text); }
  header { padding: 20px 16px 8px; }
  h1 { margin: 0; font-size: 22px; }
  .sub { color: var(--muted); font-size: 13px; margin-top: 4px; }
  main { padding: 8px 16px 32px; max-width: 640px; margin: 0 auto; }
  .stats { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; margin-bottom: 16px; }
  .stat { background: var(--card); border: 1px solid var(--border); border-radius: 12px; padding: 12px; }
  .stat .label { font-size: 12px; color: var(--muted); }
  .stat .value { font-size: 20px; font-weight: 600; margin-top: 4px; }
  .device { display: flex; align-items: center; justify-content: space-between;
            background: var(--card); border: 1px solid var(--border); border-radius: 12px;
            padding: 14px 16px; margin-bottom: 10px; }
  .device .name { font-weight: 600; }
  .device .state { font-size: 13px; margin-top: 3px; }
  .state.on { color: var(--on); } .state.off { color: var(--off); }
  .state.pending { color: var(--pending); } .state.failed { color: var(--fail); }
  button { min-width: 84px; padding: 10px 14px; border-radius: 10px; border: 0;
           font-size: 15px; font-weight: 600; cursor: pointer; color: #fff; background: var(--on); }
  button.turn-off { background: #4b5563; }
  button:disabled { opacity: 0.5; cursor: wait; }
  .banner { display: none; background: var(--fail); color: #fff; padding: 10px 16px;
            border-radius: 10px; margin-bottom: 12px; font-size: 14px; }
</style>
</head>
<body>
<header>
  <h1>Home Control</h1>
  <div class="sub">Local LoRa gateway &middot; no internet needed</div>
</header>
<main>
  <div class="banner" id="banner">Lost connection to the gateway. Retrying...</div>

  <div class="stats">
    <div class="stat"><div class="label">Temperature</div><div class="value" id="temp">--</div></div>
    <div class="stat"><div class="label">Node</div><div class="value" id="node">--</div></div>
    <div class="stat"><div class="label">Uptime</div><div class="value" id="uptime">--</div></div>
  </div>

  <div id="devices"></div>
</main>

<script>
// Ask the gateway for the current state every second and redraw.
// The page ONLY shows "ON" when the gateway says the node confirmed it.

async function refresh() {
  try {
    const res = await fetch('/api/state');
    const s = await res.json();
    document.getElementById('banner').style.display = 'none';
    document.getElementById('temp').textContent = s.temperature_c.toFixed(1) + ' °C';
    document.getElementById('node').textContent = s.node_online ? 'Online' : 'Offline';
    document.getElementById('uptime').textContent = formatUptime(s.uptime_s);
    drawDevices(s.devices);
  } catch (e) {
    document.getElementById('banner').style.display = 'block';
  }
}

function formatUptime(sec) {
  if (sec < 60) return sec + 's';
  if (sec < 3600) return Math.floor(sec / 60) + 'm';
  return Math.floor(sec / 3600) + 'h ' + Math.floor((sec % 3600) / 60) + 'm';
}

function drawDevices(devices) {
  const box = document.getElementById('devices');
  box.innerHTML = '';
  for (const d of devices) {
    const pending = d.status === 'PENDING';
    let stateText, stateClass;
    if (pending)                   { stateText = 'Waiting for node...'; stateClass = 'pending'; }
    else if (d.status === 'FAILED'){ stateText = 'Failed - no reply';   stateClass = 'failed'; }
    else                           { stateText = d.on ? 'ON' : 'OFF';   stateClass = d.on ? 'on' : 'off'; }

    const row = document.createElement('div');
    row.className = 'device';
    row.innerHTML =
      '<div><div class="name"></div><div class="state ' + stateClass + '">' + stateText + '</div></div>' +
      '<button class="' + (d.on ? 'turn-off' : '') + '"' + (pending ? ' disabled' : '') + '>' +
      (d.on ? 'Turn off' : 'Turn on') + '</button>';
    row.querySelector('.name').textContent = d.name;
    row.querySelector('button').onclick = () => setDevice(d.id, d.on ? 'off' : 'on');
    box.appendChild(row);
  }
}

async function setDevice(id, state) {
  const body = new URLSearchParams({ id: id, state: state });
  await fetch('/api/devices', { method: 'POST', body: body });
  refresh();   // show PENDING straight away
}

refresh();
setInterval(refresh, 1000);
</script>
</body>
</html>
)rawliteral";
