#ifndef _LIVEPAGE_h
#define _LIVEPAGE_h

#include <Arduino.h>

// Live page (/live): reads /api/data every 5 s. Uses the CSS variables of the frontend.
const char LIVE_PAGE_HTML[] PROGMEM = R"rawliteral(
<style>
.lv-pills{display:flex;flex-wrap:wrap;gap:6px;margin-bottom:12px}
.lv-pill{background:var(--bg2);border:1px solid var(--div);border-radius:14px;padding:4px 10px;font-size:12px;color:var(--txt2)}
.lv-pill b{color:var(--txt);font-weight:500}
.lv-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(260px,1fr));gap:12px;margin-bottom:12px}
.lv-card{background:var(--card);border:1px solid var(--div);border-radius:8px;padding:12px}
.lv-card.stale{opacity:.5}
.lv-top{display:flex;justify-content:space-between;align-items:flex-start;gap:6px}
.lv-name{font-weight:500;font-size:15px}
.lv-id{font-family:monospace;font-size:12px;color:var(--pri)}
.lv-pow{font-size:30px;font-weight:600;margin:6px 0 2px}
.lv-pow span{font-size:14px;color:var(--txt2);margin-left:3px}
.lv-spark{height:46px;background:var(--bg2);border-radius:4px;margin:6px 0}
.lv-spark svg{width:100%;height:100%}
.lv-stats{display:grid;grid-template-columns:repeat(3,1fr);gap:4px;font-size:12px;border-top:1px solid var(--div);padding-top:6px}
.lv-stats span{display:block;color:var(--txt2);font-size:11px}
</style>
<div class='card' style='margin-bottom:12px'>
<h2>&#128200; Live</h2>
<div class='lv-pills' id='lvSys'><div class='lv-pill'>Lade ...</div></div>
<h3>EC3000</h3>
<div class='lv-grid' id='lvEc'><p class='info'>Noch keine EC3000 empfangen (RFM69 mit Datenrate 20000 n&ouml;tig).</p></div>
<h3>LaCrosse</h3>
<div id='lvLc'><p class='info'>Noch keine LaCrosse-Sensoren empfangen.</p></div>
<p class='info'>Die Daten gibt es als JSON unter <a href='/api/data'>/api/data</a>.</p>
</div>
<script>
function lvDur(s){var d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);
 return d>0?d+'d '+h+'h':h>0?h+'h '+m+'m':m+'m '+(s%60)+'s';}
function lvAge(s){return s<60?s+' s':s<3600?Math.floor(s/60)+' min':Math.floor(s/3600)+' h';}
function lvEsc(t){return String(t).replace(/[&<>"']/g,function(c){return '&#'+c.charCodeAt(0)+';';});}
function lvSpark(v){if(!v||v.length<2)return "<svg viewBox='0 0 100 30'><line x1='0' y1='26' x2='100' y2='26' stroke='var(--div)' stroke-dasharray='3 3'/></svg>";
 var mx=Math.max.apply(null,v),mn=Math.min.apply(null,v),r=(mx-mn)||1,w=240,h=44,p=3;
 var pts=v.map(function(x,i){return (i/(v.length-1)*w).toFixed(1)+','+(h-p-(x-mn)/r*(h-2*p)).toFixed(1);}).join(' ');
 return "<svg viewBox='0 0 "+w+" "+h+"' preserveAspectRatio='none'><polygon points='0,"+h+" "+pts+" "+w+","+h+"' fill='var(--pri)' fill-opacity='.18'/>"+
 "<polyline points='"+pts+"' fill='none' stroke='var(--pri)' stroke-width='2'/></svg>";}
function lvPill(k,v){return "<div class='lv-pill'>"+k+": <b>"+v+"</b></div>";}
function lvLoad(){fetch('/api/data',{cache:'no-store'}).then(function(r){return r.json();}).then(function(d){
 document.getElementById('lvSys').innerHTML=lvPill('Laufzeit',lvDur(d.uptime))+lvPill('WLAN',d.wifiRssi+' dBm')+
  lvPill('MQTT',d.mqttEnabled?(d.mqtt?'verbunden':'getrennt'):'aus')+lvPill('Heap',Math.round(d.freeHeap/1024)+' KB')+
  lvPill('Frames/min',d.framesPerMinute)+lvPill('Reset',lvEsc(d.resetReason));
 if(d.ec3000.length)document.getElementById('lvEc').innerHTML=d.ec3000.map(function(e){
  return "<div class='lv-card"+(e.age>120?" stale":"")+"'><div class='lv-top'><div><div class='lv-name'>"+lvEsc(e.name||('EC3000 '+e.id))+
  "</div><div class='lv-id'>ID "+e.id+" &middot; RFM"+e.rfm+"</div></div><span class='badge "+(e.on?"ok":"warn")+"'>"+(e.on?"an":"aus")+
  " &middot; "+lvAge(e.age)+"</span></div><div class='lv-pow'>"+e.power.toFixed(1)+"<span>W</span></div>"+
  "<div class='info'>max. "+e.maxPower.toFixed(1)+" W &middot; verworfen "+e.rejected+"</div>"+
  "<div class='lv-spark'>"+lvSpark(e.history)+"</div><div class='lv-stats'><div><span>Verbrauch</span>"+e.energy.toFixed(3)+" kWh</div>"+
  "<div><span>An-Zeit</span>"+lvDur(e.onSeconds)+"</div><div><span>RSSI</span>"+e.rssi+" dBm</div></div></div>";}).join('');
 if(d.lacrosse.length)document.getElementById('lvLc').innerHTML="<table><thead><tr><th>ID</th><th>Temperatur</th><th>Feuchte</th><th>Batterie</th><th>RSSI</th><th>Zuletzt</th></tr></thead><tbody>"+
  d.lacrosse.map(function(l){return "<tr><td>"+l.id+"</td><td>"+l.temperature.toFixed(1)+" &deg;C</td><td>"+(l.humidity>0&&l.humidity<=100?l.humidity+" %":"&ndash;")+
  "</td><td class='"+(l.weakBattery?"batt-weak":"batt-ok")+"'>"+(l.weakBattery?"schwach":"ok")+(l.newBattery?" (neu)":"")+"</td><td>"+l.rssi+" dBm</td><td>"+lvAge(l.age)+"</td></tr>";}).join('')+"</tbody></table>";
}).catch(function(){document.getElementById('lvSys').innerHTML=lvPill('Verbindung','getrennt');});}
lvLoad();setInterval(lvLoad,5000);
</script>
)rawliteral";

#endif
