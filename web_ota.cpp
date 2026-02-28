/**
 * web_ota.cpp — Forno Pizza v19
 * ================================================================
 * Web server OTA con pagina HTML drag&drop
 *
 * - Porta 80, endpoint POST /update per upload .bin
 * - Pagina HTML inline (nessun file system richiesto)
 * - Barra di progresso live via fetch + polling /progress
 * - Usa le stesse variabili condivise di ota_manager:
 *     g_ota_progress, g_ota_running, g_ota_status_msg
 * ================================================================
 */

#include "web_ota.h"
#include "ui_wifi.h"   // g_ota_progress, g_ota_running, g_ota_status_msg
#include <Arduino.h>
#include <WebServer.h>
#include <Update.h>
#include <WiFi.h>

// ================================================================
//  Istanza server
// ================================================================
static WebServer _server(WEB_OTA_PORT);

// ================================================================
//  Pagina HTML — tema scuro coerente col progetto
// ================================================================
static const char OTA_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Forno Pizza — OTA Update</title>
<style>
  :root {
    --bg:     #06060e;
    --panel:  #0e0e1e;
    --border: #2a2a45;
    --orange: #ff6b00;
    --green:  #00ffcc;
    --blue:   #0080ff;
    --gray:   #606080;
    --text:   #c0c0d8;
    --error:  #ff4444;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Segoe UI', Arial, sans-serif;
    display: flex;
    flex-direction: column;
    align-items: center;
    min-height: 100vh;
    padding: 20px;
  }
  header {
    background: var(--orange);
    color: #000;
    width: 100%;
    max-width: 480px;
    padding: 10px 16px;
    border-radius: 8px 8px 0 0;
    font-weight: bold;
    font-size: 1.1rem;
    text-align: center;
    letter-spacing: 2px;
  }
  .card {
    background: var(--panel);
    border: 1px solid var(--border);
    border-top: none;
    border-radius: 0 0 8px 8px;
    width: 100%;
    max-width: 480px;
    padding: 24px;
  }
  .drop-zone {
    border: 2px dashed var(--border);
    border-radius: 8px;
    padding: 40px 20px;
    text-align: center;
    cursor: pointer;
    transition: border-color .2s, background .2s;
    margin-bottom: 16px;
  }
  .drop-zone.hover, .drop-zone:hover {
    border-color: var(--orange);
    background: rgba(255,107,0,.05);
  }
  .drop-zone.has-file {
    border-color: var(--green);
    background: rgba(0,255,204,.04);
  }
  .drop-zone svg {
    width: 48px; height: 48px;
    fill: var(--gray);
    margin-bottom: 10px;
  }
  .drop-zone p { color: var(--gray); font-size: .9rem; margin-top: 6px; }
  .drop-zone .filename {
    color: var(--green);
    font-size: 1rem;
    font-weight: bold;
    margin-top: 8px;
    word-break: break-all;
  }
  input[type=file] { display: none; }
  .btn {
    display: block;
    width: 100%;
    padding: 12px;
    background: var(--orange);
    color: #000;
    font-size: 1rem;
    font-weight: bold;
    border: none;
    border-radius: 6px;
    cursor: pointer;
    letter-spacing: 1px;
    transition: opacity .2s;
  }
  .btn:disabled { opacity: .35; cursor: not-allowed; }
  .btn:not(:disabled):hover { opacity: .85; }
  .progress-wrap {
    margin-top: 20px;
    display: none;
  }
  .progress-bar-bg {
    background: var(--border);
    border-radius: 99px;
    height: 14px;
    overflow: hidden;
  }
  .progress-bar {
    height: 100%;
    width: 0%;
    background: linear-gradient(90deg, var(--orange), var(--green));
    border-radius: 99px;
    transition: width .3s;
  }
  .progress-pct {
    text-align: right;
    font-size: .8rem;
    color: var(--gray);
    margin-top: 4px;
  }
  .status-msg {
    margin-top: 12px;
    font-size: .85rem;
    color: var(--gray);
    min-height: 1.2em;
    text-align: center;
  }
  .status-msg.ok  { color: var(--green); }
  .status-msg.err { color: var(--error); }
  .ip-info {
    margin-top: 20px;
    font-size: .78rem;
    color: var(--gray);
    text-align: center;
  }
  .ip-info span { color: var(--blue); }
</style>
</head>
<body>
<header>&#9889; FORNO PIZZA &mdash; OTA FIRMWARE UPDATE</header>
<div class="card">

  <div class="drop-zone" id="dropZone">
    <svg viewBox="0 0 24 24"><path d="M19.35 10.04A7.49 7.49 0 0012 4C9.11 4 6.6 5.64 5.35 8.04A5.994 5.994 0 000 14c0 3.31 2.69 6 6 6h13c2.76 0 5-2.24 5-5 0-2.64-2.05-4.78-4.65-4.96zM14 13v4h-4v-4H7l5-5 5 5h-3z"/></svg>
    <p>Trascina qui il file <strong>.bin</strong><br>oppure clicca per selezionarlo</p>
    <div class="filename" id="fileName"></div>
  </div>
  <input type="file" id="fileInput" accept=".bin">

  <button class="btn" id="uploadBtn" disabled>FLASH FIRMWARE</button>

  <div class="progress-wrap" id="progressWrap">
    <div class="progress-bar-bg">
      <div class="progress-bar" id="progressBar"></div>
    </div>
    <div class="progress-pct" id="progressPct">0%</div>
  </div>

  <div class="status-msg" id="statusMsg"></div>

  <div class="ip-info">
    IP dispositivo: <span id="deviceIp">caricamento...</span>
  </div>

</div>

<script>
  const dropZone  = document.getElementById('dropZone');
  const fileInput = document.getElementById('fileInput');
  const fileName  = document.getElementById('fileName');
  const uploadBtn = document.getElementById('uploadBtn');
  const progWrap  = document.getElementById('progressWrap');
  const progBar   = document.getElementById('progressBar');
  const progPct   = document.getElementById('progressPct');
  const statusMsg = document.getElementById('statusMsg');
  const deviceIp  = document.getElementById('deviceIp');

  let selectedFile = null;
  let polling = null;

  // Mostra IP
  fetch('/info').then(r=>r.json()).then(d=>{
    deviceIp.textContent = d.ip || location.hostname;
  }).catch(()=>{ deviceIp.textContent = location.hostname; });

  // Drag & drop
  dropZone.addEventListener('click', () => fileInput.click());
  dropZone.addEventListener('dragover', e => { e.preventDefault(); dropZone.classList.add('hover'); });
  dropZone.addEventListener('dragleave', () => dropZone.classList.remove('hover'));
  dropZone.addEventListener('drop', e => {
    e.preventDefault();
    dropZone.classList.remove('hover');
    const f = e.dataTransfer.files[0];
    if (f) setFile(f);
  });
  fileInput.addEventListener('change', () => {
    if (fileInput.files[0]) setFile(fileInput.files[0]);
  });

  function setFile(f) {
    if (!f.name.endsWith('.bin')) {
      setStatus('Seleziona un file .bin valido', 'err');
      return;
    }
    selectedFile = f;
    fileName.textContent = f.name + ' (' + (f.size/1024).toFixed(1) + ' KB)';
    dropZone.classList.add('has-file');
    uploadBtn.disabled = false;
    setStatus('');
  }

  function setStatus(msg, cls) {
    statusMsg.textContent = msg;
    statusMsg.className = 'status-msg' + (cls ? ' ' + cls : '');
  }

  function setProgress(pct) {
    progBar.style.width = pct + '%';
    progPct.textContent = pct + '%';
  }

  // Upload via XHR per avere il progresso reale
  uploadBtn.addEventListener('click', () => {
    if (!selectedFile) return;

    uploadBtn.disabled = true;
    progWrap.style.display = 'block';
    setProgress(0);
    setStatus('Invio firmware in corso...');

    const fd = new FormData();
    fd.append('firmware', selectedFile, selectedFile.name);

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/update');

    xhr.upload.onprogress = e => {
      if (e.lengthComputable) {
        // Upload al server ESP32: 0→50%
        const pct = Math.round((e.loaded / e.total) * 50);
        setProgress(pct);
        setStatus('Upload: ' + (e.loaded/1024).toFixed(0) + ' / ' + (e.total/1024).toFixed(0) + ' KB');
      }
    };

    xhr.onload = () => {
      if (xhr.status === 200) {
        // Upload completato, ora polling flash progress 50→100%
        startPolling();
      } else {
        setStatus('Errore upload: HTTP ' + xhr.status, 'err');
        uploadBtn.disabled = false;
      }
    };

    xhr.onerror = () => {
      // L'ESP potrebbe aver riavviato prima di rispondere — fai polling
      startPolling();
    };

    xhr.send(fd);
  });

  function startPolling() {
    if (polling) clearInterval(polling);
    polling = setInterval(() => {
      fetch('/progress')
        .then(r => r.json())
        .then(d => {
          const pct = d.progress;
          const msg = d.message || '';

          if (pct === 100) {
            clearInterval(polling);
            setProgress(100);
            setStatus('Aggiornamento completato! Riavvio in corso...', 'ok');
          } else if (pct < 0) {
            clearInterval(polling);
            setStatus('Errore: ' + msg, 'err');
            uploadBtn.disabled = false;
          } else {
            // Flash in corso: mappa 0..99 → 50..99
            const display = 50 + Math.round(pct * 0.49);
            setProgress(display);
            setStatus(msg || 'Flash in corso...');
          }
        })
        .catch(() => {
          // ESP riavviato = update completato
          clearInterval(polling);
          setProgress(100);
          setStatus('Riavvio completato! Pagina si aggiornerà tra 5s...', 'ok');
          setTimeout(() => location.reload(), 5000);
        });
    }, 500);
  }
</script>
</body>
</html>
)rawhtml";

// ================================================================
//  Handler: GET / e GET /update  →  serve la pagina HTML
// ================================================================
static void handle_root() {
    _server.send_P(200, "text/html", OTA_HTML);
}

// ================================================================
//  Handler: GET /info  →  JSON con IP
// ================================================================
static void handle_info() {
    String ip = WiFi.localIP().toString();
    String json = "{\"ip\":\"" + ip + "\"}";
    _server.send(200, "application/json", json);
}

// ================================================================
//  Handler: GET /progress  →  JSON con stato OTA
// ================================================================
static void handle_progress() {
    // Legge le variabili condivise con ota_manager / LVGL
    int   pct = (int)g_ota_progress;
    char  msg[64];
    strncpy(msg, g_ota_status_msg, sizeof(msg));
    msg[sizeof(msg)-1] = '\0';

    // Escape minimale per JSON (sostituisce " con ')
    for (int i = 0; msg[i]; i++) if (msg[i] == '"') msg[i] = '\'';

    char buf[128];
    snprintf(buf, sizeof(buf), "{\"progress\":%d,\"message\":\"%s\"}", pct, msg);
    _server.send(200, "application/json", buf);
}

// ================================================================
//  Handler: POST /update  →  riceve il .bin e lo flasha
// ================================================================
static void handle_update_upload() {
    HTTPUpload& upload = _server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        // ── Inizio upload ──
        g_ota_running  = true;
        g_ota_progress = 0;
        strncpy(g_ota_status_msg, "Ricezione firmware...", sizeof(g_ota_status_msg));

        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            snprintf(g_ota_status_msg, sizeof(g_ota_status_msg),
                     "Errore begin: %s", Update.errorString());
            g_ota_progress = -1;
        }

    } else if (upload.status == UPLOAD_FILE_WRITE) {
        // ── Chunk ricevuto ──
        if (g_ota_progress >= 0) {
            size_t wr = Update.write(upload.buf, upload.currentSize);
            if (wr != upload.currentSize) {
                snprintf(g_ota_status_msg, sizeof(g_ota_status_msg),
                         "Errore scrittura: %s", Update.errorString());
                g_ota_progress = -1;
            } else {
                // Progresso 0..99 (100 = fine confermata)
                if (upload.totalSize > 0) {
                    g_ota_progress = (int)((upload.currentSize * 99UL) / upload.totalSize);
                }
                snprintf(g_ota_status_msg, sizeof(g_ota_status_msg),
                         "Flash: %lu KB ricevuti", upload.totalSize / 1024);
            }
        }

    } else if (upload.status == UPLOAD_FILE_END) {
        // ── Fine upload ──
        if (g_ota_progress >= 0) {
            if (Update.end(true)) {
                g_ota_progress = 100;
                strncpy(g_ota_status_msg, "Aggiornamento completato! Riavvio...",
                        sizeof(g_ota_status_msg));
            } else {
                snprintf(g_ota_status_msg, sizeof(g_ota_status_msg),
                         "Errore end: %s", Update.errorString());
                g_ota_progress = -1;
            }
        }
        g_ota_running = false;

    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        g_ota_progress = -1;
        strncpy(g_ota_status_msg, "Upload annullato", sizeof(g_ota_status_msg));
        g_ota_running  = false;
    }
}

static void handle_update_done() {
    bool ok = (g_ota_progress == 100);
    if (ok) {
        _server.send(200, "text/plain", "OK");
        delay(500);
        ESP.restart();
    } else {
        _server.send(500, "text/plain", g_ota_status_msg);
    }
}

// ================================================================
//  Task FreeRTOS — gira su core 0
// ================================================================
void Task_WebOTA(void* param) {
    for (;;) {
        if (WiFi.status() == WL_CONNECTED) {
            _server.handleClient();
        }
        vTaskDelay(pdMS_TO_TICKS(5));   // yield 5ms
    }
}

// ================================================================
//  web_ota_init  — chiama da setup() DOPO wifi connect
// ================================================================
void web_ota_init(void) {
    // Registra i route
    _server.on("/",        HTTP_GET,  handle_root);
    _server.on("/update",  HTTP_GET,  handle_root);
    _server.on("/info",    HTTP_GET,  handle_info);
    _server.on("/progress",HTTP_GET,  handle_progress);

    // Upload .bin: onFileUpload riceve i chunk, on("/update") invia la risposta finale
    _server.on("/update", HTTP_POST,
        handle_update_done,
        handle_update_upload
    );

    _server.onNotFound([]() {
        _server.send(404, "text/plain", "Not found");
    });

    _server.begin();

    // Avvia task dedicato su core 0
    xTaskCreatePinnedToCore(
        Task_WebOTA,
        "Task_WebOTA",
        TASK_WEBOTA_STACK,
        NULL,
        TASK_WEBOTA_PRIO,
        NULL,
        TASK_WEBOTA_CORE
    );
}

// ================================================================
//  web_ota_handle  — alternativa senza task (chiama nel loop)
// ================================================================
void web_ota_handle(void) {
    _server.handleClient();
}
