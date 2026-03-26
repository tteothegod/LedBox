import React, {useEffect, useState} from 'react';
import {fetchStatus, triggerCalibrate, triggerSync} from '../utils/api';

export default function StatusPanel(){
  const [status, setStatus] = useState({running:false, pid:null, lastStart:null, message:'unknown'});
  const [loading, setLoading] = useState(true);

  async function load(){
    setLoading(true);
    try{
      const s = await fetchStatus();
      setStatus(s);
    }catch(e){
      setStatus(prev=>({...prev, message:'failed to load'}));
    }finally{setLoading(false)}
  }

  useEffect(()=>{ load(); const t = setInterval(load,5000); return ()=>clearInterval(t); }, []);

  return (
    <div className="status-panel">
      {loading ? <div className="muted">Loading status…</div> : (
        <div>
          <div className="status-row"><strong>Running:</strong> <span className={status.running? 'ok':'bad'}>{String(status.running)}</span></div>
          <div className="status-row"><strong>PID:</strong> <span>{status.pid ?? '—'}</span></div>
          <div className="status-row"><strong>Last start:</strong> <span>{status.lastStart ?? '—'}</span></div>
          <div className="status-row"><strong>Message:</strong> <span className="muted">{status.message}</span></div>

          <div className="actions">
            <button onClick={async()=>{ await triggerSync(); load(); }} className="btn">Trigger Sync</button>
            <button onClick={async()=>{ await triggerCalibrate(); load(); }} className="btn outline">Run Calibrate</button>
            <button onClick={load} className="btn small">Refresh</button>
          </div>
        </div>
      )}
    </div>
  );
}
