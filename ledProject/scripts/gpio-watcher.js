const { execFile, exec, execSync } = require('child_process');
const path = require('path');
const Gpio = require('onoff').Gpio;

// Configure via env
// CALIBRATE_PIN is the sysfs offset pin (e.g., 536)
const CALIBRATE_PIN = parseInt(process.env.CALIBRATE_PIN || '536', 10); 
// HARDWARE_BCM_PIN is the physical BCM pin (e.g., 24, since 512 + 24 = 536)
const HARDWARE_BCM_PIN = process.env.HARDWARE_BCM_PIN || '24'; 

const BTN_EDGE = process.env.BTN_EDGE || 'falling'; // falling because we want when button goes low (pressed). Not clk rising/falling
const DEBOUNCE_MS = parseInt(process.env.DEBOUNCE_MS || '200', 10);
const CALIBRATE_CMD = process.env.CALIBRATE_CMD || '/home/magodi12/LedBox/ledProject/scripts/calibrate_cmd.sh';
const BASE_DIR = process.env.BASE_DIR || '/home/magodi12/LedBox/ledProject';

console.log(`Starting GPIO watcher: sysfs_pin=${CALIBRATE_PIN}, hardware_pin=${HARDWARE_BCM_PIN}, edge=${BTN_EDGE}, cmd=${CALIBRATE_CMD}`);

// ---------------------------------------------------------
// FIX: Enable the internal Pull-Up resistor via system call
// ---------------------------------------------------------
try {
    console.log(`Enabling internal pull-up resistor on BCM pin ${HARDWARE_BCM_PIN}...`);
    // Try modern Bookworm/Pi5 tool first
    execSync(`pinctrl set ${HARDWARE_BCM_PIN} pu`);
    console.log('Pull-up configured via pinctrl.');
} catch (e1) {
    try {
        // Fallback to Buster/Bullseye/Pi4 tool
        execSync(`raspi-gpio set ${HARDWARE_BCM_PIN} pu`);
        console.log('Pull-up configured via raspi-gpio.');
    } catch (e2) {
        console.warn('WARNING: Failed to set pull-up resistor. If you do not have a physical 10k resistor, the button will float!');
    }
}
// ---------------------------------------------------------

let calibrateProcess = null;

// Now that the hardware is pulled HIGH, we can safely listen for the 'falling' edge
const calibrateButton = new Gpio(CALIBRATE_PIN, 'in', BTN_EDGE, { debounceTimeout: DEBOUNCE_MS });

function stopSyncService() {
    return new Promise((resolve) => {
        exec('sudo systemctl is-active ledbox-ledSyncVideo.service', (err, stdout) => {
            const isActive = stdout.trim() === 'active';
            if (isActive) {
                console.log('Stopping ledbox-ledSyncVideo.service...');
                exec('sudo systemctl stop ledbox-ledSyncVideo.service', (stopErr) => {
                    if (stopErr) console.error('Failed to stop service:', stopErr);
                    resolve(true); // Indicate it was running
                });
            } else {
                console.log('ledbox-ledSyncVideo.service is not active, no need to stop.');
                resolve(false); // Indicate it wasn't running
            }
        });
    });
}

function startSyncService() {
    return new Promise((resolve) => {
        console.log('Starting ledbox-ledSyncVideo.service...');
        exec('sudo systemctl start ledbox-ledSyncVideo.service', (err, stdout, stderr) => {
            if (err) console.error('Failed to start service:', err);
            resolve();
        });
    });
}

async function runCalibrateCmd() {
    if (calibrateProcess) {
        console.log('Calibrate Program already running; skipping trigger');
        return;
    }
    
    // Stop the active sync systemd service to prevent hardware resource collisions
    console.log('Calibrate triggered. Checking sync service status...');
    const wasSyncRunning = await stopSyncService();
    
    if (wasSyncRunning) {
        // Give the service a brief moment to cleanly release the hardware
        await new Promise(resolve => setTimeout(resolve, 500));
    }

    console.log(`${new Date().toISOString()} - Executing ${CALIBRATE_CMD}`);
    calibrateProcess = execFile(CALIBRATE_CMD, { cwd: BASE_DIR }, async (err, stdout, stderr) => {
        
        if (err) {
            console.error('Calibrate command failed:', err);
            if (stdout) console.log('stdout:', stdout.toString());
            if (stderr) console.log('stderr:', stderr.toString());
        } else {
            console.log('Calibrate command completed. stdout:', stdout?.toString());
        }
        
        // Reset the lock so it can be pressed again
        calibrateProcess = null;

        // Only restart sync service if it was running beforehand
        if (wasSyncRunning) {
            console.log('Calibrate finished. Resuming sync service...');
            await startSyncService();
        } else {
            console.log('Calibrate finished.');
        }
    });
}

// The callback gets (err, value) — value is the pin level after edge
calibrateButton.watch((err, value) => {
  if (err) {
    console.error('GPIO watch error', err);
    return;
  }
  
  console.log(`[GPIO INTERRUPT] Button pressed! Pin level: ${value}`);
  
  // Notice we must wrap the execution to handle async rejection
  runCalibrateCmd().catch(e => console.error('runCalibrateCmd error', e));
});

// Cleanup on exit
function cleanup() {
  try { calibrateButton.unexport(); } catch (e) {}
  if (calibrateProcess) calibrateProcess.kill('SIGTERM');
  process.exit(0);
}
process.on('SIGINT', cleanup);
process.on('SIGTERM', cleanup);