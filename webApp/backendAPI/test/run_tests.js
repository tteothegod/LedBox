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

async function spawnServer(port, statusFile) {
  return new Promise((resolve, reject) => {
    const env = Object.assign({}, process.env, { PORT: String(port), LEDSYNCVIDEO_STATUS_FILE: statusFile });
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

async function httpGetJson(port, path) {
  const url = `http://127.0.0.1:${port}${path}`;
  const res = await fetch(url);
  const json = await res.json();
  return { status: res.status, json };
}

async function run() {
  console.log('Running tests...');

  // Test 1: when metadata file exists
  const port1 = await findFreePort();
  const tmpdir = await fs.mkdtemp(path.join(os.tmpdir(), 'ledbox-test-'));
  const statusFile = path.join(tmpdir, 'ledSyncVideo_status.json');
  const sample = { pid: 4321, running: true, last_start: new Date().toISOString(), message: 'ok' };
  await fs.writeFile(statusFile, JSON.stringify(sample, null, 2), 'utf8');

  const server = await spawnServer(port1, statusFile);
  try {
    const { status, json } = await httpGetJson(port1, '/api/status');
    assert.strictEqual(status, 200);
    assert.strictEqual(json.pid, sample.pid);
    assert.strictEqual(json.running, sample.running);
    assert.strictEqual(json.message, sample.message);
    console.log('Test 1 passed');
  } finally {
    server.kill();
  }

  // Test 2: when metadata file missing
  const port2 = await findFreePort();
  const missingFile = path.join(tmpdir, 'missing.json');
  const server2 = await spawnServer(port2, missingFile);
  try {
    const { status, json } = await httpGetJson(port2, '/api/status');
    assert.strictEqual(status, 200);
    assert.strictEqual(json.running, false);
    assert.strictEqual(json.pid, null);
    console.log('Test 2 passed');
  } finally {
    server2.kill();
  }

  console.log('All tests passed');
}

run().catch((err) => {
  console.error('Tests failed:', err);
  process.exit(1);
});
