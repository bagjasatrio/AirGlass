#pragma once
#include <string>

namespace ap::drop {

inline std::string get_drop_page_html() {
    static const char* p1 = R"raw1(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="theme-color" content="#0b0f14">
<title>AirGlass Web Drop</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; -webkit-tap-highlight-color: transparent; }
:root {
  --bg: #090d12;
  --card-bg: rgba(255, 255, 255, 0.045);
  --card-border: rgba(255, 255, 255, 0.12);
  --accent: #38bdf8;
  --accent-glow: rgba(56, 189, 248, 0.25);
  --green: #34d399;
  --amber: #fbbf24;
  --red: #f87171;
  --text: #f1f5f9;
  --text-muted: #94a3b8;
  --font: -apple-system, BlinkMacSystemFont, "SF Pro Display", "SF Pro Text", "Segoe UI", Roboto, sans-serif;
}
body {
  font-family: var(--font);
  background-color: var(--bg);
  background-image: 
    radial-gradient(circle at 50% -20%, rgba(56, 189, 248, 0.15), transparent 60%),
    radial-gradient(circle at 80% 80%, rgba(52, 211, 153, 0.08), transparent 50%);
  color: var(--text);
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  padding: 18px;
  padding-bottom: calc(24px + env(safe-area-inset-bottom));
}
.header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 20px;
}
.brand {
  display: flex;
  align-items: center;
  gap: 10px;
}
.logo-icon {
  width: 32px;
  height: 32px;
  border-radius: 9px;
  background: linear-gradient(135deg, #38bdf8, #818cf8);
  display: flex;
  align-items: center;
  justify-content: center;
  font-weight: 800;
  font-size: 16px;
  color: #0b0f14;
  box-shadow: 0 4px 12px var(--accent-glow);
}
.brand-name {
  font-size: 19px;
  font-weight: 700;
  letter-spacing: -0.4px;
}
.badge {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  background: rgba(52, 211, 153, 0.12);
  border: 1px solid rgba(52, 211, 153, 0.3);
  color: #a7f3d0;
  font-size: 12px;
  font-weight: 500;
  padding: 5px 11px;
  border-radius: 20px;
}
.dot {
  width: 7px;
  height: 7px;
  background: var(--green);
  border-radius: 50%;
  box-shadow: 0 0 8px var(--green);
}
.tabs {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 6px;
  background: rgba(255, 255, 255, 0.04);
  border: 1px solid var(--card-border);
  padding: 4px;
  border-radius: 14px;
  margin-bottom: 18px;
}
.tab-btn {
  background: none;
  border: none;
  color: var(--text-muted);
  font-size: 14px;
  font-weight: 600;
  padding: 10px;
  border-radius: 10px;
  cursor: pointer;
  transition: all 0.2s ease;
}
.tab-btn.active {
  background: rgba(255, 255, 255, 0.1);
  color: #ffffff;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.3);
}
.view { display: none; }
.view.active { display: block; }
.card {
  background: var(--card-bg);
  border: 1px solid var(--card-border);
  border-radius: 18px;
  padding: 20px;
  backdrop-filter: blur(20px);
  -webkit-backdrop-filter: blur(20px);
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.25);
  margin-bottom: 16px;
}
.btn-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 10px;
  margin-bottom: 14px;
}
.picker-btn {
  border: 1px solid rgba(56, 189, 248, 0.35);
  background: rgba(56, 189, 248, 0.08);
  border-radius: 14px;
  padding: 16px 12px;
  text-align: center;
  cursor: pointer;
  transition: all 0.2s;
}
.picker-btn:active {
  background: rgba(56, 189, 248, 0.18);
  border-color: var(--accent);
}
.picker-icon {
  font-size: 26px;
  display: block;
  margin-bottom: 6px;
}
.picker-title {
  font-size: 13px;
  font-weight: 600;
  color: var(--text);
}
.file-list-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin: 14px 0 8px 0;
  font-size: 13px;
  color: var(--text-muted);
}
.file-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
  max-height: 260px;
  overflow-y: auto;
}
.file-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  background: rgba(255, 255, 255, 0.035);
  border: 1px solid rgba(255, 255, 255, 0.08);
  border-radius: 12px;
  padding: 10px 12px;
}
.file-info {
  display: flex;
  flex-direction: column;
  overflow: hidden;
  padding-right: 10px;
}
.file-name {
  font-size: 13px;
  font-weight: 500;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
.file-meta {
  font-size: 11px;
  color: var(--text-muted);
  display: flex;
  gap: 8px;
  margin-top: 2px;
}
.file-status-tag {
  font-weight: 600;
}
.file-status-tag.done { color: var(--green); }
.file-status-tag.uploading { color: var(--accent); }
.file-status-tag.retrying { color: var(--amber); }
.file-status-tag.error { color: var(--red); }
.btn-remove {
  background: none;
  border: none;
  color: #94a3b8;
  font-size: 16px;
  padding: 4px 8px;
  cursor: pointer;
}
.btn-action {
  background: linear-gradient(135deg, #38bdf8, #2563eb);
  color: white;
  border: none;
  font-weight: 600;
  font-size: 14px;
  padding: 13px 20px;
  border-radius: 12px;
  cursor: pointer;
  width: 100%;
  margin-top: 14px;
  box-shadow: 0 4px 14px var(--accent-glow);
}
.btn-action:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}
.btn-download {
  background: rgba(56, 189, 248, 0.15);
  border: 1px solid rgba(56, 189, 248, 0.3);
  color: var(--accent);
  padding: 6px 12px;
  border-radius: 8px;
  font-size: 12px;
  font-weight: 600;
  text-decoration: none;
}
.progress-wrap {
  display: none;
  margin-top: 14px;
}
.progress-bar-bg {
  width: 100%;
  height: 8px;
  background: rgba(255, 255, 255, 0.08);
  border-radius: 4px;
  overflow: hidden;
}
.progress-bar-fill {
  width: 0%;
  height: 100%;
  background: linear-gradient(90deg, #38bdf8, #34d399);
  transition: width 0.15s ease;
}
.progress-text {
  display: flex;
  justify-content: space-between;
  font-size: 12px;
  color: var(--text-muted);
  margin-top: 6px;
}
.wake-indicator {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  font-size: 11px;
  color: #a7f3d0;
  margin-top: 8px;
}
.notice {
  font-size: 12px;
  text-align: center;
  color: var(--text-muted);
  margin-top: auto;
  padding-top: 16px;
}
</style>
</head>)raw1";
    static const char* p2 = R"raw2(
<body>

<div class="header">
  <div class="brand">
    <div class="logo-icon">A</div>
    <div class="brand-name">AirGlass Drop</div>
  </div>
  <div class="badge">
    <div class="dot"></div>
    <span>Windows PC</span>
  </div>
</div>

<div class="tabs">
  <button class="tab-btn active" onclick="switchTab('upload')">Send to PC</button>
  <button class="tab-btn" onclick="switchTab('download')">Receive from PC</button>
</div>

<!-- Upload View -->
<div id="uploadView" class="view active">
  <div class="card">
    <div class="btn-grid">
      <div class="picker-btn" onclick="document.getElementById('photoInput').click()">
        <span class="picker-icon">&#128248;</span>
        <div class="picker-title">Add Photos / Videos</div>
      </div>
      <div class="picker-btn" onclick="document.getElementById('fileInput').click()">
        <span class="picker-icon">&#128194;</span>
        <div class="picker-title">Add Files / Documents</div>
      </div>
    </div>
    
    <input type="file" id="photoInput" accept="image/*,video/*" multiple style="display:none" onchange="appendFiles(this.files)">
    <input type="file" id="fileInput" multiple style="display:none" onchange="appendFiles(this.files)">

    <div id="fileListHeader" class="file-list-header" style="display:none">
      <span id="selectedCountText">0 files ready</span>
      <button onclick="clearFiles()" style="background:none; border:none; color:#f87171; font-size:12px; cursor:pointer;">Clear All</button>
    </div>

    <div id="selectedFiles" class="file-list"></div>

    <div id="progressWrap" class="progress-wrap">
      <div class="progress-bar-bg">
        <div id="progressFill" class="progress-bar-fill"></div>
      </div>
      <div class="progress-text">
        <span id="progressStatus">Uploading...</span>
        <span id="progressPct">0%</span>
      </div>
      <div id="wakeNotice" class="wake-indicator" style="display:none">
        &#9889; Screen sleep prevented during active transfer
      </div>
    </div>

    <button id="uploadBtn" class="btn-action" style="display:none" onclick="startSequentialUpload()">Send to Windows</button>
  </div>
</div>

<!-- Download View -->
<div id="downloadView" class="view">
  <div class="card">
    <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:12px;">
      <h3 style="font-size:15px; font-weight:600;">Files Shared from PC</h3>
      <button onclick="loadPcFiles()" style="background:none; border:none; color:var(--accent); font-size:13px; font-weight:600; cursor:pointer;">Refresh</button>
    </div>
    <div id="pcFileList" class="file-list">
      <div style="text-align:center; color:var(--text-muted); padding:20px; font-size:13px;">Checking folder...</div>
    </div>
  </div>
</div>

<div class="notice">
  Instant local transfer over Wi-Fi. Files stay 100% private on your own devices.
</div>

<script>
let filesToUpload = [];
let isUploading = false;
let currentUploadIndex = -1;
let wakeLock = null;
let keepAliveMedia = null;

// Prevent screen sleep during transfers
async function enableScreenKeepAlive() {
  document.getElementById('wakeNotice').style.display = 'inline-flex';
  try {
    if ('wakeLock' in navigator) {
      wakeLock = await navigator.wakeLock.request('screen');
      wakeLock.addEventListener('release', () => { wakeLock = null; });
    }
  } catch (e) {}

  // Invisible silent video fallback for iOS background throttling
  if (!keepAliveMedia) {
    keepAliveMedia = document.createElement('video');
    keepAliveMedia.setAttribute('playsinline', '');
    keepAliveMedia.setAttribute('loop', '');
    keepAliveMedia.muted = true;
    keepAliveMedia.src = 'data:video/mp4;base64,AAAAHGZ0eXBtcDQyAAAAAG1wNDJpc29tYXZjMQAAADpmcmVlAAAALW1kYXQAAAGzAAABt2GliQEAAAC8AAAAvQAAAAC2AEAAAIYAAAAAAAAAAAAAAAC/AAAAvw==';
    keepAliveMedia.play().catch(() => {});
  }
}

function disableScreenKeepAlive() {
  document.getElementById('wakeNotice').style.display = 'none';
  if (wakeLock) {
    wakeLock.release().catch(() => {});
    wakeLock = null;
  }
  if (keepAliveMedia) {
    keepAliveMedia.pause();
    keepAliveMedia = null;
  }
}

// Auto-recover if user manually turned off and turned back on screen
document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'visible' && isUploading) {
    enableScreenKeepAlive();
  }
});

function switchTab(t) {
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
  document.querySelectorAll('.view').forEach(v => v.classList.remove('active'));
  if (t === 'upload') {
    document.querySelectorAll('.tab-btn')[0].classList.add('active');
    document.getElementById('uploadView').classList.add('active');
  } else {
    document.querySelectorAll('.tab-btn')[1].classList.add('active');
    document.getElementById('downloadView').classList.add('active');
    loadPcFiles();
  }
}

function formatBytes(bytes) {
  if (bytes === 0) return '0 B';
  const k = 1024, dm = 1, sizes = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

function appendFiles(newFiles) {
  if (!newFiles || newFiles.length === 0) return;
  for (let i = 0; i < newFiles.length; i++) {
    const f = newFiles[i];
    if (!filesToUpload.some(x => x.name === f.name && x.size === f.size)) {
      filesToUpload.push(f);
    }
  }
  renderFileList();
}

function removeFile(index) {
  if (isUploading) return;
  filesToUpload.splice(index, 1);
  renderFileList();
}

function clearFiles() {
  if (isUploading) return;
  filesToUpload = [];
  renderFileList();
}

function renderFileList() {
  const list = document.getElementById('selectedFiles');
  const header = document.getElementById('fileListHeader');
  const btn = document.getElementById('uploadBtn');
  list.innerHTML = '';

  if (filesToUpload.length === 0) {
    header.style.display = 'none';
    btn.style.display = 'none';
    return;
  }

  let totalSize = 0;
  filesToUpload.forEach(f => totalSize += f.size);

  header.style.display = 'flex';
  document.getElementById('selectedCountText').innerText = `${filesToUpload.length} file(s) selected (${formatBytes(totalSize)})`;

  filesToUpload.forEach((f, idx) => {
    const item = document.createElement('div');
    item.className = 'file-item';
    item.id = `file-row-${idx}`;
    item.innerHTML = `
      <div class="file-info">
        <span class="file-name">${f.name}</span>
        <div class="file-meta">
          <span>${formatBytes(f.size)}</span>
          <span id="file-status-${idx}" class="file-status-tag">Ready</span>
        </div>
      </div>
      <button class="btn-remove" onclick="removeFile(${idx})">&#x2715;</button>
    `;
    list.appendChild(item);
  });

  btn.style.display = 'block';
  btn.innerText = `Send ${filesToUpload.length} File(s) to Windows`;
}

async function startSequentialUpload() {
  if (filesToUpload.length === 0 || isUploading) return;
  isUploading = true;
  await enableScreenKeepAlive();

  const btn = document.getElementById('uploadBtn');
  btn.disabled = true;
  const wrap = document.getElementById('progressWrap');
  const fill = document.getElementById('progressFill');
  const pct = document.getElementById('progressPct');
  const status = document.getElementById('progressStatus');
  wrap.style.display = 'block';

  let successCount = 0;
  let failedCount = 0;
  const totalFiles = filesToUpload.length;

  for (let i = 0; i < totalFiles; i++) {
    currentUploadIndex = i;
    const f = filesToUpload[i];
    const statusTag = document.getElementById(`file-status-${i}`);
    if (statusTag) {
      statusTag.innerText = 'Uploading...';
      statusTag.className = 'file-status-tag uploading';
    }

    status.innerText = `Uploading ${i + 1} of ${totalFiles}: ${f.name}`;

    // Auto-retry up to 3 times in case screen dimmed or Wi-Fi had jitter
    let fileSuccess = false;
    for (let attempt = 1; attempt <= 3; attempt++) {
      try {
        if (attempt > 1 && statusTag) {
          statusTag.innerText = `Retrying (${attempt}/3)...`;
          statusTag.className = 'file-status-tag retrying';
          status.innerText = `Retrying ${f.name} (attempt ${attempt}/3)...`;
          await new Promise(r => setTimeout(r, 1200));
        }

        await uploadSingleFile(f, (loaded, total) => {
          const filePct = total > 0 ? (loaded / total) : 0;
          const totalPct = Math.round(((i + filePct) / totalFiles) * 100);
          fill.style.width = totalPct + '%';
          pct.innerText = totalPct + '%';
        });

        fileSuccess = true;
        break;
      } catch (err) {
        console.warn(`Attempt ${attempt} failed for ${f.name}:`, err);
      }
    }

    if (fileSuccess) {
      if (statusTag) {
        statusTag.innerText = 'Done ✓';
        statusTag.className = 'file-status-tag done';
      }
      successCount++;
    } else {
      if (statusTag) {
        statusTag.innerText = 'Failed ✗';
        statusTag.className = 'file-status-tag error';
      }
      failedCount++;
      // Report failure directly to AirGlass server
      try {
        const reportUrl = `/api/report_failure?name=${encodeURIComponent(f.name)}&error=${encodeURIComponent('Transfer interrupted or timed out')}`;
        fetch(reportUrl, { method: 'POST' }).catch(() => {});
      } catch (e) {}
    }
  }

  fill.style.width = '100%';
  pct.innerText = '100%';
  if (failedCount === 0) {
    status.innerText = `Completed! All ${successCount} file(s) saved to PC.`;
  } else {
    status.innerText = `Finished: ${successCount} saved, ${failedCount} failed.`;
  }

  btn.disabled = false;
  isUploading = false;
  currentUploadIndex = -1;
  disableScreenKeepAlive();

  if (failedCount === 0) {
    setTimeout(() => {
      clearFiles();
      wrap.style.display = 'none';
    }, 2500);
  }
}

function uploadSingleFile(file, onProgress) {
  return new Promise((resolve, reject) => {
    const fd = new FormData();
    fd.append('file', file, file.name);

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/upload', true);
    xhr.timeout = 180000; // 3 minutes timeout per file

    xhr.upload.onprogress = (e) => {
      if (e.lengthComputable && onProgress) {
        onProgress(e.loaded, e.total);
      }
    };

    xhr.onload = () => {
      if (xhr.status === 200) resolve();
      else reject(new Error('Status ' + xhr.status));
    };

    xhr.onerror = () => reject(new Error('Network error'));
    xhr.ontimeout = () => reject(new Error('Timeout'));
    xhr.onabort = () => reject(new Error('Aborted'));
    xhr.send(fd);
  });
}

async function deleteSharedItem(name) {
  try {
    await fetch('/api/delete?name=' + encodeURIComponent(name), { method: 'POST' });
    loadPcFiles();
  } catch(e) {}
}

function loadPcFiles() {
  const container = document.getElementById('pcFileList');
  container.innerHTML = '<div style="text-align:center; color:var(--text-muted); padding:20px; font-size:13px;">Checking folder...</div>';
  fetch('/api/files')
    .then(r => r.json())
    .then(data => {
      container.innerHTML = '';
      if (!data || data.length === 0) {
        container.innerHTML = '<div style="text-align:center; color:var(--text-muted); padding:20px; font-size:13px;">No files currently shared from PC.<br><small style="opacity:0.7">Click "Send Files to iPhone" on your PC to share files.</small></div>';
        return;
      }
      data.forEach(f => {
        const item = document.createElement('div');
        item.className = 'file-item';
        item.innerHTML = `
          <div class="file-info">
            <span class="file-name">${f.name}</span>
            <span class="file-meta">${formatBytes(f.size)}</span>
          </div>
          <div style="display:flex; align-items:center; gap:8px;">
            <a class="btn-download" href="/api/download?name=${encodeURIComponent(f.name)}" download>Download</a>
            <button class="btn-remove" onclick="deleteSharedItem('${f.name.replace(/'/g, "\\'")}')">&#x2715;</button>
          </div>
        `;
        container.appendChild(item);
      });
    })
    .catch(() => {
      container.innerHTML = '<div style="text-align:center; color:#f87171; padding:20px; font-size:13px;">Failed to load files from PC</div>';
    });
}
</script>
</body>
</html>
)raw2";
    return std::string(p1) + p2;
}

} // namespace ap::drop
