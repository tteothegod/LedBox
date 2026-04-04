const port = 3000;
const root = `${window.location.protocol}//${window.location.hostname}:${port}`;

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
  if (!res.ok) throw new Error('fetch config');
  const ct = (res.headers.get('content-type') || '').toLowerCase();
  // Only accept plain text or JSON for config; reject HTML pages (error pages)
  if (!(ct.startsWith('text/plain') || ct.includes('application/json'))) {
    throw new Error(`Unexpected content-type: ${ct || 'none'}`);
  }
  const body = await res.text();
  // very basic heuristic to detect HTML error pages
  if (/<(html|!doctype)/i.test(body)) throw new Error('Response appears to be HTML');
  return body;
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

export async function fetchScreenshot(ts) {
  const tsLine = ts ? `?ts=${encodeURIComponent(ts)}` : '';
  const res = await fetch(root + '/api/screenshot' + tsLine);
  if (!res.ok) {
    throw new Error(`network (${res.status})`);
  }
  const blob = await res.blob();
  if (!blob.type || !blob.type.startsWith('image/')) {
    throw new Error('Response is not an image');
  }
  return blob;
}
