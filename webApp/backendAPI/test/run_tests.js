const { spawn } = require('child_process');
const fs = require('fs').promises;
const path = require('path');
const os = require('os');
const assert = require('assert');

function findFreePort() {
  return new Promise((resolve, reject) => {
    const srv = require('net').createServer();
    srv.listen(0, () => {
      const port = srv.address().port;
      srv.close(() => resolve(port));
    });
    srv.on('error', reject);
  });
}

async function spawnServer(port, envExtra = {}) {
  return new Promise((resolve, reject) => {
    const env = Object.assign({}, process.env, envExtra, { PORT: String(port) });
    const child = spawn(process.execPath, [path.join(__dirname, '..', 'index.js')], { env, stdio: ['ignore', 'pipe', 'pipe'] });

    const timeout = setTimeout(() => {
      child.kill();
      reject(new Error('Server did not start in time'));
    }, 10000);

    child.stdout.on('data', (d) => {
      const s = d.toString();
      process.stdout.write(`[server stdout] ${s}`);
      if (s.includes(`Listening on ${port}`)) {
        clearTimeout(timeout);
        resolve(child);
      }
    });

    child.stderr.on('data', (d) => {
      const s = d.toString();
      process.stderr.write(`[server stderr] ${s}`);
    });

    child.on('exit', (code, signal) => {
      clearTimeout(timeout);
      reject(new Error(`Server exited early with code=${code} signal=${signal}`));
    });

    child.on('error', (e) => reject(e));
  });
}

async function httpGetJson(port, p) {
  const url = `http://127.0.0.1:${port}${p}`;
  const res = await fetch(url);
  const json = await res.json();
  return { status: res.status, json };
}

async function httpGetBuffer(port, p) {
  const url = `http://127.0.0.1:${port}${p}`;
  const res = await fetch(url);
  const buffer = await res.arrayBuffer();
  return { status: res.status, buffer: Buffer.from(buffer), headers: res.headers };
}

async function httpPostText(port, p, text) {
  const url = `http://127.0.0.1:${port}${p}`;
  const res = await fetch(url, { method: 'POST', headers: { 'Content-Type': 'text/plain' }, body: text });
  const body = await res.text();
  try { return { status: res.status, json: JSON.parse(body) }; } catch { return { status: res.status, text: body }; }
}

async function run() {
  console.log('Running extended API tests...');

  const port = await findFreePort();
  const tmpdir = await fs.mkdtemp(path.join(os.tmpdir(), 'ledbox-test-'));

  // Prepare environment and files
  const statusFile = path.join(tmpdir, 'ledSyncVideo_status.json');
  const configFile = path.join(tmpdir, 'config.txt');
  const img1 = path.join(tmpdir, 'img1.png');
  const img2 = path.join(tmpdir, 'img2.jpg');

  const sampleStatus = { pid: 4321, running: true, last_start: new Date().toISOString(), message: 'ok' };
  await fs.writeFile(statusFile, JSON.stringify(sampleStatus, null, 2), 'utf8');
  await fs.writeFile(configFile, 'initial-config', 'utf8');
  await fs.writeFile(img1, 'PNGDATA1', 'utf8');
  // make img2 newer
  await new Promise(r => setTimeout(r, 10));
  await fs.writeFile(img2, 'JPGDATA2', 'utf8');

  const env = {
    LEDSYNCVIDEO_STATUS_FILE: statusFile,
    BASE_DIR: tmpdir,
    CONFIG_FILENAME: 'config.txt',
    SYNC_CMD: "echo sync-done",
    CALIBRATE_CMD: "echo calibrate-done",
    LATEST_SCREENSHOT_FILE: img2
  };

  const server = await spawnServer(port, env);
  try {
    // STATUS
    const { status: s1, json: st } = await httpGetJson(port, '/api/status');
    assert.strictEqual(s1, 200);
    assert.strictEqual(st.pid, sampleStatus.pid);
    assert.strictEqual(st.running, sampleStatus.running);
    console.log('status endpoint OK');

    // CONFIG GET (returned as plain text)
    const cfgRes = await fetch(`http://127.0.0.1:${port}/api/config`);
    const cfgText = await cfgRes.text();
    assert.strictEqual(cfgRes.status, 200);
    assert.strictEqual(cfgText, 'initial-config');
    console.log('config GET OK');

    // CONFIG POST
    const postResp = await httpPostText(port, '/api/config', 'new-config-value');
    assert.strictEqual(postResp.status, 201);
    const saved = await fs.readFile(configFile, 'utf8');
    assert.strictEqual(saved, 'new-config-value');
    console.log('config POST OK');

    // CONTROL calibrate
    const calResp = await fetch(`http://127.0.0.1:${port}/api/control/calibrate`, { method: 'POST' });
    assert.strictEqual(calResp.status, 200);
    const calJson = await calResp.json();
    assert.strictEqual(calJson.ok, true);
    assert.ok(calJson.output.includes('calibrate-done'));
    console.log('control calibrate OK');

    // CONTROL sync
    const syncResp = await fetch(`http://127.0.0.1:${port}/api/control/sync`, { method: 'POST' });
    assert.strictEqual(syncResp.status, 200);
    const syncJson = await syncResp.json();
    assert.strictEqual(syncJson.ok, true);
    assert.ok(syncJson.output.includes('sync-done'));
    console.log('control sync OK');

    // SCREENSHOT latest
    const latest = await httpGetBuffer(port, '/api/screenshot');
    assert.strictEqual(latest.status, 200);
    assert.ok(latest.buffer.length > 0);
    console.log('screenshot latest OK');

    // SCREENSHOT by filename
    const byFile = await httpGetBuffer(port, `/api/screenshot?filename=${encodeURIComponent(path.basename(img1))}`);
    assert.strictEqual(byFile.status, 200);
    assert.ok(byFile.buffer.length > 0);
    console.log('screenshot by filename OK');

    // SCREENSHOT path traversal should fail
    const trav = await fetch(`http://127.0.0.1:${port}/api/screenshot?filename=${encodeURIComponent('../etc/passwd')}`);
    assert.ok(trav.status === 400 || trav.status === 500);
    console.log('screenshot path traversal check OK');

    console.log('All extended API tests passed');
  } finally {
    server.kill();
  }
}

run().catch((err) => {
  console.error('Tests failed:', err);
  process.exit(1);
});
