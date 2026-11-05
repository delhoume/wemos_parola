#pragma once
#include <pgmspace.h>

// Single self-contained page: all CSS/JS inline, no external dependencies.
// Speed slider is inverted in the UI: right = faster (device value: lower =
// faster, 10..50).
const char UI_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Wemos Ticker</title>
<style>
:root{--bg:#0f1115;--card:#1a1d24;--fg:#e8eaed;--mut:#8b919c;--acc:#ff3b30;--bd:#2a2e38}
*{box-sizing:border-box}
body{margin:0;min-height:100vh;background:var(--bg);color:var(--fg);font:16px/1.4 -apple-system,system-ui,"Segoe UI",Roboto,sans-serif;display:flex;justify-content:center;padding:20px}
main{width:100%;max-width:480px}
h1{font-size:1.4rem;margin:0 0 16px;display:flex;align-items:center;gap:10px}
h1:before{content:"";width:12px;height:12px;border-radius:50%;background:var(--acc);box-shadow:0 0 10px var(--acc)}
.card{background:var(--card);border:1px solid var(--bd);border-radius:14px;padding:18px;margin-bottom:16px}
label{display:flex;justify-content:space-between;font-size:.85rem;color:var(--mut);text-transform:uppercase;letter-spacing:.06em;margin-bottom:8px}
label b{color:var(--fg)}
.led{background:#000;border:1px solid var(--bd);border-radius:8px;padding:10px 12px;margin-bottom:14px;font:600 1.1rem ui-monospace,Menlo,monospace;color:var(--acc);text-shadow:0 0 6px var(--acc);overflow:hidden;white-space:nowrap;text-overflow:ellipsis;min-height:2.6rem}
input[type=text]{width:100%;padding:12px;border-radius:10px;border:1px solid var(--bd);background:#0f1115;color:var(--fg);font-size:1rem;outline:none}
input[type=text]:focus{border-color:var(--acc)}
input[type=range]{width:100%;accent-color:var(--acc);height:28px;margin:0}
.row{margin-bottom:18px}.row:last-child{margin-bottom:0}
.btns{display:flex;gap:10px;margin-top:12px}
button{flex:1;padding:12px;border:0;border-radius:10px;font-size:1rem;font-weight:600;cursor:pointer;background:var(--acc);color:#fff}
button.sec{background:transparent;border:1px solid var(--bd);color:var(--fg)}
.srow{display:flex;align-items:center;gap:10px}
button.step{flex:none;width:44px;padding:6px 0;font-size:1.2rem}
button:active{transform:scale(.98)}
.info{padding:14px 18px}.info label{margin:0}
#st{text-align:center;color:var(--mut);font-size:.8rem;min-height:1.2em}
</style></head><body><main>
<h1>Wemos Ticker</h1>
<div class="card">
 <label><span>Display text</span><b id="cnt">0/127</b></label>
 <div class="led" id="prev">&nbsp;</div>
 <input type="text" id="text" maxlength="127" placeholder="Type a message…" autocomplete="off">
 <div class="btns"><button id="send">Send</button><button class="sec" id="reset">Reset</button></div>
</div>
<div class="card">
 <div class="row"><label><span>Speed</span></label>
  <div class="srow"><button class="sec step" id="dec">&minus;</button><input type="range" id="speed" min="10" max="50" step="5"><button class="sec step" id="inc">+</button></div></div>
 <div class="row"><label><span>Intensity</span><b id="iv"></b></label>
  <input type="range" id="int" min="0" max="15" step="1"></div>
</div>
<div class="card info"><label><span>Uptime</span><b id="up">–</b></label></div>
<div id="st"></div>
</main>
<script>
const $=id=>document.getElementById(id),st=$("st");
const SMIN=10,SMAX=50;
const toUi=v=>SMAX+SMIN-v,toDev=v=>SMAX+SMIN-v;
function say(m){st.textContent=m}
async function call(p){try{const r=await fetch(p);if(!r.ok)throw 0;say("Saved");return await r.text()}catch(e){say("Connection error")}}
function show(){$("iv").textContent=$("int").value+"/15"}
function cnt(){$("cnt").textContent=$("text").value.length+"/127";$("prev").textContent=$("text").value||"\u00a0"}
let t;const debounce=(k,f)=>()=>{show();clearTimeout(t);t=setTimeout(f,150)};
$("speed").oninput=debounce(0,()=>call("/speed?value="+toDev(+$("speed").value)));
const step=p=>async()=>{const r=await call(p);if(r!==undefined&&!isNaN(+r)){$("speed").value=toUi(+r);show()}};
$("inc").onclick=step("/incspeed");$("dec").onclick=step("/decspeed");
$("int").oninput=debounce(0,()=>call("/intensity?value="+$("int").value));
$("text").oninput=cnt;
$("text").onkeydown=e=>{if(e.key==="Enter")$("send").click()};
$("send").onclick=()=>call("/text?value="+encodeURIComponent($("text").value));
$("reset").onclick=async()=>{const r=await call("/text");if(r!==undefined){$("text").value=r;cnt()}};
async function load(){try{const s=await (await fetch("/state")).json();
 $("text").value=s.text||"";$("speed").value=toUi(s.speed);$("int").value=s.intensity;$("up").textContent=s.uptime;cnt();show();say("")}catch(e){say("Connection error")}}
load();
async function upd(){try{const s=await (await fetch("/state")).json();$("up").textContent=s.uptime}catch(e){}}
setInterval(upd,10000);
</script></body></html>
)HTML";
