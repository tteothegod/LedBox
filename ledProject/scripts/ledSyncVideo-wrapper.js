const { execFile } = require('child_process');
const path = require('path');
const fs = require('fs').promises;

const SYNC_CMD = process.env.SYNC_CMD || '/home/magodi12/ledProject/sync_cmd.sh';
const STATUS_FILE = process.env.LED_SYNC_STATUS_FILE || '/run/ledbox/ledSyncVideo_status.json';

// Safety: ensure absolute path
if (!SYNC_CMD || !path.isAbsolute(SYNC_CMD)) {
  console.error('SYNC_CMD must be an absolute path to an executable script/binary.');
  process.exit(1);
}

console.log(`Starting LED Sync Video wrapper with cmd=${SYNC_CMD}`);

async function atomicWrite(filePath, data) {
  const dir = path.dirname(filePath);
  await fs.mkdir(dir, { recursive: true });
  const tmp = `${filePath}.${process.pid}.${Date.now()}.tmp`;
  await fs.writeFile(tmp, data, 'utf8');
  await fs.rename(tmp, filePath);
}

async function writeStatus(status) {
  try {
    await atomicWrite(STATUS_FILE, JSON.stringify(status, null, 2));
  } catch (e) {
    console.error('Failed to write status file', e);
  }
}

async function runSyncCmd() {
  const startTime = new Date();
  const startIso = startTime.toISOString();
  console.log(`${startIso} - Starting LED Sync Video, executing ${SYNC_CMD}`);

  let child;
  try {
    // spawn the process; execFile returns a ChildProcess that contains .pid and streams
    child = execFile(SYNC_CMD, { cwd: path.dirname(SYNC_CMD) });

  } catch (spawnErr) {
    // execFile can throw synchronously in rare cases
    console.error('Failed to start process synchronously:', spawnErr);
    await writeStatus({
      pid: null,
      running: false,
      last_start: startIso,
      last_end: new Date().toISOString(),
      exitCode: null,
      message: `spawn_error: ${spawnErr.message}`
    });
    return;
  }

  // Write initial running status using the child's pid
  await writeStatus({
    pid: child && child.pid ? child.pid : null,
    running: true,
    last_start: startIso,
    message: 'running'
  });

  // Collect a little stdout/stderr for debugging (optional)
  let stdoutBuf = '';
  let stderrBuf = '';
  if (child.stdout) child.stdout.on('data', (b) => { stdoutBuf += b.toString(); });
  if (child.stderr) child.stderr.on('data', (b) => { stderrBuf += b.toString(); });

  child.on('error', async (err) => {
    console.error('Child process spawn error:', err);
    await writeStatus({
      pid: child.pid || null,
      running: false,
      last_start: startIso,
      last_end: new Date().toISOString(),
      exitCode: null,
      message: `error: ${err.message}`,
      stdout: stdoutBuf || undefined,
      stderr: stderrBuf || undefined
    });
  });

  child.on('exit', async (code, signal) => {
    const endTime = new Date();
    const durationSeconds = Math.round((endTime - startTime) / 1000);
    console.log(`Child exited pid=${child.pid} code=${code} signal=${signal}`);
    if (stdoutBuf) console.log('stdout:', stdoutBuf);
    if (stderrBuf) console.log('stderr:', stderrBuf);

    await writeStatus({
      pid: child.pid || null,
      running: false,
      last_start: startIso,
      last_end: endTime.toISOString(),
      exitCode: code,
      signal: signal || null,
      durationSeconds,
      message: code === 0 ? 'completed' : `failed (code ${code})`,
      stdout: stdoutBuf || undefined,
      stderr: stderrBuf || undefined
    });
  });
}

// Run immediately on start
runSyncCmd().catch(err => {
  console.error('Unexpected error running sync command wrapper:', err);
});