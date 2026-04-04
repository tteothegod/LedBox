import subprocess
import re
import time

AUDIO_DEVICE = "plughw:2,0"
RAM_DISK_FILE = "/dev/shm/hdmi_test.wav"
THRESHOLD = 0.010
SERVICE_NAME = "ledbox-ledSyncVideo.service"

def get_rms_amplitude():
    cmd = f"arecord -D {AUDIO_DEVICE} -d 1 -f S16_LE -r 48000 {RAM_DISK_FILE} 2>/dev/null && sox {RAM_DISK_FILE} -n stat 2>&1"
    try:
        output = subprocess.check_output(cmd, shell=True, text=True)
        match = re.search(r"RMS\s+amplitude:\s+([0-9.]+)", output)
        if match:
            return float(match.group(1))
    except Exception:
        pass
    return 0.0

print("Starting HDMI Watcher...")

# Assume video is initially off to force a check
video_active = False 

while True:
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