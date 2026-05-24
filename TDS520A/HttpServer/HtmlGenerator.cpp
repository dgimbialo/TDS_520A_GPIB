// HttpServer/HtmlGenerator.cpp
#include "pch.h"
#include "HtmlGenerator.h"
#include <sstream>
#include <iomanip>

namespace HtmlGen
{

std::string MainPage(const std::string& localIp, uint16_t wsPort)
{
    // Self-contained HTML page with inline CSS and minimal vanilla JS.
    // Connects via WebSocket, receives JSON frames, draws on <canvas>.
    // No external dependencies.
    std::ostringstream ss;
    ss << R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TDS 520A - Live Oscilloscope</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { background: #111; color: #aaa; font-family: monospace; }
  #toolbar { display: flex; align-items: center; gap: 10px; padding: 6px 10px;
             background: #1a1a1a; border-bottom: 1px solid #333; flex-wrap: wrap; }
  #toolbar label { font-size: 13px; }
  #toolbar select, #toolbar button {
    background: #2a2a2a; color: #ccc; border: 1px solid #555;
    padding: 3px 8px; border-radius: 3px; cursor: pointer; font-size: 13px;
  }
  #toolbar button:hover { background: #3a3a3a; }
  #status { margin-left: auto; font-size: 12px; color: #6f6; }
  #scopeinfo { font-size: 11px; color: #888; }
  #wrapcanvas { flex: 1; position: relative; }
  canvas { display: block; width: 100%; height: calc(100vh - 48px);
           background: #000; }
  #overlay { position: absolute; top: 4px; right: 8px;
             font-size: 12px; color: #cc0; pointer-events: none; }
</style>
</head>
<body>
<div id="toolbar">
  <label>Channel:
    <select id="chSel">
      <option value="1">CH1</option>
      <option value="2">CH2</option>
      <option value="3">CH3</option>
      <option value="4">CH4</option>
    </select>
  </label>
  <button id="btnSingle">Single</button>
  <button id="btnRun">Run</button>
  <button id="btnStop">Stop</button>
  <span id="scopeinfo">--</span>
  <span id="status">Connecting...</span>
</div>
<div id="wrapcanvas">
  <canvas id="wfCanvas"></canvas>
  <div id="overlay"></div>
</div>
<script>
(function() {
  'use strict';
  var canvas = document.getElementById('wfCanvas');
  var ctx    = canvas.getContext('2d');
  var status = document.getElementById('status');
  var info   = document.getElementById('scopeinfo');
  var overlay= document.getElementById('overlay');
  var ws, lastData = null;
  var fps = 0, frameCount = 0, lastFpsTime = Date.now();

  function resize() {
    canvas.width  = canvas.clientWidth;
    canvas.height = canvas.clientHeight;
    if (lastData) draw(lastData);
  }
  window.addEventListener('resize', resize);
  resize();

  var GRID_H = 10, GRID_V = 8;

  function drawGrid(W, H) {
    ctx.strokeStyle = '#1a3a1a';
    ctx.lineWidth = 1;
    for (var i = 0; i <= GRID_H; i++) {
      var x = W * i / GRID_H;
      ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, H); ctx.stroke();
    }
    for (var i = 0; i <= GRID_V; i++) {
      var y = H * i / GRID_V;
      ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W, y); ctx.stroke();
    }
    // Center cross ticks
    ctx.strokeStyle = '#2a5a2a';
    var cx = W/2, cy = H/2;
    for (var i = 0; i <= GRID_H; i++) {
      var x = W * i / GRID_H;
      ctx.beginPath(); ctx.moveTo(x, cy-5); ctx.lineTo(x, cy+5); ctx.stroke();
    }
    for (var i = 0; i <= GRID_V; i++) {
      var y = H * i / GRID_V;
      ctx.beginPath(); ctx.moveTo(cx-5, y); ctx.lineTo(cx+5, y); ctx.stroke();
    }
  }

  function draw(d) {
    var W = canvas.width, H = canvas.height;
    ctx.clearRect(0, 0, W, H);
    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, W, H);
    drawGrid(W, H);

    if (!d.samples || d.samples.length < 2) return;

    var n = d.samples.length;
    var vMin = d.yMin, vMax = d.yMax;
    var vSpan = vMax - vMin || 1;
    var margin = 0.08;

    function toScreenY(v) {
      return H * (1 - margin) - (v - vMin) / vSpan * H * (1 - 2*margin);
    }

    // Waveform
    ctx.beginPath();
    ctx.strokeStyle = '#00ff00';
    ctx.lineWidth = 1.5;
    for (var i = 0; i < n; i++) {
      var x = W * i / (n - 1);
      var y = toScreenY(d.samples[i]);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();

    // Trigger line (y=0 or trigger level)
    var trigY = toScreenY(0);
    ctx.beginPath();
    ctx.strokeStyle = '#ffff00';
    ctx.setLineDash([5, 5]);
    ctx.moveTo(0, trigY); ctx.lineTo(W, trigY);
    ctx.stroke();
    ctx.setLineDash([]);

    // Labels
    ctx.fillStyle = '#aaa';
    ctx.font = '12px monospace';
    if (d.secPerDiv) {
      var s = d.secPerDiv;
      var label = s < 1e-6 ? (s*1e9).toFixed(1)+' ns/div'
                : s < 1e-3 ? (s*1e6).toFixed(1)+' us/div'
                : s < 1    ? (s*1e3).toFixed(2)+' ms/div'
                :             s.toFixed(2)+' s/div';
      ctx.fillText(label, 6, H - 8);
    }
    if (d.voltsPerDiv) {
      var v = d.voltsPerDiv;
      var vlabel = v < 1 ? (v*1000).toFixed(1)+' mV/div' : v.toFixed(3)+' V/div';
      ctx.fillText(vlabel, 6, 18);
    }
    ctx.fillStyle = '#cc0';
    ctx.fillText('FPS: '+fps.toFixed(1), W-90, H-8);
  }

  function connect() {
    ws = new WebSocket('ws://)";
    ss << localIp << ":" << wsPort;
    ss << R"(/ws');
    ws.binaryType = 'arraybuffer';

    ws.onopen = function() {
      status.textContent = 'Connected';
      status.style.color = '#6f6';
    };
    ws.onclose = function() {
      status.textContent = 'Disconnected – retrying...';
      status.style.color = '#f66';
      setTimeout(connect, 2000);
    };
    ws.onerror = function() {
      status.textContent = 'Error';
      status.style.color = '#f66';
    };
    ws.onmessage = function(ev) {
      try {
        var d = JSON.parse(ev.data);
        if (d.type === 'waveform') {
          lastData = d;
          draw(d);
          frameCount++;
          var now = Date.now();
          if (now - lastFpsTime >= 1000) {
            fps = frameCount * 1000 / (now - lastFpsTime);
            frameCount = 0; lastFpsTime = now;
          }
        } else if (d.type === 'status') {
          info.textContent = d.idn + '  |  ' + 'CH'+d.channel
            + '  ' + (d.voltsPerDiv||0).toFixed(3)+'V/div'
            + '  Acq: '+d.acqRate+'/s';
        }
      } catch(e) {}
    };
  }

  // Controls
  document.getElementById('chSel').onchange = function() {
    if (ws && ws.readyState === 1)
      ws.send(JSON.stringify({cmd:'channel', ch: parseInt(this.value)}));
  };
  document.getElementById('btnRun').onclick = function() {
    if (ws && ws.readyState === 1) ws.send(JSON.stringify({cmd:'run'}));
  };
  document.getElementById('btnStop').onclick = function() {
    if (ws && ws.readyState === 1) ws.send(JSON.stringify({cmd:'stop'}));
  };
  document.getElementById('btnSingle').onclick = function() {
    if (ws && ws.readyState === 1) ws.send(JSON.stringify({cmd:'single'}));
  };

  connect();
})();
</script>
</body>
</html>)";
    return ss.str();
}

std::string StatusJson(const std::string& idn, ScopeChannel ch,
                        double voltsPerDiv, double secPerDiv,
                        uint32_t acqRate, bool connected)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "{\"type\":\"status\","
       << "\"idn\":\"" << idn << "\","
       << "\"channel\":" << static_cast<int>(ch) << ","
       << "\"voltsPerDiv\":" << voltsPerDiv << ","
       << "\"secPerDiv\":"   << secPerDiv   << ","
       << "\"acqRate\":"     << acqRate     << ","
       << "\"connected\":"   << (connected ? "true" : "false")
       << "}";
    return ss.str();
}

std::string WaveformJson(const DecodedWaveform& wave)
{
    // Send voltage samples only (time is implicit: index * xIncr + xZero)
    // This minimizes bandwidth for mobile clients.
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "{\"type\":\"waveform\","
       << "\"seq\":" << wave.SequenceNumber << ","
       << "\"yMin\":" << wave.YMin << ","
       << "\"yMax\":" << wave.YMax << ","
       << "\"xMin\":" << wave.XMin << ","
       << "\"xMax\":" << wave.XMax << ","
       << "\"voltsPerDiv\":" << wave.VoltsPerDiv << ","
       << "\"secPerDiv\":"   << wave.SecPerDiv   << ","
       << "\"samples\":[";

    // Downsample to at most 1000 points for web client
    const size_t n = wave.Samples.size();
    const size_t step = std::max(size_t(1), n / 1000);
    bool first = true;
    for (size_t i = 0; i < n; i += step)
    {
        if (!first) ss << ",";
        ss << wave.Samples[i].voltage;
        first = false;
    }
    ss << "]}";
    return ss.str();
}

} // namespace HtmlGen
