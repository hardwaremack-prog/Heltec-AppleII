// Settings page: http://<board-ip>/settings
// Generated from web/settings.html by tools/make_web.py - edit that file, not this one.
#pragma once
static const char SETTINGS_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Heltec-AppleII Settings</title>
<style>
:root{--bg:#d9d2bd;--case:#efe8d4;--ink:#2a261c;--dim:#6f6754;--card:#fffaf0;--line:#d8cfb6;--ok:#2f8f1c;--warn:#b07400;--bad:#b3261e;--acc:#2a9be0}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.45 ui-monospace,"Cascadia Mono",Menlo,Consolas,monospace}
.frame{max-width:860px;margin:16px auto;background:var(--case);padding:18px 16px 28px;border-radius:8px;box-shadow:0 2px 0 #b9b09a}
@media (max-width:880px){.frame{margin:0;border-radius:0}}
h1{font-size:20px;letter-spacing:.04em;margin:0 0 2px}
.stripes{display:inline-flex;height:14px;width:22px;flex-direction:column;vertical-align:-1px;margin-right:6px;border-radius:2px;overflow:hidden}
.stripes i{flex:1}
.sub{color:var(--dim);margin:0 0 16px}
.sub a{color:var(--acc)}
h2{font-size:13px;text-transform:uppercase;letter-spacing:.08em;margin:0 0 10px}
section{background:var(--card);padding:14px;margin:12px 0;border-radius:6px;border:1px solid var(--line)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:8px}
.stat{background:#f4eedd;padding:8px 10px;border-radius:4px}
.stat b{display:block;font-size:11px;color:var(--dim);font-weight:normal;text-transform:uppercase}
.stat span{font-size:15px;overflow-wrap:anywhere}
table{width:100%;border-collapse:collapse}
td,th{text-align:left;padding:6px 4px;border-bottom:1px solid var(--line);vertical-align:middle}
th{font-size:11px;color:var(--dim);font-weight:normal;text-transform:uppercase}
button,select,input[type=text],input[type=password],textarea{font:inherit;color:var(--ink);background:#fff;border:1px solid var(--line);border-radius:4px;padding:6px 10px}
button{cursor:pointer;background:var(--ink);color:var(--case);border-color:var(--ink);font-weight:bold}
button.alt{background:transparent;color:var(--ink)}
button:disabled{opacity:.5;cursor:default}
input[type=file]{color:var(--dim);max-width:100%}
textarea{width:100%;min-height:70px}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-top:8px}
.ok{color:var(--ok)}.warn{color:var(--warn)}.bad{color:var(--bad)}
.note{color:var(--dim);font-size:13px;margin:8px 0 0}
code{background:#f0e9d6;padding:1px 5px;border-radius:3px}
.drives{display:grid;grid-template-columns:1fr 1fr;gap:8px}
@media (max-width:560px){.drives{grid-template-columns:1fr}.hide-s{display:none}}
.drive{background:#2b2820;color:#efe8d4;border-radius:6px;padding:10px 12px}
.drive b{display:block;font-size:11px;color:#b3aa92;font-weight:normal;text-transform:uppercase}
.drive .nm{font-size:15px;margin:2px 0 6px;min-height:22px;overflow-wrap:anywhere}
.drive button{background:#efe8d4;color:#2b2820;padding:4px 9px}
.drive label{font-size:12px;color:#b3aa92}
.led{display:inline-block;width:12px;height:7px;border-radius:2px;background:#4a1515;margin-left:6px}
.led.on{background:#ff3b2f;box-shadow:0 0 6px #ff3b2f}
a.del{color:var(--bad);text-decoration:none;font-weight:bold;padding:0 4px}
#toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);background:var(--ink);color:var(--case);padding:8px 14px;border-radius:4px;font-weight:bold;display:none}
</style></head><body><div class="frame">
<h1><span class="stripes"><i style="background:#6fd63a"></i><i style="background:#f2b632"></i><i style="background:#e8572e"></i><i style="background:#c8326b"></i><i style="background:#9b4fd1"></i><i style="background:#2a9be0"></i></span>Heltec-AppleII</h1>
<p class="sub">Apple IIe / ][+ on a Heltec V3 &middot; <a href="/">&larr; back to the Apple</a></p>

<section><h2>Status</h2><div class="grid">
<div class="stat"><b>Open the Apple at</b><span id="addr">-</span></div>
<div class="stat"><b>Machine</b><span id="machine">-</span></div>
<div class="stat"><b>Viewer</b><span id="viewer">-</span></div>
<div class="stat"><b>Speed</b><span id="speed">-</span></div>
<div class="stat"><b>Wi-Fi</b><span id="wifi">-</span></div>
</div>
<div class="row"><button onclick="act('reset')">Ctrl-Reset</button><button class="alt" onclick="if(confirm('Power-cycle the Apple? Anything unsaved is lost.'))act('power')">Power off &amp; on</button>
<label><input type="checkbox" id="warp" onchange="act('warp?on='+(this.checked?1:0))"> Warp speed (fast disk loading)</label></div>
</section>

<section><h2>Disk drives (slot 6)</h2>
<div class="drives">
 <div class="drive"><b>Drive 1<span class="led" id="led0"></span></b><div class="nm" id="dn0">-</div><div id="dc0"></div></div>
 <div class="drive"><b>Drive 2<span class="led" id="led1"></span></b><div class="nm" id="dn1">-</div><div id="dc1"></div></div>
</div>
<table style="margin-top:10px"><thead><tr><th>Disk image</th><th class="hide-s">Size</th><th></th></tr></thead><tbody id="disks"></tbody></table>
<div class="row"><input type="file" id="dskfile" accept=".dsk,.do,.po,.DSK,.DO,.PO"><label><input type="checkbox" id="dskboot" checked> put in drive 1 and boot</label><button onclick="upDisk()" id="dskbtn">Upload</button><span id="dskprog" class="note"></span></div>
<p class="note">140&nbsp;KB 5.25" images: <code>.dsk</code> / <code>.do</code> (DOS 3.3 order) or <code>.po</code> (ProDOS order). Programs that save to disk write straight back into the image on the board; download it with &darr; to keep a copy. Copy-protected originals need a cracked image. <span id="fsfree"></span></p>
</section>

<section><h2>System ROM</h2>
<div id="rominfo" class="note">-</div>
<div class="row"><input type="file" id="romfile" accept=".rom,.bin,.ROM,.BIN"><button onclick="upRom()">Upload &amp; restart</button><a href="#" id="romdel" class="del" style="display:none" onclick="delRom();return false">remove</a></div>
<p class="note">Upload your own Apple ROM dump: <b>Apple IIe or enhanced IIe</b>, 16&nbsp;KB (<code>$C000-$FFFF</code>, e.g. 342-0304 + 342-0303 joined), or a 32&nbsp;KB IIe file; <b>Apple ][+</b>, 12&nbsp;KB (<code>$D000-$FFFF</code>). Apple's ROMs aren't included. The Disk II boot ROM is built in (own code), so you don't need one.</p>
</section>

<section><h2>Type into the Apple</h2>
<textarea id="typed" placeholder='10 PRINT "HELLO MESH"&#10;20 GOTO 10&#10;RUN'></textarea>
<div class="row"><button onclick="typeIt()">Type it</button></div>
</section>

<section><h2>Wi-Fi</h2>
<div class="row"><select id="ssidsel" onchange="if(this.value)ssid.value=this.value"><option value="">Scan for networks&hellip;</option></select><button class="alt" onclick="scan()">Scan</button></div>
<div class="row"><input type="text" id="ssid" placeholder="Network name"><input type="password" id="pass" placeholder="Password"><button onclick="saveWifi()">Save &amp; reboot</button></div>
<p class="note">If the board can't join your network it starts its own hotspot <code>AppleII-Setup</code> (password <code>appleiie</code>); this page is then at <code>http://192.168.4.1/settings</code>.</p>
</section>
</div><div id="toast"></div>
<script>
const $=id=>document.getElementById(id);
function toast(t){const e=$('toast');e.textContent=t;e.style.display='block';clearTimeout(e._t);e._t=setTimeout(()=>e.style.display='none',2600)}
function esc(s){return String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
async function act(a){const r=await fetch('/api/'+a,{method:'POST'});toast(r.ok?'OK':await r.text());setTimeout(load,400)}
let st={};
async function load(){
 try{st=await (await fetch('/api/status')).json()}catch(e){return}
 $('addr').innerHTML='<a href="http://'+st.ip+'/">http://'+st.ip+'/</a>';
 $('machine').textContent=st.romName;
 $('viewer').innerHTML=st.viewer?'<span class="ok">connected</span>'+(st.sound?' &#128266;':''):'none';
 $('speed').textContent=st.speed+'%'+(st.warp?' (warp)':'');
 $('warp').checked=!!st.warp;
 $('wifi').textContent=st.mode=='ap'?'Hotspot AppleII-Setup':(st.ssid+' '+st.rssi+' dBm');
 $('rominfo').innerHTML=st.romCustom?'<span class="ok">Your ROM:</span> '+esc(st.romName)+' <small>CRC '+st.romCrc+'</small>':'<span class="warn">No ROM uploaded yet</span> &mdash; the Apple shows a reminder screen until you add one.';
 $('romdel').style.display=st.romCustom?'':'none';
 st.drives.forEach((d,i)=>{
  $('dn'+i).textContent=d.name||'(empty)';
  $('led'+i).classList.toggle('on',!!d.busy);
  $('dc'+i).innerHTML=d.name?`<button onclick="ins(${i},'')">Eject</button> <label><input type="checkbox" ${d.wp?'checked':''} onchange="wp(${i},this.checked)"> write-protect</label> <span style="color:#b3aa92;font-size:12px">track ${d.track}</span>`:'';
 });
 const inDrive=n=>st.drives.findIndex(d=>d.name===n);
 $('disks').innerHTML=st.disks.length?st.disks.map(f=>{const di=inDrive(f.n);const n=esc(f.n);
  return `<tr><td>${di>=0?'<span class="ok">&#9679; drive '+(di+1)+'</span> ':''}${n}</td><td class="hide-s">${Math.round(f.s/1024)} KB</td><td style="text-align:right;white-space:nowrap">
  <button data-n="${n}" onclick="boot(this.dataset.n)">Boot</button> <button class="alt" data-n="${n}" onclick="ins(0,this.dataset.n)">D1</button> <button class="alt" data-n="${n}" onclick="ins(1,this.dataset.n)">D2</button>
  <a href="/api/dl/${encodeURIComponent(f.n)}" download="${n}" title="Download">&darr;</a> <a href="#" class="del" data-n="${n}" onclick="del(this.dataset.n);return false" title="Delete">&times;</a></td></tr>`}).join(''):'<tr><td colspan=3 class="note">No disk images yet &mdash; upload one below.</td></tr>';
 $('fsfree').textContent='Flash free: '+Math.round(st.fsfree/1024)+' KB.';
}
function upProgress(url,file,progEl,btn){return new Promise(res=>{const fd=new FormData();fd.append('file',file,file.name);const x=new XMLHttpRequest();x.open('POST',url);
 btn.disabled=true;x.upload.onprogress=e=>{if(e.lengthComputable)progEl.textContent=Math.round(e.loaded*100/e.total)+'%'};
 x.onload=()=>{btn.disabled=false;progEl.textContent='';toast(x.status==200?'Done':x.responseText);res(x.status==200)};
 x.onerror=()=>{btn.disabled=false;progEl.textContent='';toast('Upload failed');res(false)};x.send(fd)})}
async function upDisk(){const f=$('dskfile').files[0];if(!f)return toast('Pick a .dsk / .po file first');await upProgress('/api/disk?boot='+($('dskboot').checked?1:0),f,$('dskprog'),$('dskbtn'));load()}
async function ins(d,n){const r=await fetch('/api/insert?drive='+d+'&name='+encodeURIComponent(n),{method:'POST'});toast(r.ok?(n?'Disk in drive '+(d+1):'Drive '+(d+1)+' empty'):await r.text());load()}
async function boot(n){const r=await fetch('/api/insert?drive=0&boot=1&name='+encodeURIComponent(n),{method:'POST'});toast(r.ok?'Booting '+n:await r.text());load()}
async function wp(d,on){await fetch('/api/wp?drive='+d+'&on='+(on?1:0),{method:'POST'});load()}
async function del(n){if(!confirm('Delete disk image '+n+'?'))return;await fetch('/api/diskdel?name='+encodeURIComponent(n),{method:'POST'});load()}
async function upRom(){const f=$('romfile').files[0];if(!f)return toast('Pick a ROM file first');const fd=new FormData();fd.append('file',f,f.name);const r=await fetch('/api/rom',{method:'POST',body:fd});toast(r.ok?'ROM installed, restarting the Apple':await r.text());setTimeout(load,600)}
async function delRom(){if(!confirm('Remove your ROM file?'))return;await fetch('/api/romdel',{method:'POST'});load()}
async function typeIt(){const r=await fetch('/api/type',{method:'POST',body:$('typed').value});toast(r.ok?'Typing…':'Failed')}
async function scan(){toast('Scanning…');const l=await (await fetch('/api/scan')).json();$('ssidsel').innerHTML='<option value="">'+l.length+' networks found</option>'+l.map(n=>`<option value="${esc(n.s)}">${esc(n.s)} (${n.r} dBm)</option>`).join('')}
async function saveWifi(){if(!$('ssid').value)return toast('Enter a network name');const b=new URLSearchParams({ssid:$('ssid').value,pass:$('pass').value});await fetch('/api/wifi',{method:'POST',body:b});toast('Saved, rebooting…')}
load();setInterval(load,2000);
</script></body></html>
)HTML";
