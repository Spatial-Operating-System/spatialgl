import { distance } from './math.js';
import { plan } from './planner.js';
import { sampleScene, validateFrame, validateWorld } from './scene.js';
import type { Backend, DeviceFrame, RealizationState, SceneFrame, World } from './types.js';
/** Event-driven simulation: device refresh and command delivery use one monotonic clock. */
export class SpatialRuntime {
  private time=0;
  private frames: SceneFrame[]=[];
  private refreshCounts=new Map<string,number>();
  private pending: DeviceFrame[]=[];
  private applied=new Map<string,DeviceFrame>();
  constructor(readonly world: World, readonly backends: readonly Backend[], readonly spacing=0.15, readonly tolerance=0.02) {
    validateWorld(world);
    if(!Number.isFinite(spacing) || spacing<=0 || !Number.isFinite(tolerance) || tolerance<0) throw new Error('Invalid sampling or tolerance');
    const ids=new Set<string>();
    for(const b of backends) {
      const d=b.descriptor;
      if(!d.id || ids.has(d.id) || !Number.isFinite(d.refreshHz) || d.refreshHz<=0 || !Number.isFinite(d.latency) || d.latency<0 || !(d.capacity>=0)) throw new Error('Invalid device descriptor');
      ids.add(d.id); b.reset(); this.refreshCounts.set(d.id,0);
    }
  }
  submit(frame: SceneFrame): void {
    validateFrame(frame);
    if(frame.presentAt<this.time || this.frames.some(f=>f.id===frame.id)) throw new Error('Scene must have a unique ID and cannot be submitted in the past');
    this.frames.push(structuredClone(frame));
  }
  private sceneAt(time: number): SceneFrame | null {
    const latest=this.frames.filter(f=>f.presentAt<=time).sort((a,b)=>b.presentAt-a.presentAt || this.frames.indexOf(b)-this.frames.indexOf(a))[0];
    return latest && time<latest.expiresAt ? latest : null;
  }
  advance(to: number): RealizationState {
    if(!Number.isFinite(to) || to<this.time) throw new Error('Clock must advance monotonically');
    while(true) {
      const refresh=Math.min(...this.backends.map(b=>this.refreshCounts.get(b.descriptor.id)!/b.descriptor.refreshHz));
      const delivery=Math.min(...this.pending.map(f=>f.applyAt));
      const next=Math.min(refresh,delivery);
      if(next>to || !Number.isFinite(next)) break;
      this.backends.forEach(b=>b.advance(next-this.time)); this.time=next;
      // Refreshes enqueue full replacement commands, including empty frames.
      const scene=this.sceneAt(next);
      const routed=plan(scene?sampleScene(scene.primitives,this.spacing):[],this.backends,this.world);
      for(const b of this.backends) {
        const id=b.descriptor.id, count=this.refreshCounts.get(id)!;
        if(Math.abs(count/b.descriptor.refreshHz-next)<1e-9) {
          this.pending.push({deviceId:id,sceneId:scene?.id??null,issuedAt:next,applyAt:next+b.descriptor.latency,expiresAt:scene?.expiresAt??next,command:b.encode(routed.assignments.get(id)!)});
          this.refreshCounts.set(id,count+1);
        }
      }
      const due=this.pending.filter(f=>f.applyAt<=next);
      this.pending=this.pending.filter(f=>f.applyAt>next);
      for(const f of due) { this.backends.find(b=>b.descriptor.id===f.deviceId)!.apply(f); this.applied.set(f.deviceId,f); }
    }
    this.backends.forEach(b=>b.advance(to-this.time)); this.time=to;
    return this.snapshot();
  }
  snapshot(): RealizationState {
    const scene=this.sceneAt(this.time), samples=scene?sampleScene(scene.primitives,this.spacing):[];
    const routed=plan(samples,this.backends,this.world);
    const outputs=this.backends.flatMap(b=>{
      const f=this.applied.get(b.descriptor.id);
      return b.observe(this.time).map(o=>({...o,deviceId:b.descriptor.id,sourceSceneId:f?.sceneId??null,age:this.time-(f?.issuedAt??this.time),stale:f?.sceneId!==scene?.id}));
    });
    return {time:this.time,sceneId:scene?.id??null,outputs,deviceFrames:[...this.applied.values()],samples:samples.map(sample=>{
      const unmet=routed.unmet.find(u=>u.sample.id===sample.id);
      const backend=this.backends.find(b=>routed.assignments.get(b.descriptor.id)?.some(c=>c.sample.id===sample.id));
      const frame=backend?this.applied.get(backend.descriptor.id):undefined;
      const observation=backend?.observe(this.time).find(o=>o.sampleId===sample.id);
      const error=observation?distance(sample.position,observation.position):null;
      return {sample,deviceId:backend?.descriptor.id??null,actual:observation?.position??null,error,
        status:unmet?'unrealizable' as const:!observation?'pending' as const:error!<=this.tolerance?'realized' as const:'tracking' as const,
        sourceSceneId:frame?.sceneId??null,age:frame?this.time-frame.issuedAt:null,reasons:unmet?.attempts.map(a=>`${a.deviceId}: ${a.reason}`)??[]};
    })};
  }
}
