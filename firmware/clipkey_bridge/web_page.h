// 보드가 GET / 로 제공하는 송신 페이지. 브라우저만 있으면 어느 기기에서나 쓴다.
// 토큰은 브라우저 localStorage 에만 저장되고, 모든 동작은 기존 /api/v1 을 Bearer 토큰으로 호출한다.
#pragma once
#include <pgmspace.h>

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="ko"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>ClipKey Bridge</title>
<style>
/* iOS Safari 는 입력 요소 글자가 16px 미만이면 포커스 시 자동 확대한다. 입력 요소는 모두 16px 이상으로 둔다. */
:root{--bg:#f5f5f7;--fg:#1d1d1f;--mute:#6e6e73;--card:#fff;--line:#d2d2d7;--acc:#0a84ff;--err:#d70015;--ok:#34c759}
@media(prefers-color-scheme:dark){:root{--bg:#000;--fg:#f5f5f7;--mute:#98989d;--card:#1c1c1e;--line:#3a3a3c}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:16px -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;padding:max(12px,env(safe-area-inset-top)) 12px 24px}
h1{font-size:18px;margin:4px 0 10px;display:flex;justify-content:space-between;align-items:center}
#dev{font-size:13px;color:var(--mute);font-weight:400}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:12px;margin-bottom:12px}
textarea{width:100%;height:38vh;font:16px ui-monospace,Menlo,monospace;border:1px solid var(--line);border-radius:8px;padding:8px;background:var(--bg);color:var(--fg);resize:vertical;tab-size:4}
#info{font-size:13px;color:var(--mute);min-height:18px;margin-top:6px}#info.err{color:var(--err)}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center;margin-top:8px}
label{font-size:14px;display:flex;align-items:center;gap:4px}
input[type=number],input[type=password]{font-size:16px;padding:6px 8px;border:1px solid var(--line);border-radius:8px;background:var(--bg);color:var(--fg)}
input[type=number]{width:70px}select{font-size:16px;padding:6px;border:1px solid var(--line);border-radius:8px;background:var(--bg);color:var(--fg)}input[type=password]{flex:1;min-width:120px}
button{font-size:16px;padding:10px 16px;border-radius:10px;border:1px solid var(--line);background:var(--card);color:var(--fg)}
button.primary{background:var(--acc);color:#fff;border-color:var(--acc)}button:disabled{opacity:.4}
progress{width:100%;height:10px;margin-top:10px}
#status{font-size:14px;margin-top:6px;min-height:18px}
</style></head><body>
<h1>ClipKey Bridge <span id="dev">연결 확인 중…</span></h1>
<div class="card">
  <textarea id="text" placeholder="여기에 붙여넣거나 입력. 영문·숫자·기호, 줄바꿈, Tab, 한글" spellcheck="false" autocapitalize="off" autocorrect="off"></textarea>
  <div id="info">0자</div>
  <div class="row">
    <label><input type="checkbox" id="enter"> 마지막 Enter</label>
    <label><input type="checkbox" id="auto"> 버튼 없이 2초 후 입력</label>
    <label>간격 <input type="number" id="delay" min="10" max="100" step="10" value="20">ms</label>
    <label>한/영 키 <select id="ime"><option value="lang1">Windows 한/영</option><option value="ralt">Windows 오른쪽 Alt</option><option value="capslock">Mac Caps Lock</option><option value="ctrlspace">Mac ⌃Space</option></select></label>
  </div>
  <div class="row">
    <button id="paste">붙여넣기</button>
    <button id="clear">지우기</button>
    <span style="flex:1"></span>
    <button id="cancel" disabled>취소</button>
    <button id="send" class="primary" disabled>보내기</button>
  </div>
  <progress id="bar" value="0" max="1"></progress>
  <div id="status"></div>
</div>
<div class="card">
  <div class="row"><label style="flex:1">토큰 <input type="password" id="token" placeholder="보드 secrets.h 의 CLIPKEY_TOKEN"></label><button id="save">저장</button></div>
</div>
<script>
const $=id=>document.getElementById(id);
const S=JSON.parse(localStorage.getItem('clipkey')||'{}');
$('token').value=S.token||'';$('enter').checked=!!S.enter;$('auto').checked=!!S.auto;$('delay').value=S.delay||20;$('ime').value=S.ime||'lang1';
let job=null,timer=null;
const norm=t=>t.replace(/\r\n/g,'\n').replace(/\r/g,'\n').normalize('NFC');
const bytes=t=>new TextEncoder().encode(t).length;const isHangul=c=>(c>=0xAC00&&c<=0xD7A3)||(c>=0x3131&&c<=0x3163);
// crypto.randomUUID 는 HTTPS 에서만 있다. http 페이지라 getRandomValues 로 v4 UUID 를 만든다.
const uuid=()=>{const b=crypto.getRandomValues(new Uint8Array(16));b[6]=b[6]&15|64;b[8]=b[8]&63|128;const h=[...b].map(x=>x.toString(16).padStart(2,'0')).join('');return h.replace(/^(.{8})(.{4})(.{4})(.{4})(.{12})$/,'$1-$2-$3-$4-$5');};
function problem(t){if(!t)return '텍스트가 비어 있습니다';const b=bytes(t);if(b>4096)return '최대 4096바이트 ('+b+'바이트, 한글은 3바이트)';
  let i=0;for(const ch of t){const c=ch.codePointAt(0);if(!((c>=32&&c<=126)||c===10||c===9||isHangul(c)))return '영문·숫자·기호, 줄바꿈, Tab, 한글만 허용 (위치 '+i+': "'+ch+'")';i++;}return null;}
function validate(){const t=norm($('text').value),p=problem(t);$('info').textContent=p||([...t].length+'자, '+t.split('\n').length+'줄');
  $('info').className=p?'err':'';$('send').disabled=!!p||!!job||!$('token').value;return !p;}
function api(path,opt={}){opt.headers=Object.assign({'Authorization':'Bearer '+$('token').value},opt.headers||{});
  return fetch(path,opt).then(async r=>{const b=await r.text();let j={};try{j=JSON.parse(b)}catch(e){}
    if(!r.ok)throw new Error(r.status===401?'토큰이 틀립니다':(j.error||('HTTP '+r.status)));return j;});}
function saveSettings(){Object.assign(S,{token:$('token').value,enter:$('enter').checked,auto:$('auto').checked,delay:+$('delay').value,ime:$('ime').value});localStorage.setItem('clipkey',JSON.stringify(S));}
function busy(b){['text','paste','clear','enter','auto','delay','ime'].forEach(id=>$(id).disabled=b);$('cancel').disabled=!b;validate();}
function show(j){$('bar').max=Math.max(1,j.totalCharacters);$('bar').value=j.typedCharacters;const n=j.typedCharacters+' / '+j.totalCharacters;
  $('status').textContent={WAITING:$('auto').checked?'2초 뒤 자동 입력 — 멈추려면 취소 또는 보드 BOOT':'보드의 BOOT 버튼을 누르면 입력을 시작합니다 (60초 내)',
    TYPING:'입력 중 '+n,COMPLETED:'완료 '+n,CANCELLED:'취소됨 ('+j.error+') '+n,FAILED:'실패 ('+j.error+') '+n}[j.state]||j.state;}
function finish(){clearInterval(timer);timer=null;job=null;busy(false);device();}
async function poll(){try{const j=await api('/api/v1/jobs/'+job);show(j);if(!['WAITING','TYPING'].includes(j.state))finish();}
  catch(e){$('status').textContent='상태 확인 실패: '+e.message;if(/404|not_found/.test(e.message))finish();}}
async function send(){if(!validate())return;saveSettings();busy(true);$('bar').value=0;$('status').textContent='등록 중…';
  try{const j=await api('/api/v1/type?appendEnter='+$('enter').checked+'&autoStart='+$('auto').checked+'&delayMs='+$('delay').value+'&imeToggle='+$('ime').value,
      {method:'POST',headers:{'Content-Type':'text/plain; charset=utf-8','X-Request-Id':uuid()},body:norm($('text').value)});
    job=j.requestId;show(j);timer=setInterval(poll,500);}
  catch(e){$('status').textContent='전송 실패: '+e.message;busy(false);}}
async function device(){try{const s=await api('/api/v1/status');$('dev').textContent=s.firmwareVersion+' · USB '+(s.usbReady?'연결됨':'미연결')+' · '+s.state;}
  catch(e){$('dev').textContent='연결 안 됨 ('+e.message+')';}}
$('text').addEventListener('input',validate);$('token').addEventListener('input',validate);
$('send').onclick=send;$('cancel').onclick=()=>job&&api('/api/v1/jobs/'+job+'/cancel',{method:'POST'}).then(show).catch(e=>$('status').textContent='취소 실패: '+e.message);
$('clear').onclick=()=>{$('text').value='';validate();};
$('paste').onclick=async()=>{try{$('text').value=norm(await navigator.clipboard.readText());validate();}catch(e){$('status').textContent='클립보드를 읽을 수 없습니다. 입력창을 길게 눌러 붙여넣기 하세요';}};
$('save').onclick=()=>{saveSettings();validate();device();};
validate();if($('token').value)device();
</script></body></html>
)HTML";
