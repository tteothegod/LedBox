const { execFile, exec } = require('child_process');
const path = require('path');
const fs = require('fs').promises;

const CALIBRATE_CMD = process.env.CALIBRATE_CMD || '/home/magodi12/LedBox/ledProject/scripts/calibrate_cmd.sh';
const BASE_DIR = process.env.BASE_DIR || '/home/magodi12/LedBox/ledProject';
const CALIBRATE_BINARY = process.env.CALIBRATE_BINARY || '/home/magodi12/LedBox/ledProject/calibrate';
const CALIBRATE_STATUS_FILE = process.env.CALIBRATE_STATUS_FILE || '/run/ledbox/calibrate_status.json'; 

let child = null;



async function atomicWrite(filePath, data) {
  const dir = path.dirname(filePath);
  await fs.mkdir(dir, { recursive: true });
  const tmp = `${filePath}.${process.pid}.${Date.now()}.tmp`;
  await fs.writeFile(tmp, data, 'utf8');
  await fs.rename(tmp, filePath);
}

async function writeStatus(status) {
  try {
    await atomicWrite(CALIBRATE_STATUS_FILE, JSON.stringify(status, null, 2));
  } catch (e) {
    console.error('Failed to write status file', e);
  }
}
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
    console.log('Calibrate wrapper triggered. Checking sync service status...');
    const wasSyncRunning = await stopSyncService();
    
    if (wasSyncRunning) {
        // Give the service a brief moment to cleanly release the hardware
        await new Promise(resolve => setTimeout(resolve, 500));
    }

    console.log(`${new Date().toISOString()} - Executing ${CALIBRATE_BINARY}...`);
    
    try {
        child = execFile(CALIBRATE_BINARY, { cwd: BASE_DIR }, async (err, stdout, stderr) => {
            
            if (err) {
                console.error('Calibrate command failed:', err);
                if (stdout) console.log('stdout:', stdout.toString());
                if (stderr) console.log('stderr:', stderr.toString());
            } else {
                console.log('Calibrate command completed. stdout:', stdout?.toString());
            }
    
            // Only restart sync service if it was running beforehand
            if (wasSyncRunning) {
                console.log('Calibrate finished. Resuming sync service...');
                await startSyncService();
            } else {
                console.log('Calibrate finished.');
            }
        });
    } catch (spawnErr) {
        console.error('Failed to start calibrate command synchronously:', spawnErr);
        await writeStatus({
            pid: null,
            running: false,
            last_start: new Date().toISOString(),
            last_end: new Date().toISOString(),
            exitCode: null,
            message: `spawn_error: ${spawnErr.message}`
        });
        return;
    }

    await writeStatus({
        pid: child && child.pid ? child.pid : null,
        running: true,
        last_start: new Date().toISOString(),
        message: 'running'
    });

    let stdoutBuf = '';
    let stderrBuf = '';
    if (child.stdout) child.stdout.on('data', (b) => { stdoutBuf += b.toString(); });
    if (child.stderr) child.stderr.on('data', (b) => { stderrBuf += b.toString(); });

    child.on('error', async (err) => {
        console.error('Child process spawn error:', err);
        await writeStatus({
            pid: child.pid || null,
            running: false,
            last_start: new Date().toISOString(),
            last_end: new Date().toISOString(),
            exitCode: null,
            message: `error: ${err.message}`,
            stdout: stdoutBuf || undefined,
            stderr: stderrBuf || undefined
        });
    });

    child.on('exit', async (code, signal) => {
        const endTime = new Date();
        // const durationSeconds = Math.round((endTime - startTime) / 1000);
        console.log(`Child exited pid=${child.pid} code=${code} signal=${signal}`);
        if (stdoutBuf) console.log('stdout:', stdoutBuf);
        if (stderrBuf) console.log('stderr:', stderrBuf);

        await writeStatus({
            pid: child.pid || null,
            running: false,
            last_start: new Date().toISOString(),
            last_end: endTime.toISOString(),
            exitCode: code,
            signal: signal || null,
            message: code === 0 ? 'completed' : `failed (code ${code})`,
            stdout: stdoutBuf || undefined,
            stderr: stderrBuf || undefined
        });
    });

}

runCalibrateCmd().catch(e => {
    console.error('runCalibrateCmd error', e);
    process.exit(1);
});
