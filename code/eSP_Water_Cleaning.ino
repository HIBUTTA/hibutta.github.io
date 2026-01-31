/* PROJECT: Dual-MCU Env Monitor (Final)
   DEVICE:  ESP32 (Server & Logger)
   AUTHOR:  [Your Name]
   DATE:    2025
   
   FEATURES:
   - Receives formatted data string from Arduino
   - Hosts Wi-Fi Access Point (Test_Temp)
   - Auto-logs every 4 hours + Manual Button
   - Dashboard with Live Clock & History Table
   - CSV Download (Sri Lanka Timezone + DD/MM/YYYY format)
*/

#include <WiFi.h>
#include <WebServer.h>
#include <time.h>

const char* apSSID = "Test_Temp";
const char* apPassword = "12345678";

WebServer server(80);

// Serial2 (RX=16) - Use Voltage Divider on Input!
#define UART_BAUD    9600
#define UART_RX_PIN  16
#define UART_TX_PIN  17 

// Live values storage
float valInside = NAN;
float valOutside = NAN;
float valHum = NAN;

// Logging Configuration
const unsigned long AUTO_LOG_INTERVAL = 4 * 60 * 60 * 1000UL;
// 4 Hours
unsigned long lastLogTime = 0;

// ---------- Circular History Buffer ----------
#define HISTORY_LEN 600   
struct Sample {
  time_t epoch;
  float inside;
  float outside;
  float hum;
};
Sample history[HISTORY_LEN];
int histWriteIndex = 0;
int histCount = 0;
// Helper: Formats time as DD/MM/YYYY HH:MM:SS
// Uses the Timezone set in setup()
String formatTime(time_t rawtime) {
  struct tm * ti;
  ti = localtime(&rawtime); 
  char buffer[30];
  sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d", 
          ti->tm_mday, ti->tm_mon + 1, ti->tm_year + 1900, 
          ti->tm_hour, ti->tm_min, ti->tm_sec);
  return String(buffer);
}

void addHistory(float inV, float outV, float humV) {
  time_t now;
  time(&now);

  history[histWriteIndex].epoch = now;
  history[histWriteIndex].inside = inV;
  history[histWriteIndex].outside = outV;
  history[histWriteIndex].hum = humV;
  
  histWriteIndex = (histWriteIndex + 1) % HISTORY_LEN;
  if (histCount < HISTORY_LEN) histCount++;
  Serial.printf("LOGGED: %s | In:%.2f Out:%.2f Hum:%.2f\n", formatTime(now).c_str(), inV, outV, humV);
}

// ---------- Web Interface & Handlers ----------

void handleRoot();
void handleData();
void handleHistory();
void handleDownloadCSV();
void handleRecordNow();
void handleSetTime();

String buildPageHTML() {
  String html = R"rawliteral(
<!doctype html>
<html>
<head>
  <meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Env Monitor - [Your Name]</title>
  <style>
    :root{ 
      --bg:#0b0c15; --panel:#151922; 
      --text:#e1e6eb; --muted:#88909c; 
      --c-in:#ff6b6b; --c-out:#4ecdc4; --c-hum:#a388ee; 
    }
    body{margin:0; font-family: 'Segoe UI', Roboto, Helvetica, sans-serif; background:var(--bg); color:var(--text);}
    
    .container{ max-width: 1000px; margin: 0 auto; padding: 20px; }
    
    /* Header */
    .header { text-align:center; margin-bottom: 30px; }
    .header h1 { margin:0; font-weight:300; letter-spacing: 2px; text-transform:uppercase; font-size:22px; color:var(--muted); }
    #liveClock { font-size: 48px; font-weight: 700; margin: 10px 0; background: -webkit-linear-gradient(#fff, #aaa); -webkit-background-clip: text; -webkit-text-fill-color: transparent; font-variant-numeric: tabular-nums; }
    
    /* Grid System */
    .grid { display: grid;
    grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 20px; margin-bottom: 30px; }
    
    .card { background: var(--panel);
    border-radius: 12px; padding: 20px; border: 1px solid rgba(255,255,255,0.05); position: relative; overflow: hidden; box-shadow: 0 4px 20px rgba(0,0,0,0.3);
    }
    .card::before { content:''; position:absolute; top:0; left:0; width:4px; height:100%;
    }
    
    .card.in::before { background: var(--c-in); }
    .card.out::before { background: var(--c-out);
    }
    .card.hum::before { background: var(--c-hum); }

    .card-label { font-size: 12px; text-transform: uppercase; letter-spacing: 1px;
    color: var(--muted); display:flex; justify-content:space-between; }
    .card-val { font-size: 42px; font-weight: 700; margin-top: 10px;
    }
    .card.in .card-val { color: var(--c-in); }
    .card.out .card-val { color: var(--c-out);
    }
    .card.hum .card-val { color: var(--c-hum); }
    .unit { font-size: 18px; opacity: 0.7;
    font-weight: 400; }

    /* Buttons */
    .controls { display: flex; flex-wrap: wrap; gap: 10px;
    justify-content: center; margin-bottom: 30px; padding: 20px; background: var(--panel); border-radius: 12px;
    }
    
    button { 
      padding: 12px 20px;
      border: none; border-radius: 6px; 
      font-weight: 600; cursor: pointer; color: #0b0c15; font-size: 14px; transition: 0.2s;
    }
    .btn-sync { background: #e0e0e0; }
    .btn-rec { background: #ffce00;
    }
    .btn-dl { background: #4ecdc4; }
    .btn-ref { background: transparent; border: 1px solid var(--muted);
    color: var(--muted); }
    button:hover { transform: translateY(-2px); opacity: 0.9;
    }

    /* Data Table */
    .hist-panel { background: var(--panel); border-radius: 12px; padding: 20px;
    overflow-x: auto; }
    table { width: 100%; border-collapse: collapse; min-width: 600px;
    }
    th { text-align: left; color: var(--muted); font-size: 12px; padding-bottom: 10px; border-bottom: 1px solid rgba(255,255,255,0.1);
    }
    td { padding: 12px 0; border-bottom: 1px solid rgba(255,255,255,0.03); font-size: 14px;
    }
    
    .footer { text-align: center; color: var(--muted); font-size: 12px; margin-top: 40px; opacity: 0.5;
    }

  </style>
</head>
<body>

<div class="container">
  
  <div class="header">
    <h1>UASB Reacter</h1>
    <div id="liveClock">--:--:--</div>
    <div style="font-size:13px; color:var(--muted)">[Your Name] 2025</div>
  </div>

  <div class="grid">
    <div class="card in">
      <div class="card-label"><span>Indoor Temp</span><span>DS18B20</span></div>
      <div class="card-val" id="valIn">-- <span class="unit">°C</span></div>
    </div>
    <div class="card out">
      <div class="card-label"><span>Outdoor Temp</span><span>DHT22</span></div>
      <div class="card-val" id="valOut">-- <span class="unit">°C</span></div>
    </div>
    <div class="card hum">
      <div class="card-label"><span>Outdoor Humidity</span><span>DHT22</span></div>
      <div class="card-val" id="valHum">-- <span class="unit">%</span></div>
    </div>
  </div>

  <div class="controls">
    <button class="btn-sync" onclick="syncBrowserTime()">Sync Time</button>
    <button class="btn-rec" onclick="recordNow()">Record Data Point</button>
    <button class="btn-dl" onclick="window.location.href='/download.csv'">Download CSV</button>
    <button class="btn-ref" onclick="updateAll()">Refresh</button>
  </div>

  <div class="hist-panel">
    <div style="font-size:14px;
    font-weight:700; margin-bottom:10px; color:var(--text)">Data History</div>
    <table id="recentTable">
      <thead><tr><th>Time</th><th>Indoor</th><th>Outdoor</th><th>Humidity</th></tr></thead>
      <tbody id="recentBody"><tr><td colspan="4">Loading...</td></tr></tbody>
    </table>
  </div>

  <div class="footer">[Your Name] © 2025 — All rights reserved.</div>

</div>

<script>
// Live Digital Clock
function updateClock() {
  const now = new Date();
  document.getElementById('liveClock').innerText = now.toLocaleTimeString();
}
setInterval(updateClock, 1000);
updateClock();

// Data Fetching
async function fetchData() {
  try { return await (await fetch('/data?_='+Date.now())).json(); } catch(e){ return null; }
}
async function fetchHistory() {
  try { return await (await fetch('/history?_='+Date.now())).json(); } catch(e){ return []; }
}
async function recordNow() {
  if(!confirm("Capture current reading to history?")) return;
  await fetch('/record_now');
  
  setTimeout(updateAll, 500); 
}
async function syncBrowserTime() {
  // Sends Browser Epoch time to ESP32
  const nowSecs = Math.floor(Date.now() / 1000);
  await fetch('/set_time?t=' + nowSecs);
  alert("System calibrated to Browser Time!");
  updateAll();
}

// UI Rendering
function renderLatest(j) {
  if(!j) return;
  document.getElementById('valIn').innerHTML = (j.in!==null ? j.in.toFixed(2) : '--') + ' <span class="unit">°C</span>';
  document.getElementById('valOut').innerHTML = (j.out!==null ? j.out.toFixed(2) : '--') + ' <span class="unit">°C</span>';
  document.getElementById('valHum').innerHTML = (j.hum!==null ? j.hum.toFixed(1) : '--') + ' <span class="unit">%</span>';
}

function renderTable(data) {
  const body = document.getElementById('recentBody');
  body.innerHTML = '';
  if (!data || data.length == 0) {
    body.innerHTML = '<tr><td colspan="4" style="color:var(--muted); text-align:center;">No history log yet</td></tr>';
    return;
  }
  const start = Math.max(0, data.length - 20);
  const slice = data.slice(start).reverse();
  slice.forEach(s => {
    const tr = document.createElement('tr');
    const dateObj = new Date(s.epoch * 1000);
    // JS automatically handles local time, but we rely on ESP32 CSV mainly
    const timeStr = dateObj.toLocaleDateString() + ' ' + dateObj.toLocaleTimeString([], {hour:'2-digit', minute:'2-digit'});

    tr.innerHTML = `
      <td>${timeStr}</td>
      <td>${s.inside===null ? 'ERR' : Number(s.inside).toFixed(2)} °C</td>
      <td>${s.outside===null ? 'ERR' : Number(s.outside).toFixed(2)} °C</td>
      <td>${s.hum===null ? 'ERR' : Number(s.hum).toFixed(1)} %</td>
    `;
  
    body.appendChild(tr);
  });
}

async function updateAll() {
  const latest = await fetchData();
  renderLatest(latest);
  const hist = await fetchHistory();
  renderTable(hist);
}

// Init
document.addEventListener('DOMContentLoaded', function(){
  updateAll();
  setInterval(updateAll, 2000); // Live poll
});
</script>
</body>
</html>
  )rawliteral";

  html.replace("__HISTORY__", String(HISTORY_LEN));
  return html;
}

// ---------- API Handlers ----------

void handleRoot() { server.send(200, "text/html", buildPageHTML()); }

void handleData() {
  // Returns JSON of current live values
  String j = "{";
  j += "\"in\":" + (isnan(valInside) ? "null" : String(valInside, 2)) + ",";
  j += "\"out\":" + (isnan(valOutside) ? "null" : String(valOutside, 2)) + ",";
  j += "\"hum\":" + (isnan(valHum) ? "null" : String(valHum, 1));
  j += "}";
  server.send(200, "application/json", j);
}

void handleRecordNow() {
  Serial.println("Manual Record Triggered");
  addHistory(valInside, valOutside, valHum);
  server.send(200, "text/plain", "Recorded");
}

void handleSetTime() {
  if (server.hasArg("t")) {
  
    long t = server.arg("t").toInt();
    struct timeval tv;
    tv.tv_sec = t; tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    Serial.printf("Time Synced via Web: %ld\n", t);
    server.send(200, "text/plain", "OK");
  } else server.send(400, "text/plain", "Error");
}

void handleHistory() {
  String out = "[";
  int count = histCount;
  if (count > 0) {
    int start = (histWriteIndex - count + HISTORY_LEN) % HISTORY_LEN;
    for (int i = 0; i < count; ++i) {
      int idx = (start + i) % HISTORY_LEN;
      if (i) out += ",";
      out += "{";
      out += "\"epoch\":" + String(history[idx].epoch) + ",";
      out += "\"inside\":" + (isnan(history[idx].inside) ? "null" : String(history[idx].inside, 2)) + ",";
      out += "\"outside\":" + (isnan(history[idx].outside) ? "null" : String(history[idx].outside, 2)) + ",";
      out += "\"hum\":" + (isnan(history[idx].hum) ? "null" : String(history[idx].hum, 1));
      out += "}";
    }
  }
  out += "]";
  server.send(200, "application/json", out);
}

void handleDownloadCSV() {
  // Generate CSV File
  String csv = "Timestamp,Inside_C,Outside_C,Humidity_%\r\n";
  int count = histCount;
  if (count > 0) {
    int start = (histWriteIndex - count + HISTORY_LEN) % HISTORY_LEN;
    for (int i = 0; i < count; ++i) {
      int idx = (start + i) % HISTORY_LEN;
      // Using helper to format correct Sri Lanka Time
      csv += formatTime(history[idx].epoch) + ",";
      csv += (isnan(history[idx].inside) ? "" : String(history[idx].inside, 2)) + ",";
      csv += (isnan(history[idx].outside) ? "" : String(history[idx].outside, 2)) + ",";
      csv += (isnan(history[idx].hum) ? "" : String(history[idx].hum, 1));
      csv += "\r\n";
    }
  }
  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=\"env_data.csv\"");
  server.send(200, "text/csv", csv);
}

String readLineFromSerial2() {
  static String line = "";
  while (Serial2.available()) {
    char c = (char)Serial2.read();
    if (c == '\r') continue;
    if (c == '\n') {
      String out = line;
      line = "";
      return out;
    } else {
      line += c;
      if (line.length() > 400) line = "";
    }
  }
  return String();
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  // --- TIMEZONE CONFIGURATION ---
  // "IST-5:30" sets the ESP32 internal clock to UTC+05:30
  setenv("TZ", "IST-5:30", 1);
  tzset();
  // Init default time (Year 1970)
  struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 0; settimeofday(&tv, NULL);
  // Init history
  histWriteIndex = 0; histCount = 0;

  WiFi.softAP(apSSID, apPassword);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/history", handleHistory);
  server.on("/download.csv", handleDownloadCSV);
  server.on("/record_now", handleRecordNow);
  server.on("/set_time", handleSetTime);
  
  server.begin();
  Serial.println("System Started.");
  lastLogTime = millis();
}

void loop() {
  server.handleClient();
  // 1. Read Serial Frame from Arduino
  String s = readLineFromSerial2();
  if (s.length() > 0) {
    s.trim();
    if (s.startsWith("#")) {
      int idxINS = s.indexOf("INS:");
      int idxOUT = s.indexOf("OUT:");
      int idxHUM = s.indexOf("HUM:");
      
      if (idxINS >= 0 && idxOUT >= 0 && idxHUM >= 0) {
        // Robust Parsing (Works even if length changes)
        String inP  = s.substring(idxINS + 4, s.indexOf(',', idxINS));
        String outP = s.substring(idxOUT + 4, s.indexOf(',', idxOUT));
        String humP = s.substring(idxHUM + 4);
        
        inP.trim(); outP.trim(); humP.trim();
        if (inP.equalsIgnoreCase("ERR")) valInside = NAN; else valInside = inP.toFloat();
        if (outP.equalsIgnoreCase("ERR")) valOutside = NAN; else valOutside = outP.toFloat();
        if (humP.equalsIgnoreCase("ERR")) valHum = NAN; else valHum = humP.toFloat();

        // Debug to Serial Monitor
        Serial.printf("RX -> In:%.2f  Out:%.2f  Hum:%.1f\n", valInside, valOutside, valHum);
      }
    }
  }

  // 2. Auto-Log Logic
  if (millis() - lastLogTime >= AUTO_LOG_INTERVAL) {
    lastLogTime = millis();
    Serial.println("Auto-Logging Data (Timer)...");
    addHistory(valInside, valOutside, valHum);
  }
  
  delay(10);
}