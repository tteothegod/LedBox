const { execFile } = require('child_process');
const path = require('path');
const Gpio = require('onoff').Gpio;

// Configure via env
const SYNC_PIN = parseInt(process.env.SYNC_PIN || '22', 10); // BCM pin
const CALIBRATE_PIN = parseInt(process.env.CALIBRATE_PIN || '23', 10); // BCM pin
const EDGE = process.env.TRIGGER_EDGE || 'falling'; // falling because we want when button goes low (pressed). Not clk rising/falling
const DEBOUNCE_MS = parseInt(process.env.DEBOUNCE_MS || '200', 10);
const SYNC_CMD = process.env.SYNC_CMD || '/home/magodi12/ledProject/sync_cmd.sh';
const CALIBRATE_CMD = process.env.CALIBRATE_CMD || '/home/magodi12/ledProject/calibrate_cmd.sh';

// Safety: ensure absolute path
if (!SYNC_CMD || !path.isAbsolute(SYNC_CMD)) {
  console.error('SYNC_CMD must be an absolute path to an executable script/binary.');
  process.exit(1);
}

console.log(`Starting GPIO watcher: pin=${SYNC_PIN}, edge=${EDGE}, cmd=${SYNC_CMD}`);
console.log(`Starting GPIO watcher: pin=${CALIBRATE_PIN}, edge=${EDGE}, cmd=${CALIBRATE_CMD}`);

let syncRunning = false;
let calibrateRunning = false;

const syncButton = new Gpio(SYNC_PIN, 'in', EDGE, { debounceTimeout: DEBOUNCE_MS });
const calibrateButton = new Gpio(CALIBRATE_PIN, 'in', EDGE, { debounceTimeout: DEBOUNCE_MS });

async function runSyncCmd() {
  if (syncRunning) {
    console.log('Sync Program already running; skipping trigger');
    return;
  }
  syncRunning = true;
  console.log(`${new Date().toISOString()} - Triggered, executing ${SYNC_CMD}`);
  execFile(SYNC_CMD, { cwd: path.dirname(SYNC_CMD) }, (err, stdout, stderr) => {
    syncRunning = false;
    if (err) {
      console.error('Command failed:', err);
      if (stdout) console.log('stdout:', stdout.toString());
      if (stderr) console.log('stderr:', stderr.toString());
      return;
    }
    console.log('Command completed. stdout:', stdout?.toString());
  });
}

async function runCalibrateCmd() {
    if (calibrateRunning) {
        console.log('Calibrate Program already running; skipping trigger');
        return;
    }
    calibrateRunning = true;
    console.log(`${new Date().toISOString()} - Calibrate triggered, executing ${CALIBRATE_CMD}`);
    execFile(CALIBRATE_CMD, { cwd: path.dirname(CALIBRATE_CMD) }, (err, stdout, stderr) => {
        calibrateRunning = false;
        if (err) {
            console.error('Calibrate command failed:', err);
            if (stdout) console.log('stdout:', stdout.toString());
            if (stderr) console.log('stderr:', stderr.toString());
            return;
        }
        console.log('Calibrate command completed. stdout:', stdout?.toString());
    });
}

// The callback gets (err, value) — value is the pin level after edge
calibrateButton.watch((err, value) => {
  if (err) {
    console.error('GPIO watch error', err);
    return;
  }
  runCalibrateCmd().catch(e => console.error('runCalibrateCmd error', e));
});

syncButton.watch((err, value) => {
    if(err) {
        console.error('GPIO watch error', err);
        return;
    }
    runSyncCmd().catch(e => console.error('runSyncCmd error', e));
});

// Cleanup on exit
function cleanup() {
  try { syncButton.unexport(); } catch (e) {}
  try { calibrateButton.unexport(); } catch (e) {}
  process.exit(0);
}
process.on('SIGINT', cleanup);
process.on('SIGTERM', cleanup);