import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import type { RealizationState, Vec3 } from '../src/index.js';
import { defaults, makeRuntime, makeScene } from './scenario.js';
import './style.css';
const settings={...defaults};
const app=document.querySelector<HTMLDivElement>('#app')!;
app.innerHTML=`<header><div class="brand"><span class="mark">⌁</span><div><h1>SpatialGL</h1><div class="subtitle">World coordinates. Physical realizations.</div></div></div><span class="badge">SIMULATION LAB / v0.1</span></header>
<div class="layout"><aside>
<div class="section"><h2>01 / Scene</h2><label class="control" for="scenario">장면</label><select id="scenario"><option value="mixed">표면 + 공중 점</option><option value="wall">벽과 모니터</option><option value="air">공중 점 3개</option><option value="unsupported">공중 Patch · 미실현</option></select><label class="control" for="sceneHz">장면 갱신 <output id="sceneHz-out"></output></label><input class="range" id="sceneHz" type="range" min="1" max="30" value="10"></div>
<div class="section"><h2>02 / Physical backends</h2><label class="check"><input id="projector" type="checkbox" checked>프로젝터 × 2</label><label class="check"><input id="monitor" type="checkbox" checked>모니터 표면</label><label class="check"><input id="drone" type="checkbox" checked>발광 드론</label><label class="check"><input id="occlusion" type="checkbox">차폐 상자 추가</label><label class="control" for="count">드론 수 <output id="count-out"></output></label><input class="range" id="count" type="range" min="1" max="6" value="3"><label class="control" for="speed">이동 속도 <output id="speed-out"></output></label><input class="range" id="speed" type="range" min="0.2" max="4" step="0.1" value="1.2"></div>
<div class="section"><h2>03 / Device time</h2><label class="control" for="refresh">장치 갱신 <output id="refresh-out"></output></label><input class="range" id="refresh" type="range" min="1" max="60" value="30"><label class="control" for="latency">명령 지연 <output id="latency-out"></output></label><input class="range" id="latency" type="range" min="0" max="0.8" step="0.01" value="0.08"><p class="note">설정 변경 시 시뮬레이션을 재시작합니다. 모든 수치는 탐색용 가정입니다.</p></div>
<p class="note">Point · Polyline · Patch<br>+Y up · metre · second<br>목표의 반투명 점과 실제 출력을 비교하세요. 드래그로 회전, 스크롤로 확대합니다.</p></aside>
<main class="workspace"><div class="viewport"><canvas id="view" aria-label="SpatialGL 3D simulation"></canvas><div class="overlay"><h3>Physical space</h3><p>Scene intent → device commands → observed state</p></div><div class="toolbar"><button id="pause">일시정지</button><button id="step">+ 100 ms</button><button id="reset">초기화</button><button id="export">기록 저장</button></div><div class="legend"><span><i class="dot" style="background:#738498"></i>목표</span><span><i class="dot" style="background:#5cdfd1"></i>실현</span><span><i class="dot" style="background:#ffb552"></i>추적 중</span><span><i class="dot" style="background:#fb8793"></i>미실현</span></div></div>
<div class="dashboard"><div class="metrics"><div class="metric"><small>SIMULATION TIME</small><strong id="time">0.00 s</strong></div><div class="metric"><small>실현 / 전체 표본</small><strong id="coverage">0 / 0</strong></div><div class="metric"><small>이동 추적 / 대기</small><strong id="tracking">0 / 0</strong></div><div class="metric"><small>평균 위치 오차</small><strong id="error">—</strong></div></div><div id="status" class="statusline"></div><table><thead><tr><th>Primitive</th><th>Backend</th><th>실제 상태</th><th>실현 제약</th></tr></thead><tbody id="diagnostics"></tbody></table><details><summary>DeviceFrame 명령 확인</summary><pre id="commands"></pre></details><div class="footer-note">표본 기반 기하·시간 모델 · 광학/비행 물리 및 하드웨어 제어는 후속 단계</div></div></main></div>`;
const $=<T extends HTMLElement>(id: string)=>document.getElementById(id) as T;
let runtime=makeRuntime(settings), time=0, revision=0, nextScene=0, paused=false;
let state: RealizationState;
const trace: RealizationState[]=[];
const canvas=$<HTMLCanvasElement>('view');
const renderer=new THREE.WebGLRenderer({canvas,antialias:true}); renderer.setPixelRatio(Math.min(devicePixelRatio,2));
renderer.setClearColor(0x0c141f,0);
const scene=new THREE.Scene(); scene.fog=new THREE.Fog(0x0c141f,12,30);
const camera=new THREE.PerspectiveCamera(45,1,0.1,60); camera.position.set(7,5,8);
const controls=new OrbitControls(camera,canvas); controls.target.set(0,1,-0.1); controls.enableDamping=true;
scene.add(new THREE.HemisphereLight(0xdfeeff,0x374658,2));
const grid=new THREE.GridHelper(8,16,0x3a566d,0x223447); scene.add(grid);
const staticGroup=new THREE.Group(), dynamicGroup=new THREE.Group(); scene.add(staticGroup,dynamicGroup);
function clear(group: THREE.Group) {
  for(const child of [...group.children]) { group.remove(child); child.traverse(o=>{ const mesh=o as THREE.Mesh; mesh.geometry?.dispose(); if(mesh.material) (Array.isArray(mesh.material)?mesh.material:[mesh.material]).forEach(m=>m.dispose()); }); }
}
function line(points: readonly Vec3[],color: number,opacity=1): THREE.Line {
  return new THREE.Line(new THREE.BufferGeometry().setFromPoints(points.map(p=>new THREE.Vector3(...p))),new THREE.LineBasicMaterial({color,transparent:true,opacity}));
}
function rebuildWorld() {
  clear(staticGroup);
  for(const s of runtime.world.surfaces) {
    const o=new THREE.Vector3(...s.origin),u=new THREE.Vector3(...s.u),v=new THREE.Vector3(...s.v);
    const geom=new THREE.BufferGeometry();
    const corners=[o,o.clone().add(u),o.clone().add(u).add(v),o.clone().add(v)];
    geom.setFromPoints([corners[0],corners[1],corners[2],corners[0],corners[2],corners[3]]);
    staticGroup.add(new THREE.Mesh(geom,new THREE.MeshBasicMaterial({color:s.id==='wall'?0x2a4054:0x566b90,side:THREE.DoubleSide,transparent:true,opacity:s.id==='wall'?0.28:0.55,depthWrite:false})));
    const edge=corners.map(p=>p.toArray() as unknown as Vec3); staticGroup.add(line([...edge,edge[0]],0x4a6178));
  }
  for(const b of runtime.world.occluders) {
    const mesh=new THREE.Mesh(new THREE.BoxGeometry(...b.max.map((v,i)=>v-b.min[i]) as [number,number,number]),new THREE.MeshStandardMaterial({color:0x735863,transparent:true,opacity:0.7}));
    mesh.position.set(...b.max.map((v,i)=>(v+b.min[i])/2) as [number,number,number]); staticGroup.add(mesh);
  }
  if(settings.projector) for(const x of [-2,2]) {
    const mesh=new THREE.Mesh(new THREE.BoxGeometry(0.24,0.15,0.32),new THREE.MeshStandardMaterial({color:0x5cdfd1})); mesh.position.set(x,2.7,2); staticGroup.add(mesh);
    staticGroup.add(line([[x,0,2],[x,2.7,2]],0x344b61));
  }
}
function points(positions: Vec3[],color: number,size: number,opacity=1) {
  if(!positions.length) return;
  const geometry=new THREE.BufferGeometry().setFromPoints(positions.map(p=>new THREE.Vector3(...p)));
  dynamicGroup.add(new THREE.Points(geometry,new THREE.PointsMaterial({color,size,transparent:true,opacity,sizeAttenuation:true})));
}
function drawState() {
  clear(dynamicGroup);
  points(state.samples.map(s=>s.sample.position),0x8ba0b7,0.035,0.45);
  for(const [status,color] of [['realized',0x5cdfd1],['tracking',0xffb552],['pending',0x8395aa],['unrealizable',0xfb8793]] as const) {
    const list=state.samples.filter(s=>s.status===status);
    points(list.filter(s=>!s.actual).map(s=>s.sample.position),color,0.055,0.5);
  }
  for(const o of state.outputs) {
    const intended=state.samples.find(s=>s.sample.id===o.sampleId && s.deviceId===o.deviceId);
    points([o.position],!intended?0xc4a7ff:intended.status==='realized'?0x5cdfd1:0xffb552,o.deviceId==='drone-swarm'?0.12:0.055);
  }
  for(const s of state.samples) {
    if(s.actual && s.error!>0.04) dynamicGroup.add(line([s.sample.position,s.actual],0xffb552,0.5));
    if(s.actual && s.deviceId?.startsWith('projector') && s.sample.id.endsWith(':0')) dynamicGroup.add(line([[s.deviceId.endsWith('left')?-2:2,2.7,2],s.actual],0x5cdfd1,0.28));
  }
  const count=(status: string)=>state.samples.filter(s=>s.status===status).length;
  $('time').textContent=`${time.toFixed(2)} s`;
  $('coverage').textContent=`${count('realized')} / ${state.samples.length}`;
  $('tracking').textContent=`${count('tracking')} / ${count('pending')}`;
  const errors=state.samples.flatMap(s=>s.error===null?[]:[s.error]);
  $('error').textContent=errors.length?`${(errors.reduce((a,b)=>a+b,0)/errors.length*100).toFixed(1)} cm`:'—';
  $('status').textContent=`${state.sceneId??'no active scene'} · ${runtime.backends.length} devices · ${count('unrealizable')} unrealizable · ${state.outputs.filter(o=>o.stale).length} prior-scene outputs · validity 500 ms`;
  const groups=new Map<string,typeof state.samples>();
  for(const s of state.samples) groups.set(s.sample.primitiveId,[...(groups.get(s.sample.primitiveId)??[]),s]);
  $('diagnostics').replaceChildren(...[...groups].map(([id,list])=>{
    const tr=document.createElement('tr');
    for(const text of [id,[...new Set(list.map(s=>s.deviceId??'—'))].join(', '),[...new Set(list.map(s=>s.status))].join(', '),[...new Set(list.flatMap(s=>s.reasons))].join(' · ')||'—']) { const td=document.createElement('td'); td.textContent=text; tr.append(td); }
    return tr;
  }));
  $('commands').textContent=JSON.stringify(state.deviceFrames.map(f=>({...f,command:{...f.command,...(f.command.kind==='raster-samples'?{samples:f.command.samples.slice(0,2),totalSamples:f.command.samples.length}:{})}})),null,2);
}
function update(dt: number) {
  const end=time+dt;
  while(nextScene<=end+1e-9) {
    runtime.advance(nextScene); runtime.submit(makeScene(nextScene,revision++,settings.scenario)); nextScene+=1/settings.sceneHz;
  }
  time=end; state=runtime.advance(time); drawState();
  trace.push(state); if(trace.length>300) trace.shift();
}
function reset() { runtime=makeRuntime(settings); time=0; revision=0; nextScene=0; trace.length=0; rebuildWorld(); update(0); }
for(const key of ['projector','monitor','drone','occlusion'] as const) $<HTMLInputElement>(key).addEventListener('change',e=>{ settings[key]=(e.target as HTMLInputElement).checked; reset(); });
for(const key of ['count','speed','refresh','latency','sceneHz'] as const) {
  const show=()=>$(key+'-out').textContent=key==='latency'?`${(settings[key]*1000).toFixed(0)} ms`:key==='speed'?`${settings[key].toFixed(1)} m/s`:key==='count'?`${settings[key]}`:`${settings[key]} Hz`;
  show(); $<HTMLInputElement>(key).addEventListener('input',e=>{ settings[key]=Number((e.target as HTMLInputElement).value); show(); reset(); });
}
$('scenario').addEventListener('change',e=>{ settings.scenario=(e.target as HTMLSelectElement).value; reset(); });
const setPaused=(value: boolean)=>{ paused=value; $('pause').textContent=paused?'재생':'일시정지'; };
$('pause').addEventListener('click',()=>setPaused(!paused));
$('step').addEventListener('click',()=>{setPaused(true);update(0.1);});
$('reset').addEventListener('click',reset);
$('export').addEventListener('click',()=>{
  const url=URL.createObjectURL(new Blob([JSON.stringify({schemaVersion:1,settings,world:runtime.world,trace},null,2)],{type:'application/json'}));
  const a=document.createElement('a'); a.href=url; a.download='spatialgl-trace.json'; a.click(); URL.revokeObjectURL(url);
});
new ResizeObserver(()=>{ const rect=canvas.getBoundingClientRect(); renderer.setSize(rect.width,rect.height,false); camera.aspect=rect.width/rect.height; camera.updateProjectionMatrix(); }).observe(canvas);
reset(); let last=performance.now(), accumulated=0;
function animate(now: number) {
  const dt=Math.min((now-last)/1000,0.1); last=now;
  if(!paused) { accumulated+=dt; if(accumulated>=1/20) {update(accumulated);accumulated=0;} }
  controls.update(); renderer.render(scene,camera); requestAnimationFrame(animate);
}
requestAnimationFrame(animate);
