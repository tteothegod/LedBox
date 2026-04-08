const { exec, execSync } = require('child_process');
const path = require('path');
const Gpio = require('onoff').Gpio;

// Configure via env
// CALIBRATE_PIN is the sysfs offset pin (e.g., 536)
const CALIBRATE_PIN = parseInt(process.env.CALIBRATE_PIN || '536', 10); 
// HARDWARE_BCM_PIN is the physical BCM pin (e.g., 24, since 512 + 24 = 536)
const HARDWARE_BCM_PIN = process.env.HARDWARE_BCM_PIN || '24'; 

const BTN_EDGE = process.env.BTN_EDGE || 'falling'; // falling because we want when button goes low (pressed). Not clk rising/falling
const DEBOUNCE_MS = parseInt(process.env.DEBOUNCE_MS || '200', 10);
const CALIBRATE_WRAPPER = path.join(__dirname, 'calibrate_wrapper.js');

console.log(`Starting GPIO watcher: sysfs_pin=${CALIBRATE_PIN}, hardware_pin=${HARDWARE_BCM_PIN}, edge=${BTN_EDGE}, wrapper=${CALIBRATE_WRAPPER}`);

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

function runCalibrateWrapper() {
    if (calibrateProcess) {
        console.log('Calibrate wrapper already running; skipping trigger');
        return;
    }
    
    console.log('Launching calibrate_wrapper.js...');
    // We launch the wrapper in the background
    calibrateProcess = exec(`node ${CALIBRATE_WRAPPER}`, (err, stdout, stderr) => {
        if (err) {
            console.error('Calibrate wrapper failed:', err);
        }
        if (stdout) console.log('wrapper stdout:', stdout.toString());
        if (stderr) console.log('wrapper stderr:', stderr.toString());
        
        // Reset the lock so it can be pressed again later
        calibrateProcess = null;
    });
}

// The callback gets (err, value) — value is the pin level after edge
calibrateButton.watch((err, value) => {
  if (err) {
    console.error('GPIO watch error', err);
    return;
  }
  
  console.log(`[GPIO INTERRUPT] Button pressed! Pin level: ${value}`);
  
  runCalibrateWrapper();
});

// Cleanup on exit
function cleanup() {
  try { calibrateButton.unexport(); } catch (e) {}
  if (calibrateProcess) calibrateProcess.kill('SIGTERM');
  process.exit(0);
}
process.on('SIGINT', cleanup);
process.on('SIGTERM', cleanup);