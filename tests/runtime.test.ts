import { describe, expect, it } from 'vitest';
import { blocked, DroneBackend, MonitorBackend, plan, ProjectorBackend, sampleScene, SpatialRuntime } from '../src/index.js';
import type { Backend, Observation, SceneFrame, World } from '../src/index.js';
const world: World={surfaces:[{id:'wall',origin:[-2,0,0],u:[4,0,0],v:[0,3,0]}],occluders:[]};
const point=(id='p',position: readonly [number,number,number]=[0,1,0]): SceneFrame=>({id,presentAt:0,expiresAt:5,primitives:[{kind:'point',id:'dot',position,color:[1,0,0]}]});
const projector=(id='projector',latency=0)=>new ProjectorBackend({id,width:640,height:480,refreshHz:10,latency,position:[0,1,3],target:[0,1,0],up:[0,1,0],fovY:Math.PI/2,near:0.1,far:10});
const drone=(count=1)=>new DroneBackend({id:'drone',count,speed:1,home:[0,1,1],min:[-3,0,-3],max:[3,3,3],refreshHz:10,latency:0});
describe('physical support and routing',()=>{
  it('does not project unsupported points in empty air',()=>{ const b=projector(); expect(b.evaluate(sampleScene(point('p',[0,1,1]).primitives)[0],world)).toEqual({reason:'no-support-surface'}); });
  it('maps the optical axis into central pixels and reports quantization',()=>{
    const s=new SpatialRuntime(world,[projector()]);s.submit(point());const state=s.advance(0);expect(state.samples[0].status).toBe('realized');expect(state.samples[0].error).toBeGreaterThan(0);
    const cmd=state.deviceFrames[0].command;expect(cmd.kind).toBe('raster-samples');if(cmd.kind==='raster-samples')expect(cmd.samples[0].pixel).toEqual([320,240]);
  });
  it('clips projector frusta',()=>{const b=projector();b.projector.fovY=0.1;expect(b.evaluate(sampleScene(point('p',[1,1,0]).primitives)[0],world)).toEqual({reason:'outside-frustum'});});
  it('checks occlusion on the source-to-target segment',()=>{
    const sample=sampleScene(point().primitives)[0];expect(projector().evaluate(sample,{...world,occluders:[{id:'box',min:[-0.5,0.5,1],max:[0.5,1.5,2]}]})).toEqual({reason:'occluded'});
    expect(blocked([0,1,3],[0,1,0],[{id:'behind',min:[-1,0,-3],max:[1,2,-1]}])).toBe(false);
  });
  it('uses another projector when the first is blocked',()=>{
    const second=new ProjectorBackend({...projector().projector,id:'other',position:[2,1,3]});
    const p=plan(sampleScene(point().primitives),[projector(),second],{...world,occluders:[{id:'box',min:[-0.2,0.5,1.4],max:[0.2,1.5,1.6]}]});expect(p.assignments.get('other')).toHaveLength(1);
  });
  it('prefers calibrated monitor support over projection',()=>{
    const m=new MonitorBackend({id:'monitor',surfaceId:'wall',width:640,height:480,refreshHz:60,latency:0});
    const p=plan(sampleScene(point().primitives),[projector(),m],world);expect(p.assignments.get('monitor')).toHaveLength(1);expect(p.assignments.get('projector')).toHaveLength(0);
  });
  it('reports raster pixel collisions rather than claiming two independent outputs',()=>{const f=point();f.primitives=[...f.primitives,{...f.primitives[0],id:'same-pixel'}];const p=plan(sampleScene(f.primitives),[projector()],world);expect(p.assignments.get('projector')).toHaveLength(1);expect(p.unmet[0].attempts[0].reason).toBe('pixel-conflict');});
  it('reports drone capacity without turning a patch into a point',()=>{
    const f=point();f.primitives=[...f.primitives,{kind:'point',id:'second',position:[0,1,1],color:[1,1,0]}];
    const p=plan(sampleScene(f.primitives),[drone()],{surfaces:[],occluders:[]});expect(p.unmet[0].attempts[0].reason).toBe('capacity');
    const patch=sampleScene([{kind:'patch',id:'patch',origin:[0,1,1],u:[1,0,0],v:[0,1,0],color:[1,1,1]}]);expect(drone().evaluate(patch[0],world)).toEqual({reason:'unsupported-primitive'});
  });
});
describe('scene and device clocks',()=>{
  it('waits for presentAt and delivery latency',()=>{ const r=new SpatialRuntime(world,[projector('p',0.15)]);const f=point();f.presentAt=0.2;r.submit(f);expect(r.advance(0.19).sceneId).toBeNull();expect(r.advance(0.2).samples[0].status).toBe('pending');expect(r.advance(0.351).samples[0].status).toBe('realized'); });
  it('bounds drone travel by elapsed time',()=>{ const r=new SpatialRuntime({surfaces:[],occluders:[]},[drone()]);r.submit(point('p',[0,1,2]));expect(r.advance(0.5).samples[0].actual![2]).toBeCloseTo(1.5,10);expect(r.advance(1).samples[0].status).toBe('realized'); });
  it('gives identical state with large and small advances',()=>{
    const run=(times: number[])=>{ const r=new SpatialRuntime(world,[projector('p',0.17),drone()]);const f=point('p',[0,1,2]);r.submit(f);for(const t of times)r.advance(t);return r.snapshot(); };
    expect(run([1])).toEqual(run(Array.from({length:100},(_,i)=>(i+1)/100)));
  });
  it('retains source provenance while the next command is pending',()=>{
    const r=new SpatialRuntime(world,[projector('p',0.2)]);r.submit(point('first'));r.advance(0.3);
    const f=point('second',[0.5,1,0]);f.presentAt=0.3;r.submit(f);const s=r.advance(0.35).samples[0];expect(s.sourceSceneId).toBe('first');expect(s.error).toBeGreaterThan(0.4);
  });
  it('expires output even if a late device command arrives',()=>{const b=projector('p',0.3),r=new SpatialRuntime(world,[b]);const f=point();f.expiresAt=0.15;r.submit(f);r.advance(0.31);expect(b.observe(0.31)).toHaveLength(0);expect(r.snapshot().samples).toHaveLength(0);});
  it('clears removed primitives with full replacement device frames',()=>{const b=projector(),r=new SpatialRuntime(world,[b]);r.submit(point());r.advance(0.2);r.submit({id:'clear',presentAt:0.2,expiresAt:3,primitives:[]});r.advance(0.31);expect(b.observe(0.31)).toHaveLength(0);});
  it('does not resurrect a superseded scene after its replacement expires',()=>{const r=new SpatialRuntime(world,[projector()]);r.submit(point());r.advance(0.2);const f=point('new');f.presentAt=0.2;f.expiresAt=0.4;r.submit(f);expect(r.advance(0.5).sceneId).toBeNull();});
  it('reports removed outputs while clearing commands are in flight',()=>{const r=new SpatialRuntime(world,[projector('p',0.2)]);r.submit(point());r.advance(0.3);r.submit({id:'clear',presentAt:0.3,expiresAt:3,primitives:[]});const s=r.advance(0.35);expect(s.samples).toHaveLength(0);expect(s.outputs).toHaveLength(1);expect(s.outputs[0].stale).toBe(true);expect(r.advance(0.61).outputs).toHaveLength(0);});
  it('supports different refresh clocks on the same scene',()=>{const m=new MonitorBackend({id:'m',surfaceId:'wall',width:640,height:480,refreshHz:2,latency:0});const r=new SpatialRuntime(world,[m,projector()]);r.submit(point());const frames=r.advance(0.35).deviceFrames;expect(frames.find(f=>f.deviceId==='m')!.issuedAt).toBe(0);expect(frames.find(f=>f.deviceId==='projector')!.issuedAt).toBeCloseTo(0.3);});
  it('accepts another rendering command schema without changing the runtime',()=>{
    let outputs: Observation[]=[];
    const backend: Backend={descriptor:{id:'custom-emitter',modality:'emitter',refreshHz:10,latency:0,capacity:1},evaluate:sample=>({candidate:{sample,score:1,position:sample.position}}),encode:candidates=>({kind:'extension',schema:'test-emitter/v1',payload:candidates}),advance:()=>{},apply:frame=>{if(frame.command.kind==='extension')outputs=(frame.command.payload as {sample: {id: string};position: readonly [number,number,number]}[]).map(c=>({sampleId:c.sample.id,position:c.position,color:[1,0,0]}));},observe:()=>outputs,reset:()=>{outputs=[];}};
    const r=new SpatialRuntime(world,[backend]);r.submit(point());expect(r.advance(0).samples[0].status).toBe('realized');expect(r.snapshot().deviceFrames[0].command.kind).toBe('extension');
  });
  it('rejects non-finite calibration and invalid emitter bounds',()=>{expect(()=>new ProjectorBackend({...projector().projector,fovY:NaN})).toThrow();expect(()=>new DroneBackend({...drone().config,home:[NaN,0,0]})).toThrow();});
  it('rejects invalid time, duplicate IDs and negative device rates',()=>{
    const r=new SpatialRuntime(world,[projector()]);r.submit(point());expect(()=>r.submit(point())).toThrow();r.advance(0.3);expect(()=>r.advance(0.2)).toThrow();expect(()=>r.submit(point('late'))).toThrow();expect(()=>new SpatialRuntime(world,[new MonitorBackend({id:'m',surfaceId:'wall',width:2,height:2,refreshHz:-1,latency:0})])).toThrow();
  });
});
