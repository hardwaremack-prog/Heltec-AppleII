// The Apple II itself, in the browser: http://<board-ip>/
// Generated from web/player.html by tools/make_web.py - edit that file, not this one.
#pragma once
static const char PLAYER_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover,user-scalable=no">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="theme-color" content="#1d1b16">
<title>Heltec-AppleII</title>
<style>
:root{--bg:#141310;--bar:#24221c;--key:#3b382e;--key2:#555044;--ink:#f1ead6;--dim:#b3aa92;--acc:#6fd63a;--warn:#f2b632}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;height:100%;background:var(--bg);color:var(--ink);font:14px/1.3 ui-monospace,"Cascadia Mono",Menlo,Consolas,monospace;overscroll-behavior:none}
body{display:flex;flex-direction:column;height:100dvh;user-select:none;-webkit-user-select:none}
header{display:flex;align-items:center;gap:6px;padding:6px 8px;background:var(--bar);flex-wrap:wrap;padding-top:max(6px,env(safe-area-inset-top))}
header h1{font-size:14px;margin:0 6px 0 2px;letter-spacing:.06em;white-space:nowrap}
header h1 b{color:#6fd63a}header h1 i{color:#f2b632;font-style:normal}header h1 u{color:#e8572e;text-decoration:none}header h1 s{color:#9b4fd1;text-decoration:none}header h1 em{color:#2a9be0;font-style:normal}
.tb{font:inherit;color:var(--ink);background:var(--key);border:0;border-radius:4px;padding:6px 9px;cursor:pointer;white-space:nowrap;text-decoration:none}
.tb.on{background:var(--acc);color:#10260a}
.tb:active{filter:brightness(1.3)}
#status{margin-left:auto;color:var(--dim);font-size:12px;white-space:nowrap;display:flex;align-items:center;gap:8px}
#status i{display:inline-block;width:8px;height:8px;border-radius:50%;background:#e66;vertical-align:middle}
#status.ok i{background:var(--acc)}
.led{display:inline-block;width:10px;height:6px;border-radius:2px;background:#3a1010;margin-left:3px;vertical-align:middle}
.led.on{background:#ff3b2f;box-shadow:0 0 6px #ff3b2f}
#stage{flex:1;min-height:0;display:flex;align-items:center;justify-content:center;position:relative;padding:6px}
canvas{image-rendering:pixelated;image-rendering:crisp-edges;background:#000;box-shadow:0 0 0 1px #000,0 8px 30px rgba(0,0,0,.5);touch-action:none}
#overlay{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;background:rgba(20,19,16,.72);cursor:pointer;text-align:center}
#overlay div{background:var(--bar);padding:18px 22px;border-radius:6px;max-width:320px}
#overlay b{display:block;font-size:18px;margin-bottom:6px}
#overlay span{color:var(--dim)}
#kbd{display:none;background:var(--bar);padding:6px 4px calc(6px + env(safe-area-inset-bottom))}
#kbd .r{display:flex;gap:3px;margin:3px 0;justify-content:center}
.k{flex:1 1 0;min-width:0;height:38px;border:0;border-radius:4px;background:var(--key);color:var(--ink);font:inherit;font-size:13px;padding:0;touch-action:none}
.k.w15{flex-grow:1.5}@media (max-width:480px){.k{font-size:11px}}.k.w2{flex-grow:2}.k.w8{flex-grow:7}
.k.m{background:#2c2a22}
.k.dn,.k.lat{background:var(--acc);color:#10260a}
#joy{display:none;position:absolute;inset:auto 0 0 0;pointer-events:none;justify-content:space-between;align-items:flex-end;padding:14px 18px calc(14px + env(safe-area-inset-bottom))}
#pad,.fire{pointer-events:auto;touch-action:none;border-radius:50%;background:rgba(255,255,255,.10);border:2px solid rgba(255,255,255,.25)}
#pad{width:150px;height:150px;position:relative}
#pad::after{content:"";position:absolute;left:50%;top:50%;width:54px;height:54px;margin:-27px 0 0 -27px;border-radius:50%;background:rgba(255,255,255,.18);transform:translate(var(--dx,0),var(--dy,0))}
#fires{display:flex;gap:12px;align-items:flex-end}
.fire{width:84px;height:84px;display:flex;align-items:center;justify-content:center;font-weight:bold;color:var(--ink);font-size:12px;text-align:center}
.fire.dn{background:rgba(111,214,58,.45)}
dialog{background:var(--bar);color:var(--ink);border:0;border-radius:6px;width:min(520px,92vw)}
dialog textarea{width:100%;height:140px;font:inherit;background:#0e0d0a;color:var(--ink);border:1px solid var(--key2);border-radius:4px;padding:8px}
dialog::backdrop{background:rgba(0,0,0,.6)}
.row{display:flex;gap:8px;justify-content:flex-end;margin-top:8px;flex-wrap:wrap;align-items:center}
.note{color:var(--dim);font-size:12px;margin:6px 0 0}
@media (max-width:640px){header h1{display:none}.lbl{display:none}}
</style></head><body>
<header>
 <h1><b>H</b><i>E</i><u>L</u><s>T</s><em>E</em>C-APPLE ][</h1>
 <button class="tb" id="bSound" title="Sound">&#128264;<span class="lbl"> Sound</span></button>
 <button class="tb" id="bKbd" title="On-screen keyboard">&#9000;<span class="lbl"> Keys</span></button>
 <button class="tb" id="bJoy" title="Joystick">&#127918;<span class="lbl"> Joystick</span></button>
 <button class="tb" id="bCaps" title="Caps Lock">CAPS</button>
 <button class="tb" id="bMon" title="Colour or monochrome monitor">Color</button>
 <button class="tb" id="bPaste" title="Type or paste text">&#128203;<span class="lbl"> Type</span></button>
 <button class="tb" id="bReset" title="Ctrl-Reset">&#8635;<span class="lbl"> Reset</span></button>
 <button class="tb" id="bFull" title="Full screen">&#9974;</button>
 <a class="tb" href="/settings" title="ROM, disks, Wi-Fi">&#9881;<span class="lbl"> Settings</span></a>
 <span id="status"><span>D1<span class="led" id="led1"></span> D2<span class="led" id="led2"></span></span><span><i></i> <span id="stxt">connecting…</span></span></span>
</header>
<div id="stage">
 <canvas id="scr" width="560" height="384"></canvas>
 <div id="overlay"><div><b>]RUN</b><span>Tap or click to start the Apple with sound</span></div></div>
 <div id="joy"><div id="pad"></div><div id="fires"><div class="fire" id="f1">BUTTON<br>1</div><div class="fire" id="f0">BUTTON<br>0</div></div></div>
</div>
<div id="kbd"></div>
<dialog id="dlg"><b>Type into the Apple</b>
 <p class="note">Pasted text is typed on the Apple keyboard. Ctrl+V / Cmd+V on the screen does the same.</p>
 <textarea id="txt" placeholder='10 PRINT "HELLO MESH"&#10;20 GOTO 10&#10;RUN'></textarea>
 <div class="row"><label class="note"><input type="checkbox" id="numJoy"> Numeric keypad = joystick</label>
 <button class="tb" onclick="dlg.close()">Cancel</button><button class="tb on" id="bTypeGo">Type it</button></div>
</dialog>
<script>
"use strict";
const $=id=>document.getElementById(id);
const W=560,H=192;
const FONT=Uint8Array.from(atob('AAAAAAAAAAAICAgICAAIABQUFAAAAAAAFBQ+FD4UFAAIHigcCjwIADAyBAgQJgYAECgoECokGgAICAgAAAAAAAgQICAgEAgACAQCAgIECAAIKhwIHCoIAAAICD4ICAAAAAAAAAgIEAAAAAA+AAAAAAAAAAAAAAgAAAIECBAgAAAcIiYqMiIcAAgYCAgICBwAHCICDBAgPgA+AgQMAiIcAAQMFCQ+BAQAPiA8AgIiHAAOECA8IiIcAD4CBAgQEBAAHCIiHCIiHAAcIiIeAgQ4AAAACAAIAAAAAAAIAAgIEAAECBAgEAgEAAAAPgA+AAAAEAgEAgQIEAAcIgQICAAIABwiKi4sIB4ACBQiIj4iIgA8IiI8IiI8ABwiICAgIhwAPCIiIiIiPAA+ICA8ICA+AD4gIDwgICAAHiAgJiIiHgAiIiI+IiIiABwICAgICBwAAgICAgIiHAAiJCgwKCQiACAgICAgID4AIjYqKiIiIgAiIjIqJiIiABwiIiIiIhwAPCIiPCAgIAAcIiIiKiQaADwiIjwoJCIAHCIgHAIiHAA+CAgICAgIACIiIiIiIhwAIiIiIiIUCAAiIiIqKjYiACIiFAgUIiIAIiIUCAgICAA+AgQIECA+AD4wMDAwMD4AACAQCAQCAAA+BgYGBgY+AAAACBQiAAAAAAAAAAAAPgAQCAQAAAAAAAAAHAIeIh4AICA8IiIiPAAAAB4gICAeAAICHiIiIh4AAAAcIj4gHgAMEhA8EBAQAAAcIiIeAhwAICA8IiIiIgAIABgICAgcAAQADAQEJBgAICAkKDAoJAAYCAgICAgcAAAANCoqKioAAAA8IiIiIgAAABwiIiIcAAA8IiI8ICAAAB4iIh4CAgAAAC4wICAgAAAAHiAcAjwAEBA8EBASDAAAACIiIiYaAAAAIiIiFAgAAAAiIioqFAAAACIUCBQiAAAiIiIeAhwAAAA+BAgQPgAGCAgQCAgGAAgICAgICAgAMAgIBAgIMAAAABAqBAAAACoUKhQqFCoANn9/fn9/NgA2SUFCQUk2AEBgcHh8YFAAfyIUCBQifwAAAQJEKBAAAH9+AkVrd39/AAAAAAAAAH9VKlUqVSpVKggQIH8gEAgAAAAAAAAAAFUICAhJKhwIAAgcKkkICAgAfwAAAAAAAAABARExfzAQAH9/f39/f39/QEBITH9MSEABAQkZfxkJAQB/AAAAAH8AAAAAfwAAAAAICAh/CAgICH9BQUFBQX8ACAQCfwIECAABAQEBAQEBAUBAQEBAQEBAAAAIHD5/AAAAAH8+HAgAAH9AQEBAQEBAfwEBAQEBAQEBAQEBAQEBf0BAQEBAQEB/PkFdVV1BPgBVVVVVVVVVVQ=='),c=>c.charCodeAt(0));
const rgb=c=>0xFF000000|((c&255)<<16)|(c&0xFF00)|(c>>16);
const LORES=[0x000000,0xD0284E,0x2A2E9E,0xD43CE6,0x006F44,0x808080,0x2C8CFF,0xC8B8FF,0x5E5100,0xF26A1F,0x808080,0xFFA0C0,0x2EC02E,0xE6DC52,0x5CF0B4,0xFFFFFF].map(rgb);
const MONO={color:null,green:rgb(0x33FF55),amber:rgb(0xFFB020),white:rgb(0xF4F4F0)};
const BLACK=rgb(0);
const cv=$('scr'),ctx=cv.getContext('2d',{alpha:false});
const img=ctx.createImageData(W,H*2),px=new Uint32Array(img.data.buffer);
const MAIN=new Uint8Array(65536),AUX=new Uint8Array(65536);
let mode=1|256,monitor='color',flash=false,dirty=true;

/*RENDER-START*/
// Draw the Apple II screen from video memory into a 560x384 Uint32 RGBA buffer (each line doubled).
function render(px,MAIN,AUX,mode,monitor,flash){
  const TEXT=mode&1,MIXED=mode&2,PAGE2=mode&4,HIRES=mode&8,COL80=mode&16,DHR=mode&32,ALT=mode&64,STORE80=mode&128,IIE=mode&256;
  const p2=PAGE2&&!STORE80;
  const mono=MONO[monitor];
  const line=new Uint32Array(W);
  for(let y=0;y<192;y++){
    const textLine=TEXT||(MIXED&&y>=160);
    if(textLine) textRow(line,y);
    else if(HIRES) (DHR&&COL80)?dhrRow(line,y):hiresRow(line,y);
    else (DHR&&COL80)?dloresRow(line,y):loresRow(line,y);
    if(mono) for(let x=0;x<W;x++) if(line[x]!==BLACK) line[x]=mono;
    px.set(line,y*2*W);
    // second line slightly darker for a CRT look
    const o=(y*2+1)*W;
    for(let x=0;x<W;x++){const c=line[x];px[o+x]=c===BLACK?c:(0xFF000000|((c&0xFEFEFE)>>1)+((c&0xFCFCFC)>>2));}
  }
  function textBase(r){return (p2?0x800:0x400)+(r&7)*0x80+(r>>3)*0x28}
  function glyph(b){ // -> [fontIndex, inverse, flashing]
    if(b>=0x80){let c=b&0x7F;if(c<0x20)c+=0x40;if(!IIE&&c>=0x60)c-=0x40;return [c-0x20,false,false]}
    if(b<0x40){let c=b&0x3F;if(c<0x20)c+=0x40;return [c-0x20,true,false]}
    if(ALT&&IIE){ if(b<0x60) return [96+(b-0x40),false,false]; return [b-0x20,true,false]; }
    let c=b&0x3F;if(c<0x20)c+=0x40;return [c-0x20,false,true];
  }
  function textRow(line,y){
    const fg=rgb(0xFFFFFF),r=y>>3,row=y&7,base=textBase(r);
    if(COL80&&IIE){
      for(let c=0;c<80;c++){
        const b=(c&1)?MAIN[base+(c>>1)]:AUX[base+(c>>1)];
        drawCell(line,c*7,1,b,row,fg);
      }
    }else for(let c=0;c<40;c++) drawCell(line,c*14,2,MAIN[base+c],row,fg);
  }
  function drawCell(line,x0,sc,b,row,fg){
    const g=glyph(b);let bits=FONT[g[0]*8+row];
    if(g[1]||(g[2]&&flash)) bits^=0x7F;
    for(let i=0;i<7;i++){const on=bits&(0x40>>i);const c=on?fg:BLACK;for(let k=0;k<sc;k++)line[x0+i*sc+k]=c;}
  }
  function loresRow(line,y){
    const r=y>>3,base=textBase(r),hi=(y&7)>=4;
    for(let c=0;c<40;c++){const b=MAIN[base+c];const col=LORES[hi?b>>4:b&15];for(let k=0;k<14;k++)line[c*14+k]=col}
  }
  function dloresRow(line,y){
    const r=y>>3,base=textBase(r),hi=(y&7)>=4;
    for(let c=0;c<80;c++){const b=(c&1)?MAIN[base+(c>>1)]:AUX[base+(c>>1)];let n=hi?b>>4:b&15;
      if(!(c&1)) n=((n<<1)|(n>>3))&15;   // aux colours are rotated by one bit
      const col=LORES[n];for(let k=0;k<7;k++)line[c*7+k]=col}
  }
  function hiresBase(y){return (p2?0x4000:0x2000)+(y&7)*0x400+((y>>3)&7)*0x80+(y>>6)*0x28}
  function hiresRow(line,y){
    const base=hiresBase(y);
    line.fill(BLACK);
    const bits=new Uint8Array(282),pal=new Uint8Array(282);
    for(let c=0;c<40;c++){const b=MAIN[base+c];for(let i=0;i<7;i++){bits[1+c*7+i]=(b>>i)&1;pal[1+c*7+i]=b>>7}}
    if(mono){for(let x=0;x<280;x++)if(bits[x+1]){const s=2*x+pal[x+1];line[s]=rgb(0xFFFFFF);if(s+1<W)line[s+1]=rgb(0xFFFFFF)}return}
    const WHITE=rgb(0xFFFFFF),VIO=LORES[3],GRN=LORES[12],BLU=LORES[6],ORA=LORES[9];
    for(let x=0;x<280;x++){
      const on=bits[x+1],l=bits[x],r=bits[x+2],pb=pal[x+1];
      let c;
      if(on) c=(l||r)?WHITE:((x&1)?(pb?ORA:GRN):(pb?BLU:VIO));
      else if(l&&r){const q=x-1,pq=pal[x];c=(q&1)?(pq?ORA:GRN):(pq?BLU:VIO)}  // fill between same-colour dots
      else continue;
      const s=2*x+pb;line[s]=c;if(s+1<W)line[s+1]=c;
    }
  }
  function dhrRow(line,y){
    const base=hiresBase(y);
    const bits=new Uint8Array(560);
    for(let c=0;c<40;c++){const a=AUX[base+c],m=MAIN[base+c];
      for(let i=0;i<7;i++){bits[c*14+i]=(a>>i)&1;bits[c*14+7+i]=(m>>i)&1}}
    if(mono){for(let x=0;x<W;x++)line[x]=bits[x]?rgb(0xFFFFFF):BLACK;return}
    for(let x=0;x<W;x+=4){const n=bits[x]|(bits[x+1]<<1)|(bits[x+2]<<2)|(bits[x+3]<<3);const col=LORES[n];for(let k=0;k<4;k++)line[x+k]=col}
  }
}
/*RENDER-END*/

function draw(){render(px,MAIN,AUX,mode,monitor,flash);ctx.putImageData(img,0,0);dirty=false}
setInterval(()=>{flash=!flash;if(mode&1||mode&2)draw()},267);

// ---------------- connection
let ws=null,started=false,soundOn=true,frames=0,lastFpsT=performance.now(),noReconnect=false,caps=true;
function setStatus(ok,t){$('status').className=ok?'ok':'';$('stxt').textContent=t}
function send(bytes){if(ws&&ws.readyState===1)ws.send(bytes)}
function connect(){
  ws=new WebSocket('ws://'+location.hostname+':81/');
  ws.binaryType='arraybuffer';
  ws.onopen=()=>{setStatus(true,'connected');send(new Uint8Array([0x12]));send(new Uint8Array([0x14,soundOn&&started?1:0]));send(new Uint8Array([0x17,caps?1:0]))};
  ws.onclose=()=>{setStatus(false,noReconnect?'another viewer took over':'reconnecting…');ws=null;if(!noReconnect)setTimeout(connect,1500)};
  ws.onerror=()=>{};
  ws.onmessage=e=>{
    const u8=new Uint8Array(e.data);
    switch(u8[0]){
      case 0:RATE=u8[2]|(u8[3]<<8)|(u8[4]<<16);break;
      case 1:{let n=u8[1],p=2;for(let i=0;i<n;i++){const id=u8[p]|(u8[p+1]<<8);p+=2;const solid=id&0x8000,bank=(id>>8)&1,pg=id&255;
        const mem=bank?AUX:MAIN,o=pg*256;if(solid){mem.fill(u8[p],o,o+256);p++}else{mem.set(u8.subarray(p,p+256),o);p+=256}}break}
      case 3:mode=u8[1]|(u8[2]<<8);$('led1').classList.toggle('on',!!(u8[3]&1));$('led2').classList.toggle('on',!!(u8[3]&2));
        draw();send(new Uint8Array([0x13]));frames++;break;
      case 2:playAudio(u8);break;
      case 4:noReconnect=true;setStatus(false,new TextDecoder().decode(u8.subarray(1)));break;
    }
  };
}
setInterval(()=>{const t=performance.now();if(ws&&ws.readyState===1)setStatus(true,Math.round(frames*1000/(t-lastFpsT))+' fps');frames=0;lastFpsT=t},2000);

// ---------------- sound
let ac=null,nextT=0,RATE=22050;
function startAudio(){
  if(!ac){try{if(navigator.audioSession)navigator.audioSession.type='playback'}catch(e){}
    ac=new (window.AudioContext||window.webkitAudioContext)();}
  ac.resume();
}
function playAudio(u8){
  if(!ac||!soundOn)return;
  const n=(u8.length-1)>>1;if(!n)return;
  const dv=new DataView(u8.buffer,u8.byteOffset+1,n*2);
  const buf=ac.createBuffer(1,n,RATE),ch=buf.getChannelData(0);
  for(let i=0;i<n;i++)ch[i]=dv.getInt16(i*2,true)/32768;
  const now=ac.currentTime;
  if(nextT<now+0.02)nextT=now+0.09;
  if(nextT>now+0.45)return;
  const s=ac.createBufferSource();s.buffer=buf;s.connect(ac.destination);s.start(nextT);nextT+=buf.duration;
}
function setSound(on){soundOn=on;$('bSound').classList.toggle('on',on);$('bSound').firstChild.textContent=on?'\u{1F50A}':'\u{1F508}';
  if(on&&started)startAudio();send(new Uint8Array([0x14,on&&started?1:0]))}

// ---------------- keys: Apple key codes (7-bit ASCII); 0x100 open apple, 0x101 solid apple, 0x102 RESET
function key(down,code){const b=new Uint8Array(6);b[0]=0x10;b[1]=down?1:0;b[2]=code&255;b[3]=(code>>8)&255;send(b)}
const NAMED={Enter:13,Backspace:8,Delete:127,Escape:27,Tab:9,ArrowLeft:8,ArrowRight:21,ArrowUp:11,ArrowDown:10};
let padX=127,padY=127,kpad=new Set();
function sendPad(){const b=new Uint8Array([0x16,padX,padY]);send(b)}
function codeFor(e){
  if(e.code==='AltLeft'||e.code==='MetaLeft')return 0x100;
  if(e.code==='AltRight'||e.code==='MetaRight')return 0x101;
  if((e.ctrlKey&&(e.key==='Pause'||e.key==='Cancel'||e.key==='F12')))return 0x102;
  if(NAMED[e.key]!==undefined)return NAMED[e.key];
  if(e.key.length===1){let c=e.key.charCodeAt(0);if(c>=128)return -1;if(e.ctrlKey){c=e.key.toUpperCase().charCodeAt(0);if(c>=64&&c<96)return c-64;return -1}return c}
  return -1;
}
const NUMJOY={Numpad8:[0,-1],Numpad2:[0,1],Numpad4:[-1,0],Numpad6:[1,0],Numpad7:[-1,-1],Numpad9:[1,-1],Numpad1:[-1,1],Numpad3:[1,1]};
function numJoy(){let dx=0,dy=0;for(const k of kpad){dx+=NUMJOY[k][0];dy+=NUMJOY[k][1]}padX=dx<0?0:dx>0?255:127;padY=dy<0?0:dy>0?255:127;sendPad()}
const held=new Map();
document.addEventListener('keydown',e=>{
  if(dlg.open)return;
  if((e.ctrlKey||e.metaKey)&&(e.key==='v'||e.key==='V'))return;
  if($('numJoy').checked){
    if(NUMJOY[e.code]){e.preventDefault();kpad.add(e.code);numJoy();return}
    if(e.code==='Numpad0'||e.code==='Numpad5'){e.preventDefault();if(!e.repeat)key(true,0x100);return}
    if(e.code==='NumpadDecimal'||e.code==='NumpadEnter'){e.preventDefault();if(!e.repeat)key(true,0x101);return}
  }
  const c=codeFor(e);if(c<0)return;
  e.preventDefault();if(!started)begin();
  if(c>=0x100){if(e.repeat||held.has(e.code))return;held.set(e.code,c);key(true,c);return}
  if(!held.has(e.code))held.set(e.code,c);
  key(true,c);   // auto-repeat: every repeat is a new key press, as on a IIe
});
document.addEventListener('keyup',e=>{
  if(NUMJOY[e.code]&&kpad.has(e.code)){kpad.delete(e.code);numJoy();return}
  if($('numJoy').checked){if(e.code==='Numpad0'||e.code==='Numpad5'){key(false,0x100);return}if(e.code==='NumpadDecimal'||e.code==='NumpadEnter'){key(false,0x101);return}}
  const c=held.get(e.code);if(c===undefined)return;e.preventDefault();held.delete(e.code);key(false,c)});
window.addEventListener('blur',()=>{for(const c of held.values())key(false,c);held.clear()});
document.addEventListener('paste',e=>{if(dlg.open)return;const t=e.clipboardData.getData('text');if(t){e.preventDefault();typeText(t)}});
function typeText(t){const enc=new TextEncoder().encode(t.replace(/\r\n/g,'\n'));const b=new Uint8Array(enc.length+1);b[0]=0x11;b.set(enc,1);send(b)}

// ---------------- on-screen Apple IIe keyboard
const KROWS=[
 ['ESC','1','2','3','4','5','6','7','8','9','0','-','=','DELETE'],
 ['TAB','Q','W','E','R','T','Y','U','I','O','P','[',']'],
 ['CTRL','A','S','D','F','G','H','J','K','L',';',"'",'RETURN'],
 ['SHIFT','Z','X','C','V','B','N','M',',','.','/','↑'],
 ['CAPS','OA','SPACE','SA','←','↓','→']];
const SHIFTED={'1':'!','2':'@','3':'#','4':'$','5':'%','6':'^','7':'&','8':'*','9':'(','0':')','-':'_','=':'+','[':'{',']':'}',';':':',"'":'"',',':'<','.':'>','/':'?'};
const SPECIAL={ESC:27,TAB:9,DELETE:127,RETURN:13,SPACE:32,'←':8,'→':21,'↑':11,'↓':10,OA:0x100,SA:0x101};
const latched=new Set();
function vcode(l){
  if(SPECIAL[l]!==undefined)return SPECIAL[l];
  let ch=l;
  if(latched.has('SHIFT'))ch=SHIFTED[l]||l.toUpperCase();
  else if(ch.length===1&&/[A-Z]/.test(ch)&&!caps)ch=ch.toLowerCase();
  let c=ch.charCodeAt(0);
  if(latched.has('CTRL')){const u=l.toUpperCase().charCodeAt(0);if(u>=64&&u<96)c=u-64}
  return c;
}
function buildKbd(){
  const k=$('kbd');
  KROWS.forEach(row=>{const r=document.createElement('div');r.className='r';
    row.forEach(l=>{const b=document.createElement('button');b.className='k';b.textContent=l==='OA'?'Open':l==='SA'?'Solid':l==='DELETE'?'DEL':l==='RETURN'?'RET':l;
      if(l==='SPACE')b.classList.add('w8');else if(['RETURN','DELETE','SHIFT','CTRL','TAB','ESC','CAPS'].includes(l))b.classList.add('w15');
      if(['SHIFT','CTRL','CAPS','OA','SA'].includes(l))b.classList.add('m');
      if(l==='SHIFT'||l==='CTRL'){
        b.onpointerdown=e=>{e.preventDefault();if(!started)begin();if(latched.has(l)){latched.delete(l);b.classList.remove('lat')}else{latched.add(l);b.classList.add('lat')}};
      }else if(l==='CAPS'){
        b.id='kCaps';b.onpointerdown=e=>{e.preventDefault();setCaps(!caps)};
      }else{
        let down=false,code=0;
        const up=()=>{if(!down)return;down=false;b.classList.remove('dn');key(false,code);
          if(code<0x100){latched.clear();document.querySelectorAll('.k.lat').forEach(x=>x.classList.remove('lat'))}};
        b.onpointerdown=e=>{e.preventDefault();if(!started)begin();b.setPointerCapture(e.pointerId);down=true;b.classList.add('dn');code=vcode(l);key(true,code)};
        b.onpointerup=up;b.onpointercancel=up;
      }
      r.appendChild(b)});
    k.appendChild(r)});
}
buildKbd();

// ---------------- on-screen joystick (analog paddles 0/1 + buttons)
const pad=$('pad');
function padMove(e){const r=pad.getBoundingClientRect(),dx=e.clientX-r.left-r.width/2,dy=e.clientY-r.top-r.height/2,lim=r.width*0.42;
  const nx=Math.max(-1,Math.min(1,dx/lim)),ny=Math.max(-1,Math.min(1,dy/lim));
  padX=Math.round((nx+1)*127.5);padY=Math.round((ny+1)*127.5);sendPad();
  const k=r.width*0.3;pad.style.setProperty('--dx',nx*k+'px');pad.style.setProperty('--dy',ny*k+'px')}
pad.onpointerdown=e=>{e.preventDefault();if(!started)begin();pad.setPointerCapture(e.pointerId);padMove(e)};
pad.onpointermove=e=>{if(pad.hasPointerCapture(e.pointerId))padMove(e)};
pad.onpointerup=pad.onpointercancel=()=>{padX=padY=127;sendPad();pad.style.setProperty('--dx','0px');pad.style.setProperty('--dy','0px')};
[['f0',0x100],['f1',0x101]].forEach(([id,c])=>{const f=$(id);
  f.onpointerdown=e=>{e.preventDefault();if(!started)begin();f.setPointerCapture(e.pointerId);f.classList.add('dn');key(true,c)};
  f.onpointerup=f.onpointercancel=()=>{f.classList.remove('dn');key(false,c)}});

// ---------------- toolbar
function pref(k,v){try{if(v===undefined)return localStorage.getItem(k);localStorage.setItem(k,v)}catch(e){return null}}
function toggle(btn,el,disp){const on=el.style.display!==disp;el.style.display=on?disp:'none';btn.classList.toggle('on',on);fit();pref(el.id,on?1:0)}
function setCaps(on){caps=on;$('bCaps').classList.toggle('on',on);const kc=$('kCaps');if(kc)kc.classList.toggle('lat',on);send(new Uint8Array([0x17,on?1:0]));pref('caps',on?1:0)}
const MONS=['color','green','amber','white'];
function setMon(m){monitor=m;$('bMon').textContent=m[0].toUpperCase()+m.slice(1);pref('mon',m);draw()}
$('bSound').onclick=()=>setSound(!soundOn);
$('bKbd').onclick=()=>toggle($('bKbd'),$('kbd'),'block');
$('bJoy').onclick=()=>toggle($('bJoy'),$('joy'),'flex');
$('bCaps').onclick=()=>setCaps(!caps);
$('bMon').onclick=()=>setMon(MONS[(MONS.indexOf(monitor)+1)%MONS.length]);
$('bPaste').onclick=()=>{dlg.showModal();$('txt').focus()};
$('bTypeGo').onclick=()=>{typeText($('txt').value);dlg.close()};
$('bReset').onclick=()=>{key(true,0x102);key(false,0x102)};
$('bFull').onclick=()=>{const d=document.documentElement;if(!document.fullscreenElement&&d.requestFullscreen)d.requestFullscreen().catch(()=>{});else if(document.exitFullscreen)document.exitFullscreen()};
$('numJoy').onchange=()=>pref('numjoy',$('numJoy').checked?1:0);
const dlg=$('dlg');
function begin(){if(started)return;started=true;$('overlay').style.display='none';if(soundOn)startAudio();send(new Uint8Array([0x14,soundOn?1:0]))}
$('overlay').onclick=begin;

// ---------------- layout (4:3 like the real monitor)
function fit(){const st=$('stage'),aw=st.clientWidth-12,ah=st.clientHeight-12;let w=aw,h=w*3/4;if(h>ah){h=ah;w=h*4/3}
  cv.style.width=Math.max(160,w)+'px';cv.style.height=Math.max(120,h)+'px'}
window.addEventListener('resize',fit);
const touch=matchMedia('(pointer:coarse)').matches;
if(pref('kbd')==='1'||(pref('kbd')===null&&touch))toggle($('bKbd'),$('kbd'),'block');
if(pref('joy')==='1')toggle($('bJoy'),$('joy'),'flex');
if(pref('numjoy')==='1')$('numJoy').checked=true;
if(MONS.includes(pref('mon')))monitor=pref('mon');
setMon(monitor);setCaps(pref('caps')!=='0');setSound(true);fit();draw();connect();
</script></body></html>
)HTML";
