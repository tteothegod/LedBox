import React, {useEffect, useState} from 'react';
import {fetchConfig, saveConfig} from '../utils/api';

function parseConfigText(text){
  const lines = text.split(/\r?\n/);
  const obj = {};
  for(const l of lines){
    const t = l.trim();
    if(!t || t.startsWith('#')) continue;
    const i = t.indexOf('='); if(i<0) continue;
    const k = t.slice(0,i); const v = t.slice(i+1);
    obj[k]=v;
  }
  return obj;
}

function toConfigText(obj){
  const header = `# LedBox Configuration File\n`;
  const lines = [];
  for(const k of Object.keys(obj)) lines.push(`${k}=${obj[k]}`);
  return header + lines.join('\n') + '\n';
}

export default function ConfigForm(){
  const [raw, setRaw] = useState('');
  const [dirty, setDirty] = useState(false);
  const [saving, setSaving] = useState(false);

  async function load(){
    try{
      const t = await fetchConfig(); 
      setRaw(t); 
      setDirty(false);
    }catch(e){ setRaw('# failed to load config: ' + e.message); }
  }

  useEffect(()=>{ load(); }, []);

  async function onSave(){
    setSaving(true);
    try{
      await saveConfig(raw);
      setDirty(false);
      await load();
      alert('Config saved');
    }catch(e){ alert('Save failed'); }
    setSaving(false);
  }

  function autoTune(){
    // small helper to expose BRIGHTNESS slider convenience
    const obj = parseConfigText(raw);
    if(obj.BRIGHTNESS){
      let b = Math.max(32, Math.min(255, Math.round(Number(obj.BRIGHTNESS)*0.9)));
      obj.BRIGHTNESS = String(b);
      setRaw(toConfigText(obj)); setDirty(true);
    } else alert('BRIGHTNESS not found');
  }

  return (
    <div className="config-form">
      <div className="muted">Edit `config.txt` settings below. Click Save to write to disk (backend required).</div>
      <textarea value={raw} onChange={e=>{setRaw(e.target.value); setDirty(true)}} rows={12} />
      <div className="form-row">
        <button className="btn" onClick={load}>Reload</button>
        <button className="btn outline" onClick={onSave} disabled={!dirty || saving}>{saving? 'Saving…' : 'Save Config'}</button>
        <button className="btn outline" onClick={autoTune}>Auto Adjust Brightness</button>
      </div>
      <div className="note muted">Tip: Backend endpoint POST /api/config expects raw file text.</div>
    </div>
  );
}
