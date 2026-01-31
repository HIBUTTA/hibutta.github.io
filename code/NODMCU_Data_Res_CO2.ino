/* BOARD: NodeMCU (ESP8266)
   PROJECT: CO2-Sync
   DEVELOPER: [Your Name]
   ROLE: Web Server & Data Logger
   FEATURES: Retroactive Time Sync, Immediate Log, 2-Hr Loop
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>     
#include <sys/time.h> 

const char* apSSID = "CO2-Sync";
const char* apPassword = "12345678";

ESP8266WebServer server(80);
#define UART_BAUD 9600

// --- Configuration ---
// Auto-save interval: 2 hours = 7200 seconds
const uint32_t ARCHIVE_INTERVAL_MS = 7200UL * 1000UL;
#define HISTORY_LEN 600

struct Sample {
  time_t epoch; 
  float co2;
  float temp;
};

Sample history[HISTORY_LEN];
int histWriteIndex = 0;
int histCount = 0;

float curCO2 = NAN;
float curTemp = NAN;

// Timer Logic
unsigned long lastLogMillis = 0;
bool timeSynced = false;
bool firstPointCaptured = false;

// --- Time Helper ---
String formatDateTime(time_t rawtime) {
  struct tm * ti;
  ti = localtime(&rawtime); 
  char buffer[30];
  sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d", 
          ti->tm_mday, ti->tm_mon + 1, ti->tm_year + 1900, 
          ti->tm_hour, ti->tm_min, ti->tm_sec);
  return String(buffer);
}

// Function to save data
void addHistory(float co2v, float tempv, bool isAuto) {
  time_t now;
  time(&now);
  // Get current time (might be 1970 if not synced yet)

  history[histWriteIndex].epoch = now;
  history[histWriteIndex].co2 = co2v;
  history[histWriteIndex].temp = tempv;
  
  histWriteIndex = (histWriteIndex + 1) % HISTORY_LEN;
  if (histCount < HISTORY_LEN) histCount++;
  // Only reset the 2-hour timer if this was an AUTO save
  if (isAuto) {
    lastLogMillis = millis();
  }
  
  Serial.printf("LOGGED (%s): %s | %.0f, %.2f\n", isAuto?"AUTO":"MANUAL", formatDateTime(now).c_str(), co2v, tempv);
}

// --- Serial Helper ---
bool isNumeric(const String &s) {
  for (size_t i = 0; i < s.length(); ++i) {
    if (!isdigit(s[i]) && s[i] != '.' && s[i] != '-' && s[i] != '+') return false;
  }
  return true;
}

String readSerialLine() {
  static String line = "";
  while (Serial.available()) {
    char c = (char)Serial.read();
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

// --- HTML Dashboard ---
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
<head>
  <meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
  <title>CO₂-Sync</title>
  <style>
    :root { --bg: #111827; --card: #1f2937; --text: #f3f4f6; --muted: #9ca3af; --accent: #3b82f6; --success: #10b981; --warn: #f59e0b; --danger: #ef4444; --border: #374151; }
    body { margin:0; font-family:'Segoe UI',sans-serif; background:var(--bg); color:var(--text); display:flex; flex-direction:column; min-height:100vh; }
    .container { max-width:650px; margin:0 auto; padding:20px; width:100%; flex:1; }
    .header { text-align:center; margin-bottom:25px; border-bottom:1px solid var(--border); padding-bottom:20px; }
    h1 { margin:0; font-size:28px; font-weight:800; letter-spacing:1px; background: -webkit-linear-gradient(45deg, #60a5fa, 
    #34d399); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
    .status-bar { display:flex; justify-content:center; gap:15px; margin-top:10px; flex-wrap:wrap;
    }
    .badge { font-family:monospace; font-size:14px; color:var(--muted); background:rgba(255,255,255,0.05); padding:5px 12px; border-radius:15px; border:1px solid rgba(255,255,255,0.1);
    }
    .grid { display:grid; grid-template-columns:1fr 1fr; gap:15px; margin-bottom:25px; }
    .card { background:var(--card); padding:20px; border-radius:16px;
    text-align:center; border:1px solid var(--border); box-shadow:0 10px 15px -3px rgba(0,0,0,0.3); }
    .card-label { font-size:12px; text-transform:uppercase; color:var(--muted); letter-spacing:1px; margin-bottom:8px;
    }
    .card-value { font-size:36px; font-weight:800; }
    .unit { font-size:14px; font-weight:600; color:var(--muted);
    }
    .safe { color:var(--success); } .warn { color:var(--warn); } .danger { color:var(--danger);
    }
    .controls { display:grid; grid-template-columns:1fr 1fr; gap:10px; margin-bottom:25px; }
    button { padding:14px; border:none; border-radius:10px;
    font-weight:600; cursor:pointer; font-size:13px; transition:0.2s; color:#fff; }
    .btn-sync { background:var(--accent); grid-column: span 2;
    }
    .btn-save { background:var(--card); border:1px solid var(--muted); color:var(--text); }
    .btn-dl { background:var(--success); color:#064e3b;
    }
    button:hover { opacity:0.9; transform:translateY(-1px); }
    .table-container { background:var(--card); border-radius:12px; border:1px solid var(--border); overflow:hidden;
    }
    .table-header { padding:12px; background:rgba(0,0,0,0.2); font-weight:600; border-bottom:1px solid var(--border); font-size:14px; color:var(--muted); text-align:center;
    }
    .scroll-area { max-height:250px; overflow-y:auto; }
    table { width:100%; border-collapse:collapse; font-size:13px;
    }
    th, td { padding:10px; text-align:center; border-bottom:1px solid var(--border); }
    th { color:var(--muted); font-size:11px;
    position:sticky; top:0; background:var(--card); }
    .footer { text-align:center; padding:20px; border-top:1px solid var(--border); color:var(--muted); font-size:13px; margin-top:20px; opacity:0.8;
    }
  </style>
</head>
<body>
<div class="container">
  <div class="header">
    <h1>🌍 CO₂-Sync</h1>
    <div class="status-bar">
      <div class="badge" id="liveDT">System Ready</div>
      <div class="badge timer" id="nextSave">Auto-Save: --:--:--</div>
    </div>
  </div>
  <div class="grid">
    <div class="card">
      <div class="card-label">Air Quality</div>
      <div class="card-value" id="dispCO2">--</div>
      <div class="unit">ppm</div>
    </div>
    <div class="card">
      <div class="card-label">Temperature</div>
      <div class="card-value" id="dispTemp">--</div>
      <div class="unit">°C</div>
    </div>
  </div>
  <div class="controls">
    <button class="btn-sync" id="btnSync" onclick="syncTime()">⚡ TAP TO SYNC TIME</button>
    <button class="btn-save" onclick="archiveNow()">Record Point Now</button>
    <button class="btn-dl" onclick="window.location.href='/download.csv'">Download CSV</button>
  </div>
  <div class="table-container">
    <div class="table-header">Recorded History</div>
    <div class="scroll-area">
      <table id="histTable">
        <thead><tr><th>Date & Time</th><th>CO₂</th><th>Temp</th></tr></thead>
        <tbody id="histBody"><tr><td colspan="3" style="padding:15px;
        color:var(--muted)">Waiting for data...</td></tr></tbody>
      </table>
    </div>
  </div>
</div>
<div class="footer">
  Developed by <strong>[Your Name]</strong>
</div>
<script>
function secToHMS(s) {
  if (s < 0) return "Logging...";
  var h = Math.floor(s/3600); s -= h*3600;
  var m = Math.floor(s/60); s -= m*60;
  return String(h).padStart(2,'0')+":"+String(m).padStart(2,'0')+":"+String(s).padStart(2,'0');
}
function updateClock() {
  const now = new Date();
  const opt = { weekday: 'short', month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit', second: '2-digit' };
  document.getElementById('liveDT').innerText = now.toLocaleDateString('en-GB', opt);
}
setInterval(updateClock, 1000);

async function fetchData() {
  try {
    const r = await fetch('/data');
    const d = await r.json();
    
    const co2 = document.getElementById('dispCO2');
    const temp = document.getElementById('dispTemp');
    if(d.co2 !== null) {
      co2.innerText = d.co2.toFixed(0);
      co2.className = 'card-value ' + (d.co2 > 1000 ?
      'danger' : (d.co2 > 800 ? 'warn' : 'safe'));
    } else { co2.innerText = '--';
    }
    temp.innerText = (d.temp !== null) ? d.temp.toFixed(1) : '--';

    const tm = document.getElementById('nextSave');
    const btn = document.getElementById('btnSync');
    
    tm.innerText = "Next Auto-Save: " + secToHMS(d.next);
    if (d.synced == 1) {
      btn.innerText = "Time Synced ✅";
      btn.style.background = "#10b981";
      btn.disabled = true;
    } else {
      btn.innerText = "⚡ TAP TO SYNC TIME (Fix 1970 Dates)";
    }

    const rh = await fetch('/history');
    const hData = await rh.json();
    const body = document.getElementById('histBody');
    body.innerHTML = '';
    if(hData.length === 0) {
      body.innerHTML = '<tr><td colspan="3" style="padding:15px; color:var(--muted)">No records yet</td></tr>';
    } else {
      hData.reverse().forEach(row => {
        const tr = document.createElement('tr');
        const dt = new Date(row.epoch * 1000);
        const tStr = dt.toLocaleDateString() + ' ' + dt.toLocaleTimeString([], {hour:'2-digit', minute:'2-digit'});
        tr.innerHTML = `<td>${tStr}</td><td>${row.co2?row.co2.toFixed(0):'ERR'}</td><td>${row.temp?row.temp.toFixed(1):'ERR'}</td>`;
        body.appendChild(tr);
      });
    }
  } catch(e) {}
}

async function syncTime() {
  const t = Math.floor(Date.now() / 1000);
  try {
    await fetch('/set_time?t=' + t);
    alert("Time Synced Successfully!\nPrevious '1970' records have been corrected to real time.");
    fetchData();
  } catch(e) { alert("Sync Failed"); }
}

async function archiveNow() {
  if(confirm("Manually record current reading?")) {
    await fetch('/archiveNow', {method:'POST'});
    setTimeout(fetchData, 500); 
  }
}

setInterval(fetchData, 2000); 
fetchData();
updateClock();
</script>
</body>
</html>
)rawliteral";

// --- API Handlers ---

void handleRoot(){ server.send(200, "text/html", INDEX_HTML); }

void handleData() {
  unsigned long elapsed = millis() - lastLogMillis;
  long remaining = 0;
  if (elapsed < ARCHIVE_INTERVAL_MS) {
     remaining = (ARCHIVE_INTERVAL_MS - elapsed) / 1000;
  }

  String j = "{";
  j += "\"co2\":" + (isnan(curCO2) ? "null" : String(curCO2, 0)) + ",";
  j += "\"temp\":" + (isnan(curTemp) ? "null" : String(curTemp, 2)) + ",";
  j += "\"next\":" + String(remaining) + ",";
  j += "\"synced\":" + String(timeSynced ? 1 : 0);
  
  j += "}";
  server.send(200, "application/json", j);
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
      out += "\"co2\":" + (isnan(history[idx].co2) ? "null" : String(history[idx].co2, 0)) + ",";
      out += "\"temp\":" + (isnan(history[idx].temp) ? "null" : String(history[idx].temp, 2));
      out += "}";
    }
  }
  out += "]";
  server.send(200, "application/json", out);
}

void handleSetTime() {
  if (server.hasArg("t")) {
    long new_real_time = server.arg("t").toInt();
    // 1. Get current internal time (which is likely 1970-based)
    time_t current_internal;
    time(&current_internal);
    // 2. Calculate Offset (Difference between Real 2025 and Internal 1970)
    long offset = new_real_time - (long)current_internal;
    // 3. Retroactively Update OLD Records
    // We update any record that has a year < 2000 (meaning it was recorded before sync)
    if (histCount > 0) {
       for (int i = 0; i < HISTORY_LEN; i++) {
          // If record is older than year 2000, it needs updating.
          if (history[i].epoch > 0 && history[i].epoch < 946684800) {
             history[i].epoch += offset;
          }
       }
    }

    // 4. Apply New Time to System
    struct timeval tv = { .tv_sec = new_real_time, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    
    timeSynced = true;
    server.send(200, "text/plain", "OK");
  } else server.send(400, "text/plain", "Error");
}

void handleArchiveNow() {
  // Manual Save: false = do NOT reset auto-timer
  addHistory(curCO2, curTemp, false);
  server.send(200, "text/plain", "OK");
}

void handleDownloadCSV() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv", "");
  server.sendContent("Date_Time,CO2_ppm,Temp_C\r\n");

  int count = histCount;
  if (count > 0) {
    int start = (histWriteIndex - count + HISTORY_LEN) % HISTORY_LEN;
    for (int i = 0; i < count; ++i) {
      int idx = (start + i) % HISTORY_LEN;
      String line = formatDateTime(history[idx].epoch) + ",";
      line += (isnan(history[idx].co2) ? "" : String(history[idx].co2, 0)) + ",";
      line += (isnan(history[idx].temp) ? "" : String(history[idx].temp, 2));
      line += "\r\n";
      server.sendContent(line);
    }
  }
  server.sendContent("");
}

void setup() {
  Serial.begin(UART_BAUD);
  
  setenv("TZ", "IST-5:30", 1);
  tzset(); 

  histWriteIndex = 0; histCount = 0;
  for(int i=0; i<HISTORY_LEN; i++) history[i].epoch = 0;
  
  timeSynced = false;
  firstPointCaptured = false;
  lastLogMillis = millis(); 

  WiFi.softAP(apSSID, apPassword);
  
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/history", handleHistory);
  server.on("/set_time", handleSetTime);
  server.on("/archiveNow", HTTP_POST, handleArchiveNow);
  server.on("/download.csv", handleDownloadCSV);
  
  server.begin();
}

void loop() {
  server.handleClient();
  String s = readSerialLine();
  if (s.length() > 0) {
    s.trim();
    int comma = s.indexOf(',');
    if (comma > 0) {
      String a = s.substring(0, comma);
      String b = s.substring(comma + 1);
      a.trim(); b.trim();
      if(isNumeric(a) && isNumeric(b) && !a.equalsIgnoreCase("ERR")) {
        curCO2 = a.toFloat();
        curTemp = b.toFloat();
        
        // --- IMMEDIATE START LOGIC ---
        // As soon as valid data arrives (after Arduino warmup), record Point #1
        if (!firstPointCaptured) {
           addHistory(curCO2, curTemp, true);
           // true = starts the 2-hour timer
           firstPointCaptured = true;
        }
      } else { curCO2 = NAN; curTemp = NAN;
      }
    }
  }

  // Auto-Log Logic (Runs immediately after boot)
  if (millis() - lastLogMillis >= ARCHIVE_INTERVAL_MS) {
    if (!isnan(curCO2)) {
      addHistory(curCO2, curTemp, true);
      // true = reset timer
    } else {
      lastLogMillis = millis();
    }
  }
}