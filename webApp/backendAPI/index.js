const express = require('express');
const fs = require('fs').promises;
const fsSync = require('fs');
const path = require('path');
const os = require('os');
const { exec } = require('child_process');

const app = express();

// Accept plain text bodies for config upload (adjust limits as needed)
app.use(express.text({ type: 'text/*', limit: '200kb' }));

// Base directory for files (config, screenshots). Configure via env var.
const BASE_DIR = path.resolve(process.env.BASE_DIR || path.join(os.homedir(), 'ledBox')) + path.sep;
const CONFIG_FILENAME = process.env.CONFIG_FILENAME || 'config.txt';

// Path to the runtime metadata produced by the ledbox service
const LEDSYNCVIDEO_STATUS_FILE = process.env.LEDSYNCVIDEO_STATUS_FILE || '/run/ledbox/ledSyncVideo_status.json';
// const CALIBRATE_STATUS_FILE = process.env.CALIBRATE_STATUS_FILE || '/run/ledbox/calibrate_status.json';
const CALIBRATE_STATUS_FILE = process.env.CALIBRATE_STATUS_FILE || '/run/ledbox/calibrate_status.json';
const LATEST_SCREENSHOT_FILE = process.env.LATEST_SCREENSHOT_FILE || path.join(BASE_DIR, 'latestImg.jpg');

// Server start timestamp for status endpoint
const SERVER_STARTED_AT = new Date().toISOString();

// Helper to prevent path traversal: resolves and ensures it stays inside BASE_DIR
function safeResolve(relPath) {
  const resolved = path.resolve(BASE_DIR, relPath);
  if (!resolved.startsWith(BASE_DIR)) {
    const err = new Error('Invalid path');
    err.status = 400;
    throw err;
  }
  return resolved;
}

// Resolve a candidate path that may be absolute or relative, but ensure
// the final resolved path is inside `BASE_DIR` to prevent traversal.
function resolveWithinBase(candidate) {
  const resolved = path.isAbsolute(candidate) ? path.resolve(candidate) : path.resolve(BASE_DIR, candidate);
  if (!resolved.startsWith(BASE_DIR)) {
    const err = new Error('Invalid path');
    err.status = 400;
    throw err;
  }
  return resolved;
}

async function readMetadata(filePath) {
  try {
    const raw = await fs.readFile(filePath, 'utf8'); // path is a string
    return JSON.parse(raw);
  } catch (err) {
    if (err.code === 'ENOENT') {
      // file does not exist yet
      return null;
    }
    // rethrow other errors (permission issues, invalid JSON, etc.)
    throw err;
  }
}

// GET /api/status -> basic process/service status
app.get('/api/status', async (req, res, next) => {
  try {
    const ledSync_metadata = await readMetadata(LEDSYNCVIDEO_STATUS_FILE);

    if (!ledSync_metadata) {
      return res.json({
        running: false,
        pid: null,
        lastStart: null,
        message: 'no metadata',
        metadata: null,
      });
    }

    // Map possible snake_case or camelCase fields from the status file
    const running = typeof ledSync_metadata.running === 'boolean' ? ledSync_metadata.running : (typeof ledSync_metadata.running === 'string' ? ledSync_metadata.running === 'true' : true);
    const pid = ledSync_metadata.pid ?? null;
    const lastStart = ledSync_metadata.last_start ?? ledSync_metadata.lastStart ?? ledSync_metadata.startTime ?? SERVER_STARTED_AT;
    const message = ledSync_metadata.message ?? ledSync_metadata.msg ?? 'ok';

    res.json({ running, pid, lastStart, message, metadata: ledSync_metadata });
  } catch (err) {
    next(err);
  }
});

// GET /api/config -> returns config.txt (text/plain)
app.get('/api/config', async (req, res, next) => {
  try {
    const p = safeResolve(CONFIG_FILENAME);
    const data = await fs.readFile(p, 'utf8');
    res.type('text/plain').send(data);
  } catch (err) {
    if (err.code === 'ENOENT') return res.status(404).send('config.txt not found');
    next(err);
  }
});

// POST /api/config -> replace config.txt with posted text/plain body
app.post('/api/config', async (req, res, next) => {
    if (typeof req.body !== 'string') return res.status(400).send('Expected text/plain body');
  try {
    const p = safeResolve(CONFIG_FILENAME);
    await fs.writeFile(p, req.body, 'utf8');
    res.status(201).json({ ok: true });
  } catch (err) {
    next(err);
  }
});

// Helper to run optional shell commands for control endpoints
function runCommandIfConfigured(envName) {
  return new Promise((resolve, reject) => {
    const cmd = process.env[envName];
    if (!cmd) return resolve({ skipped: true, message: `${envName} not configured` });
    exec(cmd, { cwd: BASE_DIR, timeout: 60_000 }, (err, stdout, stderr) => {
      if (err) return reject({ err, stdout, stderr });
      resolve({ stdout: stdout.trim(), stderr: stderr.trim() });
    });
  });
}

// POST /api/control/calibrate
app.post('/api/control/calibrate', async (req, res, next) => {
  try {
    const result = await runCommandIfConfigured('CALIBRATE_CMD');
    if (result.skipped) return res.status(204).send();
    res.json({ ok: true, output: result.stdout, errorOutput: result.stderr });
  } catch (err) {
    next(err);
  }
});

// POST /api/control/sync
app.post('/api/control/sync', async (req, res, next) => {
  try {
    const result = await runCommandIfConfigured('SYNC_CMD');
    if (result.skipped) return res.status(204).send();
    res.json({ ok: true, output: result.stdout, errorOutput: result.stderr });
  } catch (err) {
    next(err);
  }
});

// GET /api/screenshot?filename=... -> serve screenshot file or latest image
app.get('/api/screenshot', async (req, res, next) => {
  try {
    // Serve only the configured latest screenshot file. Do not accept arbitrary filenames.
    const filePath = resolveWithinBase(LATEST_SCREENSHOT_FILE);
    if (!fsSync.existsSync(filePath)) return res.status(404).send('no screenshots found');
    res.sendFile(filePath);
  } catch (err) {
    next(err);
  }
});

// Basic error handler
app.use((err, req, res, next) => {
  console.error(err);
  res.status(err.status || 500).json({ error: err.message || 'internal error' });
});

const port = process.env.PORT || 3000;
app.listen(port, "0.0.0.0", () => console.log(`Listening on ${port}`));