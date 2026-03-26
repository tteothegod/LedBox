Frontend dashboard notes

This React app provides a simple local dashboard for the LED Sync Box.

Required backend REST endpoints (you should implement on the Pi):

- GET /api/status -> JSON: { running: bool, pid: int|null, lastStart: string|null, message: string }
- GET /api/config -> returns raw `config.txt` text
- POST /api/config -> accepts raw `config.txt` text to overwrite config
- POST /api/control/calibrate -> trigger `./calibrate` (GPIO or spawn)
- POST /api/control/sync -> trigger `./ledSyncVideo` (GPIO or spawn)
- GET /api/stream.jpg -> returns latest camera frame / snapshot (jpg)

Implementation tips:
- Serve this React app (build) from a lightweight server (nginx or express) on the Pi.
- Implement endpoints in a small python/express service that reads/writes `ledProject/config.txt`, triggers the programs, and serves camera snapshots.
- Keep permissions in mind when running programs from a web service (use systemd or a privileged helper to actually run GPIO-triggered programs).
