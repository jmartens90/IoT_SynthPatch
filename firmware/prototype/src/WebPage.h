// WebPage.h - the single-page GUI served at http://synthpatch.local/ (or http://192.168.4.1/).
// Plain HTML + JavaScript, no internet needed (no CDN libraries): knobs are SVG, charts a canvas.
//
// Layout, top to bottom:
//   header   display with the current patch (A1..D8 + name, * = edited), network status
//   patches  bank A..D, slots 1..8, name field, SAVE
//   modules  LFO (scope + shape + RATE/DEPTH/OFFSET), ENVELOPE (graph + A/D/S/R/PEAK + gate),
//            keyboard -> V/oct
//   monitor  live values, history chart (live .. 46 days), note events  (the IoT course part)
#pragma once
#include <pgmspace.h>

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SynthPatch</title>
<style>
:root{--panel:#24272c;--face:#2e3238;--line:#3b4048;--text:#e6e6e6;--dim:#8a929c;--accent:#f0a020;--blue:#4ea1ff;--red:#ff4d4d}
*{box-sizing:border-box}
body{font:14px system-ui,sans-serif;margin:0;background:#16181b;color:var(--text)}
header{display:flex;gap:16px;align-items:center;justify-content:space-between;padding:10px 16px;background:#0f1012;border-bottom:1px solid var(--line)}
.logo{font-weight:800;letter-spacing:.25em;font-size:15px}
.lcd{font:600 18px ui-monospace,Menlo,Consolas,monospace;background:#0b1a10;color:#7dff9a;padding:6px 14px;border-radius:4px;
 border:1px solid #1d3a25;box-shadow:inset 0 0 12px #000;min-width:260px;text-shadow:0 0 6px #3f9f55}
.lcd small{color:#4f9c62;font-size:12px;margin-left:8px}
#net{color:var(--dim);font-size:12px}
main{padding:12px;display:grid;gap:12px}
.rack{display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(340px,1fr))}
.mod{background:linear-gradient(#2a2e34,#23262b);border:1px solid var(--line);border-radius:10px;padding:12px 14px;
 box-shadow:0 2px 8px #0008}
.mod h2{margin:0 0 10px;font-size:12px;letter-spacing:.2em;color:var(--dim);display:flex;justify-content:space-between;align-items:center}
.mod h2 b{color:var(--text)}
.wide{grid-column:1/-1}
#scope,#adsr{height:130px} #chart{height:240px}
canvas{display:block;width:100%;background:#0d0e10;border-radius:6px;border:1px solid #000}
.knobs{display:flex;flex-wrap:wrap;justify-content:space-around;gap:6px;margin-top:10px}
.knob{display:flex;flex-direction:column;align-items:center;width:74px;user-select:none;touch-action:none;cursor:ns-resize}
.knob svg{width:64px;height:64px}
.knob .lbl{font-size:11px;letter-spacing:.12em;color:var(--dim);margin-top:2px}
.knob .val{font:12px ui-monospace,Menlo,Consolas,monospace;color:var(--accent)}
.row{display:flex;gap:6px;align-items:center;flex-wrap:wrap}
button{font:inherit;background:var(--face);color:var(--text);border:1px solid #111;border-radius:5px;padding:6px 10px;cursor:pointer;
 box-shadow:0 2px 0 #0009}
button:active{transform:translateY(1px);box-shadow:none}
button.on{background:var(--accent);color:#111}
button.save{background:#b8321f;color:#fff;font-weight:700;letter-spacing:.1em}
.shapes button{padding:4px 8px} .shapes svg{width:28px;height:14px;display:block}
.led{display:inline-block;width:9px;height:9px;border-radius:50%;background:#401010;margin-right:5px;vertical-align:middle}
.led.on{background:var(--red);box-shadow:0 0 8px var(--red)}
.led.g.on{background:#59ff7a;box-shadow:0 0 8px #59ff7a}
.slots button{width:42px;position:relative}
.slots button.full::after{content:"";position:absolute;right:4px;top:4px;width:5px;height:5px;border-radius:50%;background:#59ff7a}
.slots button.cur{outline:2px solid var(--accent)}
input[type=text]{font:inherit;background:#111;color:var(--text);border:1px solid var(--line);border-radius:5px;padding:6px 8px;width:170px}
select{font:inherit;background:#111;color:var(--text);border:1px solid var(--line);border-radius:5px;padding:3px}
.steps{display:grid;grid-template-columns:repeat(16,1fr);gap:4px;margin-top:10px}
.st{display:flex;flex-direction:column;align-items:center;gap:4px;padding:6px 0;border-radius:6px;background:#1b1d21;border:1px solid #111;
 user-select:none;touch-action:none}
.st.sel{outline:2px solid var(--blue)} .st.out{opacity:.3}
.st .nt{font:600 13px ui-monospace,Menlo,Consolas,monospace;cursor:ns-resize;padding:2px 0}
.st .num{font-size:10px;color:var(--dim)}
.st button{padding:2px 0;width:80%;font-size:11px}
.keys{display:flex;user-select:none;touch-action:none;margin-top:8px}
.k{flex:1;height:90px;background:#eee;border:1px solid #333;border-radius:0 0 4px 4px}
.k.b{background:#222;height:56px;flex:.6;margin:0 -1.5%;z-index:1}
.k.on{background:var(--accent)}
.kpis{display:flex;flex-wrap:wrap;gap:18px}
.kpi b{display:block;font-size:20px;font-variant-numeric:tabular-nums} .kpi span{color:var(--dim);font-size:12px}
#ev{font-size:13px;color:#bbb;max-height:120px;overflow:auto}
small,.hint{color:var(--dim);font-size:12px}
a{color:#9fb3c8}
</style></head><body>
<header><span class="logo">SYNTHPATCH</span>
 <div class="lcd" id="lcd">-- INIT</div><span id="net"></span></header>
<main>

<section class="mod"><h2>PATCH MEMORY <span class="hint">click a slot to load it</span></h2>
 <div class="row" id="banks"></div>
 <div class="row slots" id="slots" style="margin-top:8px"></div>
 <div class="row" style="margin-top:10px">
  <input type="text" id="pname" maxlength="16" placeholder="patch name">
  <button class="save" id="save">SAVE</button><button id="del" title="delete the selected slot">delete</button>
  <span class="hint" id="pmsg"></span></div>
</section>

<div class="rack">
 <section class="mod" id="lfo1"><h2><b>LFO</b><span><span class="led" id="clip"></span>CLIP · MCP4725 0..3.3 V</span></h2>
  <canvas id="scope" height="130"></canvas>
  <div class="row shapes" id="shapes" style="margin-top:8px"></div>
  <div class="knobs" id="lfoKnobs"></div>
 </section>

 <section class="mod" id="env1"><h2><b>ENVELOPE</b><span id="envout">DAC</span></h2>
  <canvas id="adsr" height="130"></canvas>
  <div class="knobs" id="envKnobs"></div>
  <div class="row" style="margin-top:8px"><button id="gate">GATE (hold)</button><span><span class="led g" id="envled"></span>output</span></div>
 </section>
</div>

<section class="mod"><h2><b>SEQUENCER</b><span>V/oct → AD5693R · gate → envelope + GPIO 4 · trigger in GPIO 5</span></h2>
 <div class="row">
  <button id="play">▶ PLAY</button><button id="stop">■ STOP</button><button id="trig" title="restart at step 1">TRIG</button>
  <span style="width:16px"></span>
  <button id="mRetrig" title="a new gate for every step">RETRIG</button><button id="mLegato" title="one gate per loop, only the pitch changes">LEGATO</button>
  <span class="hint" id="mhint"></span><span style="flex:1"></span><b id="trans" style="color:var(--accent)"></b>
 </div>
 <div class="knobs" id="seqKnobs" style="justify-content:flex-start;gap:18px"></div>
 <div class="steps" id="steps"></div>
 <p class="hint"><b>Write:</b> click a step, then press keys: each key fills the selected step and moves on.
 Drag a note up/down or use the wheel for a semitone. ON/OFF = play or rest.
 <b>Play:</b> with no step selected (click it again to deselect), a key restarts the sequence from step 1,
 transposed so the bottom C = the key you pressed.</p>
 <label class="hint"><input type="checkbox" id="tonek" checked> keys also play the note on the test tone (GPIO 17 → 13)</label>
 <div class="keys" id="keys"></div></section>

<section class="mod"><h2><b>MONITOR</b><span id="outs"></span></h2>
 <div class="kpis">
  <div class="kpi"><b id="temp">-</b><span>temperature <i id="tsrc"></i></span></div>
  <div class="kpi"><b id="hz">-</b><span>input frequency</span></div>
  <div class="kpi"><b id="note">-</b><span>note / cents off</span></div>
  <div class="kpi"><b id="drift">-</b><span>drift (30 s avg)</span></div>
  <div class="kpi"><b id="volts">-</b><span>ADC input</span></div>
 </div>
 <div class="row" style="margin:12px 0 6px">
  <select id="range"><option value="0">live (10 min)</option><option value="1">1 hour</option><option value="24">24 hours</option>
  <option value="168">7 days</option><option value="336">14 days</option><option value="1104">46 days</option></select>
  <select id="series"><option value="drift">drift (cents)</option><option value="hz">frequency (Hz)</option><option value="volts">ADC (V)</option><option value="rh">humidity (%)</option></select>
  <small>orange = temperature, blue = selected, lines = note changes (live)</small>
  <a href="/log-full.csv?days=14">download 14 days (CSV)</a> <small id="loginfo"></small></div>
 <canvas id="chart" height="240"></canvas>
 <h2 style="margin-top:12px">NOTE EVENTS</h2><div id="ev"></div></section>
</main>
<script>
const $=id=>document.getElementById(id), NAMES=["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"];
const nn=n=>n<0?"-":NAMES[n%12]+(Math.floor(n/12)-1);
const post=u=>fetch(u,{method:"POST"});
const VDD=3.3, MID=VDD/2;            // the DAC can only make 0..3.3 V; the LFO swings around 1.65 V
let S={};                            // current settings: S.lfo1.rate, S.env1.attack, ... (numbers), S.lfo1.shape
let lfoV=MID, envV=0;                // live outputs from /api/live

// ================= KNOB =================
// One SVG knob: drag up/down (hold Shift for fine), mouse wheel, double-click = default value.
// map: how the 0..1 knob position becomes a value. "log" for rate and times, so the small values
// (5 ms, 0.1 Hz) get as much of the knob as the big ones.
const KNOBS={};
function knob(box,mod,p,opt){
 const el=document.createElement("div");el.className="knob";
 el.innerHTML=`<svg viewBox="0 0 64 64"><circle cx="32" cy="32" r="23" fill="#15171a" stroke="#000" stroke-width="2"/>
  <path class="trk" fill="none" stroke="#3b4048" stroke-width="4" stroke-linecap="round"/>
  <path class="arc" fill="none" stroke="#f0a020" stroke-width="4" stroke-linecap="round"/>
  <line class="ptr" x1="32" y1="32" x2="32" y2="14" stroke="#e6e6e6" stroke-width="3" stroke-linecap="round"/></svg>
  <span class="val"></span><span class="lbl">${opt.label}</span>`;
 box.appendChild(el);
 const log=opt.log, lo=p.min, hi=p.max;
 const toPos=v=>log?Math.log(v/lo)/Math.log(hi/lo):(v-lo)/(hi-lo);          // value -> 0..1
 const fromPos=x=>{x=Math.min(1,Math.max(0,x));return log?lo*Math.pow(hi/lo,x):lo+x*(hi-lo);};
 const A0=-135,A1=135;                                                       // knob turns 270 degrees
 const pt=a=>{const r=(a-90)*Math.PI/180;return[32+23*Math.cos(r)*1.18,32+23*Math.sin(r)*1.18];};
 const arc=(a,b)=>{if(a>b)[a,b]=[b,a];const[x0,y0]=pt(a),[x1,y1]=pt(b);return`M${x0} ${y0}A27 27 0 ${b-a>180?1:0} 1 ${x1} ${y1}`;};
 el.querySelector(".trk").setAttribute("d",arc(A0,A1));
 let pos=toPos(S[mod][p.name]), timer;
 const k={set(v){pos=toPos(v);draw();}, el};
 function draw(){
  const v=fromPos(pos),a=A0+pos*(A1-A0),from=opt.bipolar?0:A0;              // bipolar: arc from the top
  el.querySelector(".arc").setAttribute("d",arc(from,a));
  el.querySelector(".ptr").setAttribute("transform",`rotate(${a} 32 32)`);
  el.querySelector(".val").textContent=opt.fmt(v);
 }
 function change(x){
  pos=Math.min(1,Math.max(0,x));const v=fromPos(pos);S[mod][p.name]=v;draw();redraw();edited();
  clearTimeout(timer);timer=setTimeout(()=>opt.send?opt.send(v):post(`/api/set?target=${mod}&name=${p.name}&value=${v.toFixed(4)}`),40);
 }
 let y0,p0;
 el.onpointerdown=e=>{el.setPointerCapture(e.pointerId);y0=e.clientY;p0=pos;};
 el.onpointermove=e=>{if(y0===undefined)return;change(p0+(y0-e.clientY)/(e.shiftKey?800:180));};
 el.onpointerup=el.onpointercancel=()=>{y0=undefined;};
 el.onwheel=e=>{e.preventDefault();change(pos-Math.sign(e.deltaY)*(e.shiftKey?.005:.03));};
 el.ondblclick=()=>change(toPos(opt.def));
 draw();KNOBS[mod+"."+p.name]=k;return k;
}
const f2=(v,u)=>v.toFixed(2)+u, sec=v=>v<1?Math.round(v*1000)+" ms":v.toFixed(2)+" s";
const KOPT={                                                                  // look and default per parameter
 "lfo1.rate":{label:"RATE",log:1,def:.5,fmt:v=>v<1?v.toFixed(2)+" Hz":v.toFixed(1)+" Hz"},
 "lfo1.depth":{label:"DEPTH",def:MID,fmt:v=>"±"+f2(v," V")},
 "lfo1.offset":{label:"OFFSET",bipolar:1,def:MID,fmt:v=>(v>=MID?"+":"")+f2(v-MID," V")},   // shown around 1.65 V
 "env1.attack":{label:"ATTACK",log:1,def:.05,fmt:sec},"env1.decay":{label:"DECAY",log:1,def:.2,fmt:sec},
 "env1.sustain":{label:"SUSTAIN",def:.6,fmt:v=>Math.round(v*100)+" %"},"env1.release":{label:"RELEASE",log:1,def:.8,fmt:sec},
 "env1.peak":{label:"PEAK",def:VDD,fmt:v=>f2(v," V")}};

// ================= WAVE PREVIEWS =================
const WAVES={sine:x=>Math.sin(2*Math.PI*x),triangle:x=>x<.25?4*x:x<.75?2-4*x:4*x-4,saw:x=>2*x-1,square:x=>x<.5?1:-1};
function canvas(id){const c=$(id),g=c.getContext("2d"),r=devicePixelRatio,W=c.width=c.clientWidth*r,H=c.height=c.clientHeight*r;return{g,W,H,r};}
function drawScope(){
 const{g,W,H,r}=canvas("scope"),L=S.lfo1;if(!L)return;
 const Y=v=>H-8*r-(v/VDD)*(H-16*r);
 g.fillStyle="#0d0e10";g.fillRect(0,0,W,H);
 g.strokeStyle="#333";g.setLineDash([4*r,4*r]);g.beginPath();g.moveTo(0,Y(MID));g.lineTo(W,Y(MID));g.stroke();g.setLineDash([]);
 g.strokeStyle="#5a2a2a";[0,VDD].forEach(v=>{g.beginPath();g.moveTo(0,Y(v));g.lineTo(W,Y(v));g.stroke();});
 g.fillStyle="#667";g.font=`${10*r}px system-ui`;g.fillText("3.3 V",4*r,Y(VDD)+11*r);g.fillText("1.65",4*r,Y(MID)-3*r);g.fillText("0 V",4*r,Y(0)-3*r);
 // two cycles of the wave; whatever is outside 0..3.3 V is drawn red where the DAC clips it
 const f=WAVES[L.shape]||WAVES.sine;let clip=false;g.lineWidth=2*r;
 for(let i=0;i<W;i++){const raw=L.offset+L.depth*f((i/W*2)%1),v=Math.min(VDD,Math.max(0,raw));
  if(raw!==v)clip=true;g.strokeStyle=raw!==v?"#ff4d4d":"#f0a020";g.beginPath();g.moveTo(i,Y(v));g.lineTo(i+1,Y(v));g.stroke();}
 g.fillStyle="#4ea1ff";g.beginPath();g.arc(W-8*r,Y(lfoV),4*r,0,7);g.fill();       // the real output now
 $("clip").classList.toggle("on",clip);
}
function drawAdsr(){
 const{g,W,H,r}=canvas("adsr"),E=S.env1;if(!E)return;
 const hold=.25*(E.attack+E.decay+E.release)+.1, tot=E.attack+E.decay+hold+E.release;
 const X=t=>6*r+t/tot*(W-12*r), Y=v=>H-8*r-(v/VDD)*(H-16*r), pk=E.peak, su=E.sustain*pk;
 g.fillStyle="#0d0e10";g.fillRect(0,0,W,H);
 g.strokeStyle="#f0a020";g.lineWidth=2*r;g.beginPath();g.moveTo(X(0),Y(0));g.lineTo(X(E.attack),Y(pk));
 g.lineTo(X(E.attack+E.decay),Y(su));g.lineTo(X(E.attack+E.decay+hold),Y(su));g.lineTo(X(tot),Y(0));g.stroke();
 g.fillStyle="#667";g.font=`${10*r}px system-ui`;
 [["A",E.attack/2],["D",E.attack+E.decay/2],["S",E.attack+E.decay+hold/2],["R",tot-E.release/2]].forEach(([l,t])=>g.fillText(l,X(t),H-2*r));
 g.fillStyle="#4ea1ff";g.fillRect(W-10*r,Y(envV),6*r,Y(0)-Y(envV));               // live level bar
}
const redraw=()=>{drawScope();drawAdsr();};

// ================= BUILD THE MODULES FROM /api/settings =================
async function loadSettings(){
 const s=await (await fetch("/api/settings")).json();
 for(const[id,m] of Object.entries(s)){S[id]={shape:m.shape};S[id]._p=m.params;for(const p of m.params)S[id][p.name]=p.value;}
}
function buildModules(){
 for(const[id,box] of [["lfo1",$("lfoKnobs")],["env1",$("envKnobs")]])
  for(const p of S[id]._p) if(KOPT[id+"."+p.name]) knob(box,id,p,KOPT[id+"."+p.name]);
 for(const sh of Object.keys(WAVES)){                                          // shape buttons with a tiny icon
  const b=document.createElement("button");b.dataset.shape=sh;b.title=sh;let d="";
  for(let i=0;i<=28;i++)d+=(i?"L":"M")+i+" "+(7-6*WAVES[sh](Math.min(i/28,.999)));
  b.innerHTML=`<svg viewBox="0 0 28 14"><path d="${d}" fill="none" stroke="currentColor" stroke-width="2"/></svg>`;
  b.onclick=()=>{S.lfo1.shape=sh;post(`/api/shape?target=lfo1&shape=${sh}`);showShape();redraw();edited();};
  $("shapes").appendChild(b);
 }
 showShape();
}
const showShape=()=>document.querySelectorAll("#shapes button").forEach(b=>b.classList.toggle("on",b.dataset.shape==S.lfo1.shape));
function syncKnobs(){for(const[key,k] of Object.entries(KNOBS)){const[m,n]=key.split(".");k.set(S[m][n]);}showShape();redraw();}

// ================= PATCH MEMORY =================
// 4 banks x 8 slots. The ESP stores them (PatchBank.cpp); the page only lists, saves and loads.
let P=[], bank=0, cur=-1, dirty=false;
const lcd=()=>{const p=P[cur];$("lcd").innerHTML=(cur<0?"--":p.label)+" "+(cur<0||!p.name?"INIT":p.name.toUpperCase())+(dirty?"*":"")+
 `<small>${dirty?"edited":""}</small>`;};
const edited=()=>{if(!dirty){dirty=true;lcd();}};
function drawSlots(){
 $("banks").innerHTML="";"ABCD".split("").forEach((c,i)=>{const b=document.createElement("button");b.textContent="BANK "+c;
  b.className=i==bank?"on":"";b.onclick=()=>{bank=i;drawSlots();};$("banks").appendChild(b);});
 $("slots").innerHTML="";
 for(let i=0;i<8;i++){const s=bank*8+i,p=P[s],b=document.createElement("button");b.textContent=i+1;
  b.title=p.name?`${p.label} ${p.name}`:`${p.label} (empty)`;b.className=(p.name?"full ":"")+(s==cur?"cur":"");
  b.onclick=()=>select(s);$("slots").appendChild(b);}
}
async function listPatches(){P=await (await fetch("/api/patches")).json();drawSlots();lcd();}
async function select(s){
 cur=s;$("pname").value=P[s].name||"";
 if(P[s].name){                                  // a filled slot: load it into the engine
  if(dirty&&!confirm("Discard your unsaved changes?"))return;
  await post(`/api/patch/load?slot=${s}`);dirty=false;
  setTimeout(async()=>{await loadSettings();await loadSeq();syncKnobs();drawSteps();showMode();},150);              // engine applies it within ~1 ms
  msg(`loaded ${P[s].label}`);
 }else msg(`${P[s].label} is empty: type a name and press SAVE`);
 drawSlots();lcd();
}
$("save").onclick=async()=>{
 if(cur<0){msg("pick a slot first");return;}
 const name=$("pname").value.trim()||"Init";
 if(P[cur].name&&P[cur].name!==name&&!confirm(`Overwrite ${P[cur].label} "${P[cur].name}"?`))return;
 const r=await post(`/api/patch/save?slot=${cur}&name=${encodeURIComponent(name)}`);
 dirty=!r.ok;await listPatches();msg(r.ok?`saved to ${P[cur].label}`:"save failed");
};
$("del").onclick=async()=>{if(cur<0||!P[cur].name||!confirm(`Delete ${P[cur].label} "${P[cur].name}"?`))return;
 await post(`/api/patch/delete?slot=${cur}`);await listPatches();msg("deleted");};
const msg=t=>$("pmsg").textContent=t;

// ================= SEQUENCER =================
// The ESP runs it (1 ms timing in the engine task); the page only edits the pattern and shows the
// running light. Pattern: {bpm, gate (0.05..0.95), mode "retrig"|"legato", length, steps:[{n,on} x16]}
let selStep=-1;
const postSeq=o=>fetch("/api/seq",{method:"POST",body:JSON.stringify(o)});
async function loadSeq(){const q=await (await fetch("/api/seq")).json();S.seq=q;}
function buildSeq(){
 const sendNum=k=>v=>{S.seq[k]=k=="length"?Math.round(v):v;postSeq({[k]:S.seq[k]});if(k=="length")drawSteps();};
 knob($("seqKnobs"),"seq",{name:"bpm",min:20,max:300},{label:"BPM",def:120,fmt:v=>Math.round(v)+"",send:sendNum("bpm")});
 knob($("seqKnobs"),"seq",{name:"gate",min:.05,max:.95},{label:"GATE",def:.5,fmt:v=>Math.round(v*100)+" %",send:sendNum("gate")});
 knob($("seqKnobs"),"seq",{name:"length",min:1,max:16},{label:"STEPS",def:8,fmt:v=>Math.round(v)+"",send:sendNum("length")});
 $("play").onclick=()=>seqCmd("/api/seq/run?on=1");$("stop").onclick=()=>seqCmd("/api/seq/run?on=0");$("trig").onclick=()=>seqCmd("/api/seq/trig");
 $("mRetrig").onclick=()=>setMode("retrig");$("mLegato").onclick=()=>setMode("legato");
 drawSteps();showMode();
}
function setMode(m){S.seq.mode=m;postSeq({mode:m});showMode();edited();}
function showMode(){$("mRetrig").classList.toggle("on",S.seq.mode=="retrig");$("mLegato").classList.toggle("on",S.seq.mode=="legato");
 $("mhint").textContent=S.seq.mode=="legato"?"one gate per loop, GATE knob not used":"new gate every step";}
const sendSteps=()=>{postSeq({steps:S.seq.steps});edited();};
function drawSteps(){
 const box=$("steps");box.innerHTML="";
 S.seq.steps.forEach((st,i)=>{
  const d=document.createElement("div");d.className="st"+(i>=Math.round(S.seq.length)?" out":"")+(i==selStep?" sel":"");
  d.innerHTML=`<span class="led g" id="sl${i}"></span><span class="nt">${nn(st.n)}</span><button class="${st.on?"on":""}">${st.on?"ON":"OFF"}</button><span class="num">${i+1}</span>`;
  d.onclick=e=>{if(e.target.tagName=="BUTTON")return;selStep=selStep==i?-1:i;drawSteps();};
  d.querySelector("button").onclick=()=>{st.on=!st.on;sendSteps();drawSteps();};
  const nt=d.querySelector(".nt");let y0,n0,moved;               // drag the note up/down = semitones
  const setN=n=>{st.n=Math.max(24,Math.min(96,n));nt.textContent=nn(st.n);};
  nt.onpointerdown=e=>{nt.setPointerCapture(e.pointerId);y0=e.clientY;n0=st.n;moved=false;};
  nt.onpointermove=e=>{if(y0===undefined)return;const n=n0+Math.round((y0-e.clientY)/12);if(n!=st.n){moved=true;setN(n);}};
  nt.onpointerup=()=>{y0=undefined;if(moved){sendSteps();}};
  nt.onwheel=e=>{e.preventDefault();setN(st.n-Math.sign(e.deltaY));clearTimeout(nt.t);nt.t=setTimeout(sendSteps,150);};
  box.appendChild(d);});
}
// Running light. Asking the ESP 8x a second still skipped steps (a step at 120 BPM is 125 ms,
// about as long as a poll), so: ask twice a second where it is (step, ms into the step, step time)
// and let the browser count the steps in between with its own clock.
let seqPos=-1, R={run:false,pos:-1,len:8,step_ms:125,in_ms:0,tr:0}, rAt=0;
async function pollSeq(again=true){
 try{R=await (await fetch("/api/seq/pos")).json();rAt=performance.now();
  $("play").classList.toggle("on",R.run);
  $("trans").textContent=R.tr?`transpose ${R.tr>0?"+":""}${R.tr} (C3 → ${nn(48+R.tr)})`:"transpose 0";}catch(e){}
 if(again)setTimeout(pollSeq,500);
}
const seqCmd=async u=>{await post(u);pollSeq(false);};   // after play/stop/trig: re-sync the light at once
function animSeq(){
 let p=-1;
 if(R.run&&R.pos>=0)p=(R.pos+Math.floor((R.in_ms+performance.now()-rAt)/R.step_ms))%R.len;
 if(p!==seqPos){$("sl"+seqPos)?.classList.remove("on");seqPos=p;$("sl"+seqPos)?.classList.add("on");}
 requestAnimationFrame(animSeq);
}

// ================= GATE + KEYBOARD =================
const hold=(el,on,off)=>{el.onpointerdown=e=>{el.setPointerCapture(e.pointerId);el.classList.add("on");on();};
 el.onpointerup=el.onpointercancel=()=>{el.classList.remove("on");off();};};
hold($("gate"),()=>post("/api/gate?on=1"),()=>post("/api/gate?on=0"));
// Two jobs for the keys:
//  * a step is selected  -> STEP ENTRY: write the note there, select the next step
//                           (and play the note so you hear it, when the sequencer is stopped)
//  * no step selected    -> PLAY THE SEQUENCE from this key: restart at step 1, transposed so the
//                           bottom C (C3) of the keyboard = the key you pressed
for(let n=48;n<=72;n++){const k=document.createElement("div");k.className="k"+([1,3,6,8,10].includes(n%12)?" b":"");k.title=nn(n);
 const running=()=>$("play").classList.contains("on");
 let preview=false;
 hold(k,()=>{
   if(selStep>=0){S.seq.steps[selStep].n=n;S.seq.steps[selStep].on=true;sendSteps();selStep=(selStep+1)%Math.round(S.seq.length);drawSteps();
     if(!running()){preview=true;post(`/api/note?n=${n}&on=1&tone=${$("tonek").checked?1:0}`);}}
   else seqCmd(`/api/seq/trig?note=${n}`);},
  ()=>{if(preview){preview=false;post(`/api/note?n=${n}&on=0`);}});
 $("keys").appendChild(k);}

// ================= HISTORY CHART (IoT monitor) =================
let H={t:[],temp:[],hz:[],drift:[],volts:[],rh:[]}, EV=[], evTotal=-1;
function draw(){
 const{g,W,H:Ht,r}=canvas("chart");g.fillStyle="#0d0e10";g.fillRect(0,0,W,Ht);if(H.t.length<2)return;
 const t0=H.t[0],t1=H.t[H.t.length-1]||1,X=t=>40*r+(t-t0)/Math.max(1,t1-t0)*(W-80*r);
 const line=(arr,col,side)=>{const v=arr.filter(x=>x!==null&&!isNaN(x));if(!v.length)return;
  let lo=Math.min(...v),hi=Math.max(...v);if(hi-lo<1e-3){lo-=.5;hi+=.5;}const Y=y=>Ht-10*r-(y-lo)/(hi-lo)*(Ht-20*r);
  g.strokeStyle=col;g.lineWidth=2*r;g.beginPath();let pen=false;
  arr.forEach((y,i)=>{if(y===null||isNaN(y)){pen=false;return;}pen?g.lineTo(X(H.t[i]),Y(y)):g.moveTo(X(H.t[i]),Y(y));pen=true;});g.stroke();
  g.fillStyle=col;g.font=`${11*r}px system-ui`;g.textAlign=side;const x=side=="left"?2:W-2;
  g.fillText(hi.toPrecision(4),x,14*r);g.fillText(lo.toPrecision(4),x,Ht-4*r);};
 g.strokeStyle="#555";if(live())EV.forEach(e=>{if(e.t>=t0){g.beginPath();g.moveTo(X(e.t),0);g.lineTo(X(e.t),Ht);g.stroke();}});
 line(H.temp,"#f0a020","left");if(H[$("series").value])line(H[$("series").value],"#4ea1ff","right");
 const fmt=t=>t>1e9?new Date(t*1000).toLocaleString([],{month:"numeric",day:"numeric",hour:"2-digit",minute:"2-digit"}):t+" s";
 g.fillStyle="#889";g.font=`${11*r}px system-ui`;g.textAlign="left";g.fillText(fmt(t0),40*r,Ht-4*r);
 g.textAlign="right";g.fillText(fmt(t1),W-40*r,Ht-4*r);
}
$("series").onchange=draw;addEventListener("resize",()=>{draw();redraw();});
const live=()=>$("range").value=="0";
async function loadRange(){H=await (await fetch(live()?"/api/history":`/api/log?hours=${$("range").value}&points=800`)).json();draw();}
$("range").onchange=loadRange;

// ================= LIVE VALUES, once a second =================
async function tick(){
 try{
  const s=await (await fetch("/api/live")).json();
  lfoV=s.lfo_v;envV=s.env_v;$("envled").classList.toggle("on",s.env_v>.05);redraw();
  $("temp").textContent=s.temp_c==null?"-":s.temp_c.toFixed(2)+" °C"+(s.rh==null?"":` · ${s.rh.toFixed(0)}%`);
  $("tsrc").textContent=s.temp_sensor?"("+s.temp_sensor+")":"";
  $("hz").textContent=s.hz>0?s.hz.toFixed(2)+" Hz":"no input";
  $("note").textContent=s.hz>0?`${nn(s.note)} ${s.cents_off>=0?"+":""}${s.cents_off.toFixed(1)}¢`:"-";
  $("drift").textContent=`${s.drift.toFixed(1)}¢ (${s.drift_per_min>=0?"+":""}${s.drift_per_min.toFixed(1)}/min)`;
  $("volts").textContent=s.volts.toFixed(3)+" V";
  $("outs").textContent=`LFO ${s.lfo_v.toFixed(2)} V · ENV ${s.env_v.toFixed(2)} V · V/oct ${s.voct_v.toFixed(3)} V (${nn(s.voct_note)})`;
  $("envout").textContent=s.env_out;
  $("net").textContent=`${s.wifi} · ${s.rssi} dBm · up ${s.uptime}s`+(s.mqtt?" · MQTT ok":"");
  $("loginfo").textContent=s.time?`· ${s.log_count} samples stored`:"· log waits for NTP time (home WiFi)";
  if(!live()){if(s.uptime%60==0)loadRange();}
  else if(!H.t.length||s.uptime>H.t[H.t.length-1]){for(const k of ["temp","hz","drift","volts","rh"])H[k].push(s[{temp:"temp_c",hz:"hz",drift:"drift",volts:"volts",rh:"rh"}[k]]);H.t.push(s.uptime);
   if(H.t.length>600)for(const k in H)H[k].shift();}
  if(s.events_total!==evTotal){evTotal=s.events_total;await loadEvents();}
  draw();
 }catch(e){$("net").textContent="offline";}
}
async function loadEvents(){EV=await (await fetch("/api/events")).json();
 $("ev").innerHTML=EV.slice().reverse().map(e=>`${e.t}s: ${nn(e.from)} → <b>${nn(e.to)}</b>`).join("<br>");}
(async()=>{await loadSettings();await loadSeq();buildModules();buildSeq();pollSeq();animSeq();redraw();await listPatches();await loadRange();await loadEvents();tick();setInterval(tick,1000);})();
</script></body></html>)HTML";
