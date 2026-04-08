import json
import subprocess
import re
import time

AUDIO_DEVICE = "plughw:2,0"
RAM_DISK_FILE = "/dev/shm/hdmi_test.wav"
THRESHOLD = 0.050
SERVICE_NAME = "ledbox-ledSyncVideo.service"
calibrate_status_file = "/run/ledbox/calibrate_status.json"
sync_status_file = "/run/ledbox/ledSyncVideo_status.json"

def get_rms_amplitude():
    cmd = f"arecord -D {AUDIO_DEVICE} -d 1 -f S16_LE -r 48000 {RAM_DISK_FILE} 2>/dev/null && sox {RAM_DISK_FILE} -n stat 2>&1"
    # arecord -D plughw:2,0 -d 1 -f S16_LE -r 48000 RAM_DISK_FILE 2>/dev/null && sox RAM_DISK_FILE -n stat 2>&1
    try:
        output = subprocess.check_output(cmd, shell=True, text=True)
        match = re.search(r"RMS\s+amplitude:\s+([0-9.]+)", output)
        if match:
            return float(match.group(1))
    except Exception:
        pass
    return 0.0

print("Starting HDMI Watcher...")

def get_status(status_file):
    try:
        with open(status_file, 'r') as f:
            return json.load(f)
    except Exception as e:
        print(f"Error reading status file {status_file}: {e}")
        return {}

# Assume video is initially off to force a check
video_active = False 

while True:
    # /run/ledbox/calibrate_status.json
    calibrate_status = get_status(calibrate_status_file)
    if calibrate_status.get('running', False):
        print("Calibration in progress. Skipping HDMI check.")
        time.sleep(2)
        continue

    sync_status = get_status(sync_status_file)
    if sync_status.get('running', False):
        print("LED Sync Video is already running. Skipping HDMI check.")
        time.sleep(2)
        continue

    rms = get_rms_amplitude()
    print(f"RMS Amplitude: {rms:.4f}")
    
    if rms > THRESHOLD:
        if not video_active:
            print(f"[ACTIVE] Video signal detected (RMS: {rms:.4f}). Starting {SERVICE_NAME}...")
            subprocess.run(["sudo", "systemctl", "start", SERVICE_NAME])
            video_active = True
    else:
        if video_active:
            print(f"[NO SIGNAL] Video lost (RMS: {rms:.4f}). Stopping {SERVICE_NAME}...")
            subprocess.run(["sudo", "systemctl", "stop", SERVICE_NAME])
            video_active = False
            
    # Wait 2 seconds before checking again (adjustable)
    time.sleep(2)