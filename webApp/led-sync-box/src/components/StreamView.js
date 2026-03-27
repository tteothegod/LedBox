import React, { useEffect, useState, useRef } from 'react';
import { fetchScreenshot } from '../utils/api';

export default function StreamView() {
  const [enabled, setEnabled] = useState(true);
  const [src, setSrc] = useState(null);
  const [error, setError] = useState(null);
  const [ts, setTs] = useState(null);
  const timerRef = useRef(null);
  const urlRef = useRef(null);

  useEffect(() => {
    let mounted = true;

    async function fetchImg() {
      const now = Date.now();
      try {
        setError(null);
        const blob = await fetchScreenshot(now);
        if (!mounted) return;
        // revoke previous object URL
        if (urlRef.current) URL.revokeObjectURL(urlRef.current);
        const url = URL.createObjectURL(blob);
        urlRef.current = url;
        setSrc(url);
        setTs(now);
      } catch (err) {
        console.error('Error fetching screenshot', err);
        if (!mounted) return;
        setError(err.message || 'failed');
        setSrc(null);
      }
    }

    // Start immediate fetch
    if (enabled) fetchImg();

    // Start interval polling
    if (enabled) {
      timerRef.current = setInterval(fetchImg, 800);
    } else if (timerRef.current) {
      clearInterval(timerRef.current);
      timerRef.current = null;
    }

    return () => {
      mounted = false;
      if (timerRef.current) clearInterval(timerRef.current);
      if (urlRef.current) URL.revokeObjectURL(urlRef.current);
    };
  }, [enabled]);

  return (
    <div className="stream-view">
        <div className="stream-controls">
        <label className="switch">
          <input type="checkbox" checked={enabled} onChange={e => setEnabled(e.target.checked)} />
          <span className="slider" />
        </label>
        <div className="muted">Auto-refresh</div>
        <a
          className={`btn small-outline ${!src ? 'disabled' : ''}`}
          href={src || undefined}
          download={src ? `stream-${ts || Date.now()}.jpg` : undefined}
          onClick={e => { if (!src) e.preventDefault(); }}
          aria-disabled={!src}
        >
          Download Snapshot
        </a>
      </div>

      <div className="stream-wrap">
        {enabled ? (
          src ? (
            <img alt="Live" src={src} className="live-img" />
          ) : (
            <div className="muted">{error ? `Error loading image: ${error}` : 'Loading...'}</div>
          )
        ) : (
          <div className="muted">Auto-refresh paused. Use Download to fetch a snapshot.</div>
        )}
      </div>
    </div>
  );
}
