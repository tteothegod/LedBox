import React, {useEffect, useState, useRef} from 'react';

export default function StreamView(){
  const [enabled, setEnabled] = useState(true);
  const [ts, setTs] = useState(Date.now());
  const timerRef = useRef(null);

  useEffect(()=>{
    if(enabled){
      timerRef.current = setInterval(()=> setTs(Date.now()), 800);
    }else{ clearInterval(timerRef.current); }
    return ()=> clearInterval(timerRef.current);
  }, [enabled]);

  const url = `/api/stream.jpg?ts=${ts}`;

  return (
    <div className="stream-view">
      <div className="stream-controls">
        <label className="switch">
          <input type="checkbox" checked={enabled} onChange={e=>setEnabled(e.target.checked)} />
          <span className="slider" />
        </label>
        <div className="muted">Auto-refresh</div>
        <a className="btn small" href={url} download={`stream-${ts}.jpg`}>Download Snapshot</a>
      </div>

      <div className="stream-wrap">
        {enabled ? (
          <img alt="Live" src={url} className="live-img" />
        ) : (
          <div className="muted">Auto-refresh paused. Use Download to fetch a snapshot.</div>
        )}
      </div>
    </div>
  );
}
