const root = '';

export async function fetchStatus(){
  try{
    const res = await fetch(root + '/api/status');
    if(!res.ok) throw new Error('network');
    return await res.json();
  }catch(e){
    return {running:false, pid:null, lastStart:null, message:'offline'};
  }
}

export async function fetchConfig(){
  const res = await fetch(root + '/api/config');
  if(!res.ok) throw new Error('fetch config');
  return await res.text();
}

export async function saveConfig(text){
  const res = await fetch(root + '/api/config', {method:'POST', headers:{'Content-Type':'text/plain'}, body:text});
  if(!res.ok) throw new Error('save failed');
  return await res.text();
}

export async function triggerCalibrate(){
  await fetch(root + '/api/control/calibrate', {method:'POST'});
}

export async function triggerSync(){
  await fetch(root + '/api/control/sync', {method:'POST'});
}
