// CA53Y Comms — phone portal (served from the ESP32 at 192.168.4.1)
#pragma once
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0F2A30">
<title>CA53Y Comms</title>
<style>
:root{--lac:#0F2A30;--lac2:#17393F;--line:#2D545A;--gold:#CFA84E;--champ:#EAD9A8;--ivory:#F2EBD9;--mist:#A3B8B2;--verd:#86C2AE;--rose:#E39098;--ink:#1A2A2C;
--disp:Didot,"Bodoni 72","Bodoni MT",Georgia,serif;--body:"Avenir Next",Avenir,"Segoe UI",Roboto,system-ui,sans-serif}
*{box-sizing:border-box}
html,body{margin:0;min-height:100%;background:var(--lac);color:var(--ivory);font:16px/1.45 var(--body);-webkit-text-size-adjust:100%}
body{padding:env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left)}
[hidden]{display:none!important}
button,input,textarea{font:inherit;color:inherit}
:focus-visible{outline:2px solid var(--gold);outline-offset:2px}
.app{max-width:560px;margin:0 auto;min-height:100vh;display:flex;flex-direction:column}
.plate{position:relative;overflow:hidden;padding:30px 20px 20px;text-align:center;border-bottom:1px solid var(--line)}
.plate::before{content:"";position:absolute;left:50%;top:100%;width:680px;height:680px;transform:translate(-50%,-50%);
background:repeating-conic-gradient(from 0deg,rgba(207,168,78,.20) 0 3deg,transparent 3deg 10deg);
-webkit-mask:radial-gradient(closest-side,transparent 16%,#000 18%,#000 55%,transparent 100%);
mask:radial-gradient(closest-side,transparent 16%,#000 18%,#000 55%,transparent 100%);pointer-events:none}
.plate>*{position:relative}
.plate h1{margin:0;font:400 clamp(30px,9vw,42px)/1.05 var(--disp);color:var(--champ);letter-spacing:.01em}
.code{display:inline-block;margin-top:10px;padding:3px 14px;border:1px solid var(--gold);border-left-width:4px;border-right-width:4px;color:var(--gold);font:400 19px/1.3 var(--disp);letter-spacing:.2em;background:var(--lac)}
.nick{margin:10px 0 0;color:var(--mist);font-size:15px}
.gear{position:absolute;top:12px;right:12px;background:none;border:0;color:var(--mist);padding:8px;font-size:14px;cursor:pointer}
h2{font:400 22px/1.2 var(--disp);color:var(--champ);margin:26px 20px 8px;display:flex;align-items:baseline;justify-content:space-between}
h2 .btn{font:15px/1 var(--body)}
.row{display:flex;align-items:center;gap:12px;width:100%;padding:13px 20px;border:0;border-bottom:1px solid var(--line);background:none;text-align:left;cursor:pointer}
.row:first-child{border-top:1px solid var(--line)}
.who{flex:1;min-width:0}
.who b{display:block;font-weight:600;font-size:17px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.who span{display:block;color:var(--mist);font-size:13px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.lock{color:var(--gold);margin-left:6px;vertical-align:-1px}
.badge{min-width:24px;height:24px;padding:0 7px;border-radius:12px;background:var(--gold);color:var(--ink);font-weight:700;font-size:13px;display:grid;place-items:center}
.empty{color:var(--mist);margin:4px 20px 0;max-width:46ch;font-size:15px}
.btn{background:none;border:1px solid var(--gold);color:var(--champ);padding:9px 16px;border-radius:2px;cursor:pointer}
.btn.solid{background:var(--gold);color:var(--ink);font-weight:600}
.btn.quiet{border-color:var(--line);color:var(--mist)}
.btn.danger{border-color:var(--rose);color:var(--rose)}
.thead{position:sticky;top:0;z-index:2;background:var(--lac);border-bottom:1px solid var(--line);padding:10px 20px 12px}
.back{background:none;border:0;color:var(--gold);padding:6px 0;cursor:pointer}
.thead h3{margin:2px 0 0;font:400 26px/1.15 var(--disp);color:var(--champ);display:flex;justify-content:space-between;align-items:baseline;gap:10px}
.thead h3 span{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.mode{margin:4px 0 0;font-size:13px;color:var(--mist)}
.mode.priv{color:var(--gold)}
.msgs{flex:1;padding:16px 16px 8px;display:flex;flex-direction:column;gap:6px}
.b{max-width:82%;padding:9px 13px;border-radius:14px;white-space:pre-wrap;overflow-wrap:anywhere}
.b.in{align-self:flex-start;background:var(--lac2);border:1px solid var(--line);border-bottom-left-radius:3px}
.b.out{align-self:flex-end;background:var(--champ);color:var(--ink);border-bottom-right-radius:3px}
.b.locked{font-style:italic;color:var(--mist)}
.meta{font-size:12px;color:var(--mist);margin:-2px 4px 6px}
.meta.r{align-self:flex-end}.meta.l{align-self:flex-start}
.meta.ok{color:var(--verd)}.meta.bad{color:var(--rose);cursor:pointer;text-decoration:underline}
.compose{position:sticky;bottom:0;background:var(--lac);border-top:1px solid var(--line);padding:10px 12px calc(10px + env(safe-area-inset-bottom));display:flex;gap:8px;align-items:flex-end}
textarea,input[type=text],input[type=password]{width:100%;background:var(--lac2);border:1px solid var(--line);border-radius:2px;padding:10px 12px;font-size:16px}
textarea{resize:none;min-height:44px;max-height:120px}
.count{font-size:12px;color:var(--mist);text-align:right;margin:0 16px 4px}
.sheet{position:fixed;inset:0;z-index:10;background:rgba(6,20,23,.74);display:flex;align-items:flex-end;justify-content:center}
.panel{width:100%;max-width:560px;max-height:92vh;overflow:auto;background:var(--lac2);border-top:3px double var(--gold);padding:20px 20px calc(24px + env(safe-area-inset-bottom))}
.panel h3{margin:0 0 14px;font:400 26px/1.2 var(--disp);color:var(--champ)}
label{display:block;margin:14px 0 6px;font-size:14px;color:var(--champ)}
.hint{font-size:13px;color:var(--mist);margin:6px 0 0}
.check{display:flex;gap:10px;align-items:center;margin-top:16px;color:var(--ivory);font-size:15px}
.check input{width:20px;height:20px;accent-color:var(--gold)}
.actions{display:flex;flex-wrap:wrap;gap:10px;margin-top:20px}
.setup{padding:8px 20px 40px}
.setup p{color:var(--mist);max-width:46ch}
.banner{background:var(--rose);color:var(--ink);padding:8px 16px;font-size:14px;text-align:center}
.toast{position:fixed;left:50%;bottom:90px;transform:translateX(-50%);background:var(--champ);color:var(--ink);padding:10px 16px;border-radius:2px;max-width:90vw;z-index:20;font-size:15px}
</style></head><body>
<div class="app">
 <div class="banner" id="offline" hidden>Lost the connection. Rejoin this device's Wi-Fi network, then reload.</div>
 <header class="plate">
  <button class="gear" id="openSettings" hidden>Settings</button>
  <h1>CA53Ycomms</h1>
  <div class="code" id="idcode">&nbsp;</div>
  <p class="nick" id="nick"></p>
 </header>

 <section id="setup" class="setup" hidden>
  <h2 style="margin-left:0">Set up this device</h2>
  <p>This Wi-Fi network is open until you set a password. Once it has one, only someone with the password can connect, and only one phone at a time.</p>
  <label for="uNick">Your name</label>
  <input type="text" id="uNick" maxlength="23" autocomplete="off" placeholder="Shown to people you message">
  <label for="uPw">Wi-Fi password for this device</label>
  <input type="password" id="uPw" maxlength="63" autocomplete="new-password">
  <label for="uPw2">Type it again</label>
  <input type="password" id="uPw2" maxlength="63" autocomplete="new-password">
  <p class="hint">8 to 63 characters. Forgot it later? Hold the BOOT button for 5 seconds to erase the device.</p>
  <div class="actions"><button class="btn solid" id="doSetup">Save and restart</button></div>
  <p id="setupDone" hidden></p>
 </section>

 <main id="list" hidden>
  <h2>Contacts <button class="btn quiet" id="openAdd">Add contact</button></h2>
  <div id="contacts"></div>
  <h2>Nearby</h2>
  <div id="nearby"></div>
 </main>

 <section id="thread" hidden style="display:flex;flex-direction:column;flex:1">
  <div class="thead">
   <button class="back" id="back">&lsaquo; Contacts</button>
   <h3><span id="tName"></span><button class="btn quiet" id="tEdit" style="font:14px/1 var(--body);padding:7px 12px">Edit</button></h3>
   <p class="mode" id="tMode"></p>
  </div>
  <div class="msgs" id="msgs"></div>
  <div class="count" id="count" hidden></div>
  <div class="compose">
   <textarea id="text" rows="1" placeholder="Message"></textarea>
   <button class="btn solid" id="send">Send</button>
  </div>
 </section>
</div>

<div class="sheet" id="cSheet" hidden><div class="panel">
 <h3 id="cTitle">Add contact</h3>
 <div id="cNearbyWrap"><p class="hint" style="margin-top:0">Pick a device in range, or type its ID below.</p><div id="cNearby" style="margin:10px -20px 0"></div></div>
 <label for="cId">Device ID</label>
 <input type="text" id="cId" autocapitalize="off" autocorrect="off" spellcheck="false" placeholder="1234ysA or CA53Ycomms-1234ysA">
 <label for="cNick">Name</label>
 <input type="text" id="cNick" maxlength="23" placeholder="Optional">
 <label for="cPin">Shared PIN</label>
 <input type="password" id="cPin" maxlength="32" autocomplete="off">
 <p class="hint" id="cPinHint">Optional. If you both enter the same PIN for each other, your messages are encrypted and other devices can't read them. A passphrase is much stronger than 4 digits.</p>
 <div class="actions">
  <button class="btn solid" id="cSave">Save contact</button>
  <button class="btn quiet" id="cClearPin" hidden>Remove PIN</button>
  <button class="btn danger" id="cDel" hidden>Delete contact</button>
  <button class="btn quiet" id="cCancel">Cancel</button>
 </div>
</div></div>

<div class="sheet" id="sSheet" hidden><div class="panel">
 <h3>Settings</h3>
 <p class="hint" style="margin-top:0">Your device ID is <b id="sId" style="color:var(--champ)"></b>. Give it to people who want to add you.</p>
 <label for="sNick">Your name</label>
 <input type="text" id="sNick" maxlength="23">
 <label class="check"><input type="checkbox" id="sDisc"> Show this device in other people's Nearby list</label>
 <label for="sPw">New Wi-Fi password</label>
 <input type="password" id="sPw" maxlength="63" autocomplete="new-password" placeholder="Leave blank to keep the current one">
 <p class="hint">Changing the password restarts the device. Messages are kept only until it restarts.</p>
 <div class="actions"><button class="btn solid" id="sSave">Save settings</button><button class="btn quiet" id="sCancel">Cancel</button></div>
</div></div>
<div class="toast" id="toast" hidden></div>

<script>
const $=s=>document.querySelector(s);
const LOCK='<svg class="lock" width="12" height="13" viewBox="0 0 12 13" aria-label="PIN set"><path d="M3 6V4a3 3 0 0 1 6 0v2" fill="none" stroke="currentColor" stroke-width="1.5"/><rect x="1.5" y="6" width="9" height="6.5" rx="1" fill="currentColor"/></svg>';
const PRE='CA53Ycomms-';
let S={boot:0,seq:0,me:null,setup:true,contacts:[],nearby:[],msgs:new Map(),open:null,busy:false,sig:'',editing:null,readSent:0};
const lc=s=>(s||'').toLowerCase();
const bytes=s=>new TextEncoder().encode(s).length;
function el(t,c,x){const e=document.createElement(t);if(c)e.className=c;if(x!=null)e.textContent=x;return e}
function toast(m){const t=$('#toast');t.textContent=m;t.hidden=false;clearTimeout(t._h);t._h=setTimeout(()=>t.hidden=true,3200)}
async function api(p,d){const r=await fetch(p,{method:'POST',body:new URLSearchParams(d||{})});const t=await r.text();if(!r.ok)throw new Error(t||('Error '+r.status));return t}
function contact(id){return S.contacts.find(c=>lc(c.id)===lc(id))}
function label(c){return c.nick||c.id}
function signal(r){return r>-60?'Strong signal':r>-75?'Good signal':'Weak signal'}
function ago(ms){const s=Math.round(ms/1000);return s<60?'just now':s<3600?Math.round(s/60)+' min ago':Math.round(s/3600)+' h ago'}
function tm(ts){return new Date(ts).toLocaleTimeString([],{hour:'numeric',minute:'2-digit'})}

async function poll(){
 if(S.busy)return;S.busy=true;
 try{
  const r=await fetch('/api/state?since='+S.seq,{cache:'no-store'});const j=await r.json();
  if(S.boot&&j.boot!==S.boot){S.boot=j.boot;S.seq=0;S.msgs.clear();S.busy=false;return poll()}
  S.boot=j.boot;S.me=j.me;S.setup=j.setup;S.contacts=j.contacts;S.nearby=j.nearby;
  const now=Date.now();
  for(const m of j.msgs){const o=S.msgs.get(m.u);m.ts=o?o.ts:now-m.age;S.msgs.set(m.u,m)}
  S.seq=j.seq;$('#offline').hidden=true;render();
 }catch(e){$('#offline').hidden=false}
 S.busy=false;
}

function render(){
 if(!S.me)return;
 $('#idcode').textContent=S.me.id.slice(PRE.length);
 $('#nick').textContent=S.me.nick||(S.setup?'No name set':'');
 $('#setup').hidden=S.setup;$('#openSettings').hidden=!S.setup;
 if(!S.setup){$('#list').hidden=true;$('#thread').hidden=true;return}
 if(S.open&&!contact(S.open))S.open=null;
 $('#list').hidden=!!S.open;$('#thread').hidden=!S.open;
 S.open?renderThread():renderList();
 if(!$('#cSheet').hidden&&!S.editing)renderAddNearby();
}

function lastMsg(id){let l=null;for(const m of S.msgs.values())if(lc(m.p)===lc(id)&&(!l||m.u>l.u))l=m;return l}

function renderList(){
 const box=$('#contacts');box.textContent='';
 if(!S.contacts.length)box.append(el('p','empty','No contacts yet. Add someone from Nearby below, or tap Add contact and type their device ID.'));
 for(const c of S.contacts){
  const r=el('button','row');r.onclick=()=>openThread(c.id);
  const w=el('div','who');const b=el('b',null,label(c));if(c.pin)b.insertAdjacentHTML('beforeend',LOCK);
  const l=lastMsg(c.id);
  const sub=c.new&&!l?'New: sent you a message':l?(l.d?'You: ':'')+(l.st==11?'Locked message':l.x):(c.nick?c.id:'No messages yet');
  w.append(b,el('span',null,sub));r.append(w);
  if(c.unread)r.append(el('span','badge',c.unread));
  box.append(r);
 }
 const nb=$('#nearby');nb.textContent='';
 const list=S.nearby.filter(n=>!contact(n.id));
 if(!list.length)nb.append(el('p','empty','No other devices in range right now. Devices show up here when they are switched on and set to be visible.'));
 for(const n of list){
  const r=el('button','row');r.onclick=()=>openAdd(n.id,n.nick);
  const w=el('div','who');w.append(el('b',null,n.nick||n.id),el('span',null,(n.nick?n.id+', ':'')+signal(n.rssi).toLowerCase()));
  r.append(w,el('span','btn quiet','Add'));nb.append(r);
 }
}

function openThread(id){S.open=id;S.sig='';S.readSent=0;render();window.scrollTo(0,document.body.scrollHeight);$('#text').focus()}

function renderThread(){
 const c=contact(S.open);
 $('#tName').textContent=label(c);
 const m=$('#tMode');
 if(c.pin){m.className='mode priv';m.textContent='Private. Messages are encrypted with the PIN you share.'}
 else{m.className='mode';m.textContent='Open. Any CA53Y device in range could read these. Tap Edit to add a shared PIN.'}
 if(c.unread&&S.readSent!==c.unread){S.readSent=c.unread;api('/api/read',{id:c.id}).catch(()=>{})}
 const list=[...S.msgs.values()].filter(x=>lc(x.p)===lc(c.id)).sort((a,b)=>a.u-b.u);
 const sig=list.map(x=>x.u+':'+x.st).join(',');
 if(sig===S.sig)return;S.sig=sig;
 const box=$('#msgs');const nearBottom=window.innerHeight+window.scrollY>=document.body.scrollHeight-80;
 box.textContent='';
 if(!list.length)box.append(el('p','empty','No messages yet. Anything you send goes straight to their device over ESP-NOW.'));
 for(const x of list){
  if(x.st==11){box.append(el('div','b in locked','Locked message. Their PIN for you doesn\'t match yours. Ask them which PIN they set, then tap Edit.'));box.append(el('div','meta l',tm(x.ts)));continue}
  box.append(el('div','b '+(x.d?'out':'in'),x.x));
  let t=tm(x.ts),cls='meta '+(x.d?'r':'l');
  if(x.d){
   if(x.st==0)t+=', sending';
   else if(x.st==1){t+=', delivered';cls+=' ok'}
   else if(x.st==3){t+=', delivered but they can\'t read it. Your PINs don\'t match.';cls+=' bad'}
   else if(x.st==2){t+=', not delivered. Tap to send again.';cls+=' bad'}
  }
  const md=el('div',cls,t);
  if(x.d&&x.st==2)md.onclick=()=>sendText(x.x);
  box.append(md);
 }
 if(nearBottom)window.scrollTo(0,document.body.scrollHeight);
}

async function sendText(t){
 try{await api('/api/send',{to:S.open,text:t});poll()}catch(e){toast(e.message)}
}
function updateCount(){const n=bytes($('#text').value),c=$('#count');c.hidden=n<120;c.textContent=n+' of 160';c.style.color=n>160?'var(--rose)':''}
$('#text').addEventListener('input',e=>{const t=e.target;t.style.height='auto';t.style.height=Math.min(t.scrollHeight,120)+'px';updateCount()});
$('#text').addEventListener('keydown',e=>{if(e.key==='Enter'&&!e.shiftKey&&!('ontouchstart' in window)){e.preventDefault();$('#send').click()}});
$('#send').onclick=()=>{const t=$('#text').value.trim();if(!t)return;if(bytes(t)>160){toast('Messages can be up to 160 characters.');return}
 $('#text').value='';$('#text').style.height='auto';updateCount();sendText(t)};
$('#back').onclick=()=>{S.open=null;render();window.scrollTo(0,0)};

function renderAddNearby(){
 const box=$('#cNearby');box.textContent='';
 const list=S.nearby.filter(n=>!contact(n.id));
 $('#cNearbyWrap').hidden=!list.length;
 for(const n of list){const r=el('button','row');r.onclick=()=>{$('#cId').value=n.id;$('#cNick').value=n.nick||'';$('#cPin').focus()};
  const w=el('div','who');w.append(el('b',null,n.nick||n.id),el('span',null,n.id));r.append(w);box.append(r)}
}
function openAdd(id,nick){
 S.editing=null;$('#cTitle').textContent='Add contact';$('#cId').readOnly=false;
 $('#cId').value=id||'';$('#cNick').value=nick||'';$('#cPin').value='';$('#cPin').placeholder='';
 $('#cDel').hidden=true;$('#cClearPin').hidden=true;$('#cSave').textContent='Save contact';
 renderAddNearby();$('#cSheet').hidden=false;
}
function openEdit(){
 const c=contact(S.open);S.editing=c.id;
 $('#cTitle').textContent='Edit contact';$('#cNearbyWrap').hidden=true;
 $('#cId').value=c.id;$('#cId').readOnly=true;$('#cNick').value=c.nick||'';$('#cPin').value='';
 $('#cPin').placeholder=c.pin?'PIN is set. Type a new one to change it.':'';
 $('#cClearPin').hidden=!c.pin;$('#cDel').hidden=false;$('#cSave').textContent='Save changes';
 $('#cSheet').hidden=false;
}
$('#openAdd').onclick=()=>openAdd();
$('#tEdit').onclick=openEdit;
$('#cCancel').onclick=()=>{$('#cSheet').hidden=true;S.editing=null};
async function saveContact(mode){
 const id=$('#cId').value.trim(),pin=$('#cPin').value;
 if(!id){toast('Enter the device ID.');return}
 if(mode==='set'&&pin&&(pin.length<4||pin.length>32)){toast('PINs are 4 to 32 characters.');return}
 try{await api('/api/contact',{id,nick:$('#cNick').value.trim(),pin,pinmode:mode==='clear'?'clear':(pin?'set':'keep')});
  $('#cSheet').hidden=true;const was=S.editing;S.editing=null;
  toast(mode==='clear'?'PIN removed':was?'Changes saved':'Contact saved');S.sig='';poll()}catch(e){toast(e.message)}
}
$('#cSave').onclick=()=>saveContact('set');
$('#cClearPin').onclick=()=>saveContact('clear');
$('#cDel').onclick=async()=>{if(!confirm('Delete this contact and its PIN?'))return;
 try{await api('/api/contact/del',{id:S.editing});$('#cSheet').hidden=true;S.editing=null;S.open=null;toast('Contact deleted');poll()}catch(e){toast(e.message)}};

$('#openSettings').onclick=()=>{$('#sId').textContent=S.me.id;$('#sNick').value=S.me.nick;$('#sDisc').checked=S.me.disc;$('#sPw').value='';$('#sSheet').hidden=false};
$('#sCancel').onclick=()=>$('#sSheet').hidden=true;
$('#sSave').onclick=async()=>{const pw=$('#sPw').value;
 if(pw&&(pw.length<8||pw.length>63)){toast('Passwords are 8 to 63 characters.');return}
 try{await api('/api/settings',{nick:$('#sNick').value.trim(),disc:$('#sDisc').checked?1:0,pw});$('#sSheet').hidden=true;
  toast(pw?'Password changed. The device is restarting. Rejoin with the new password.':'Settings saved');poll()}catch(e){toast(e.message)}};

$('#doSetup').onclick=async()=>{const pw=$('#uPw').value;
 if(pw.length<8||pw.length>63){toast('Passwords are 8 to 63 characters.');return}
 if(pw!==$('#uPw2').value){toast('The two passwords don\'t match.');return}
 try{await api('/api/setup',{nick:$('#uNick').value.trim(),pw});
  const d=$('#setupDone');d.hidden=false;d.style.color='var(--champ)';
  d.textContent='Saved. The device is restarting. In Wi-Fi settings, join '+S.me.id+' with your new password, then open 192.168.4.1.';
  $('#doSetup').disabled=true}catch(e){toast(e.message)}};

poll();setInterval(poll,1200);
</script></body></html>)HTML";
