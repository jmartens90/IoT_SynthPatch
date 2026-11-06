// WebPage.h - the single-page GUI served at http://synthpatch.local/ (or http://192.168.4.1/).
// Plain HTML + JavaScript, no internet needed (no CDN libraries): the chart is drawn on a canvas.
#pragma once
#include <pgmspace.h>

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SynthPatch prototype</title>
<style>
body{font:15px system-ui,sans-serif;margin:0;background:#15171a;color:#e8e8e8}
header{padding:10px 16px;background:#1f2328;display:flex;justify-content:space-between;align-items:center}
h1{font-size:18px;margin:0} h2{font-size:15px;margin:0 0 8px;color:#9fb3c8}
main{display:grid;gap:12px;padding:12px;grid-template-columns:repeat(auto-fit,minmax(320px,1fr))}
.card{background:#1f2328;border-radius:8px;padding:12px}
.wide{grid-column:1/-1}
.kpis{display:flex;flex-wrap:wrap;gap:16px}
.kpi b{display:block;font-size:22px;font-variant-numeric:tabular-nums} .kpi span{color:#9aa;font-size:12px}
canvas{width:100%;height:260px;display:block}
label{display:grid;grid-template-columns:70px 1fr 70px;align-items:center;gap:8px;margin:4px 0}
input[type=range]{width:100%} select,button,input{font:inherit}
button{background:#2d6cdf;color:#fff;border:0;border-radius:6px;padding:8px 14px}
button:active,button.on{background:#f0a020}
.keys{display:flex;user-select:none;touch-action:none;margin-top:8px}
.k{flex:1;height:90px;background:#eee;border:1px solid #333;border-radius:0 0 4px 4px;position:relative}
.k.b{background:#222;height:56px;flex:.6;margin:0 -1.5%;z-index:1}
.k.on{background:#f0a020}
#ev{font-size:13px;color:#bbb;max-height:120px;overflow:auto}
small{color:#889}
</style></head><body>
<header><h1>SynthPatch prototype</h1><small id="net"></small></header>
<main>
<section class="card wide"><div class="kpis">
 <div class="kpi"><b id="temp">-</b><span>temperature <i id="tsrc"></i></span></div>
 <div class="kpi"><b id="hz">-</b><span>input frequency</span></div>
 <div class="kpi"><b id="note">-</b><span>note / cents off</span></div>
 <div class="kpi"><b id="drift">-</b><span>drift (30 s avg)</span></div>
 <div class="kpi"><b id="volts">-</b><span>ADC input</span></div>
 <div class="kpi"><b id="outs">-</b><span>LFO / ENV / V/oct out</span></div>
</div></section>

<section class="card wide"><h2>History
 <select id="range"><option value="0">live (10 min)</option><option value="1">1 hour</option><option value="24">24 hours</option>
 <option value="168">7 days</option><option value="336">14 days</option><option value="1104">46 days</option></select>
 <select id="series"><option value="drift">drift (cents)</option><option value="hz">frequency (Hz)</option><option value="volts">ADC (V)</option><option value="rh">humidity (%)</option></select>
 <small>orange = temperature, blue = selected, lines = note changes (live)</small> ·
 <a href="/log-full.csv?days=14" style="color:#9fb3c8">download 14 days (CSV)</a> <small id="loginfo"></small></h2>
 <canvas id="chart"></canvas></section>

<section class="card" id="lfo1"><h2>LFO → MCP4725</h2></section>
<section class="card" id="env1"><h2>Envelope → <span id="envout">DAC</span></h2>
 <button id="gate">Hold to gate</button></section>

<section class="card wide"><h2>Keyboard → V/oct (AD5693R) + envelope gate</h2>
 <label style="grid-template-columns:auto auto 1fr"><input type="checkbox" id="tonek" checked> also play the note on the test tone (GPIO 17 → 13) <span></span></label>
 <div class="keys" id="keys"></div>
 <h2 style="margin-top:12px">Note events</h2><div id="ev"></div></section>
</main>
<script>
const $=id=>document.getElementById(id), NAMES=["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"];
const nn=n=>n<0?"-":NAMES[n%12]+(Math.floor(n/12)-1);
const post=(u)=>fetch(u,{method:"POST"});
let H={t:[],temp:[],hz:[],drift:[],volts:[]}, EV=[], evTotal=-1;

// ---- controls built from /api/settings, so new parameters show up by themselves ----
async function buildControls(){
 const s=await (await fetch("/api/settings")).json();
 for(const [id,m] of Object.entries(s)){
  const box=$(id); if(!box) continue;
  if(m.shape!==undefined){
   const l=document.createElement("label");
   l.innerHTML=`shape <select>${["sine","triangle","saw","square"].map(x=>`<option ${x==m.shape?"selected":""}>${x}</option>`).join("")}</select><span></span>`;
   l.querySelector("select").onchange=e=>post(`/api/shape?target=${id}&shape=${e.target.value}`);
   box.insertBefore(l,box.querySelector("button"));
  }
  for(const p of m.params){
   const l=document.createElement("label"), log=p.name=="rate"||p.unit=="s";
   const toS=v=>log?Math.log(v):v, fromS=v=>log?Math.exp(v):v;
   l.innerHTML=`${p.name}<input type="range" min="${toS(p.min)}" max="${toS(p.max)}" step="any" value="${toS(p.value)}"><span></span>`;
   const r=l.querySelector("input"), out=l.querySelector("span");
   const show=()=>out.textContent=(+fromS(+r.value)).toPrecision(3)+" "+p.unit; show();
   let timer; r.oninput=()=>{show();clearTimeout(timer);timer=setTimeout(()=>post(`/api/set?target=${id}&name=${p.name}&value=${fromS(+r.value)}`),60);};
   box.insertBefore(l,box.querySelector("button"));
  }
 }
}

// ---- gate button and keyboard ----
const hold=(el,on,off)=>{el.onpointerdown=e=>{el.setPointerCapture(e.pointerId);el.classList.add("on");on();};
 el.onpointerup=el.onpointercancel=()=>{el.classList.remove("on");off();};};
hold($("gate"),()=>post("/api/gate?on=1"),()=>post("/api/gate?on=0"));
for(let n=48;n<=72;n++){const k=document.createElement("div");k.className="k"+([1,3,6,8,10].includes(n%12)?" b":"");k.title=nn(n);
 hold(k,()=>post(`/api/note?n=${n}&on=1&tone=${$("tonek").checked?1:0}`),()=>post(`/api/note?n=${n}&on=0`));$("keys").appendChild(k);}

// ---- chart ----
function draw(){
 const c=$("chart"),g=c.getContext("2d"),W=c.width=c.clientWidth*devicePixelRatio,Ht=c.height=260*devicePixelRatio;
 g.fillStyle="#111";g.fillRect(0,0,W,Ht);if(H.t.length<2)return;
 const t0=H.t[0],t1=H.t[H.t.length-1]||1,X=t=>40*devicePixelRatio+(t-t0)/Math.max(1,t1-t0)*(W-80*devicePixelRatio);
 const line=(arr,col,side)=>{const v=arr.filter(x=>x!==null&&!isNaN(x));if(!v.length)return;
  let lo=Math.min(...v),hi=Math.max(...v);if(hi-lo<1e-3){lo-=.5;hi+=.5;}const Y=y=>Ht-10-(y-lo)/(hi-lo)*(Ht-20);
  g.strokeStyle=col;g.lineWidth=2*devicePixelRatio;g.beginPath();let pen=false;
  arr.forEach((y,i)=>{if(y===null||isNaN(y)){pen=false;return;}pen?g.lineTo(X(H.t[i]),Y(y)):g.moveTo(X(H.t[i]),Y(y));pen=true;});g.stroke();
  g.fillStyle=col;g.font=`${11*devicePixelRatio}px system-ui`;g.textAlign=side;const x=side=="left"?2:W-2;
  g.fillText(hi.toPrecision(4),x,14*devicePixelRatio);g.fillText(lo.toPrecision(4),x,Ht-4);};
 g.strokeStyle="#555";if(live())EV.forEach(e=>{if(e.t>=t0){g.beginPath();g.moveTo(X(e.t),0);g.lineTo(X(e.t),Ht);g.stroke();}});
 line(H.temp,"#f0a020","left");if(H[$("series").value])line(H[$("series").value],"#4ea1ff","right");
 // time axis: real dates for the flash log, seconds since boot for the live view
 const fmt=t=>t>1e9?new Date(t*1000).toLocaleString([], {month:"numeric",day:"numeric",hour:"2-digit",minute:"2-digit"}):t+" s";
 g.fillStyle="#889";g.font=`${11*devicePixelRatio}px system-ui`;g.textAlign="left";g.fillText(fmt(t0),40*devicePixelRatio,Ht-4);
 g.textAlign="right";g.fillText(fmt(t1),W-40*devicePixelRatio,Ht-4);
}
$("series").onchange=draw;addEventListener("resize",draw);
const live=()=>$("range").value=="0";
// Range picker: live = last 10 min from RAM (1 per s), the others = flash log (1 per 10 s, averaged to 800 points)
async function loadRange(){
 H=await (await fetch(live()?"/api/history":`/api/log?hours=${$("range").value}&points=800`)).json();
 draw();
}
$("range").onchange=loadRange;

// ---- live values once a second ----
async function tick(){
 try{
  const s=await (await fetch("/api/live")).json();
  $("temp").textContent=s.temp_c==null?"-":s.temp_c.toFixed(2)+" °C"+(s.rh==null?"":` · ${s.rh.toFixed(0)}%`);
  $("tsrc").textContent=s.temp_sensor?"("+s.temp_sensor+")":"";
  $("hz").textContent=s.hz>0?s.hz.toFixed(2)+" Hz":"no input";
  $("note").textContent=s.hz>0?`${nn(s.note)} ${s.cents_off>=0?"+":""}${s.cents_off.toFixed(1)}¢`:"-";
  $("drift").textContent=`${s.drift.toFixed(1)}¢ (${s.drift_per_min>=0?"+":""}${s.drift_per_min.toFixed(1)}/min)`;
  $("volts").textContent=s.volts.toFixed(3)+" V";
  $("outs").textContent=`${s.lfo_v.toFixed(2)} / ${s.env_v.toFixed(2)} / ${s.voct_v.toFixed(3)} V`;
  $("envout").textContent=s.env_out;
  $("net").textContent=`${s.wifi} · ${s.rssi} dBm · up ${s.uptime}s`+(s.mqtt?" · MQTT ok":"");
  $("loginfo").textContent=s.time?`· ${s.log_count} samples stored`:"· log waits for NTP time (home WiFi)";
  if(!live()){if(s.uptime%60==0)loadRange();}  // stored view: refresh once a minute
  else if(!H.t.length||s.uptime>H.t[H.t.length-1]){for(const k of ["temp","hz","drift","volts","rh"])H[k].push(s[{temp:"temp_c",hz:"hz",drift:"drift",volts:"volts",rh:"rh"}[k]]);H.t.push(s.uptime);
   if(H.t.length>600)for(const k in H)H[k].shift();}
  if(s.events_total!==evTotal){evTotal=s.events_total;await loadEvents();}
  draw();
 }catch(e){$("net").textContent="offline";}
}
async function loadEvents(){EV=(await (await fetch("/api/events")).json());
 $("ev").innerHTML=EV.slice().reverse().map(e=>`${e.t}s: ${nn(e.from)} → <b>${nn(e.to)}</b>`).join("<br>");}
(async()=>{await buildControls();await loadRange();await loadEvents();tick();setInterval(tick,1000);})();
</script></body></html>)HTML";
