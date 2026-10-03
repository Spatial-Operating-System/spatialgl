import { add, distance, finiteVec, scale, sub } from '../core/math.js';
import type { Backend, Candidate, DeviceDescriptor, DeviceFrame, Feasibility, Observation, Vec3, Sample, World } from '../core/types.js';
export interface DroneConfig { id: string; refreshHz: number; latency: number; count: number; speed: number; home: Vec3; min: Vec3; max: Vec3 }
export class DroneBackend implements Backend {
  readonly descriptor: DeviceDescriptor;
  private frame: DeviceFrame | null=null;
  private positions: Vec3[]=[];
  private bindings=new Map<string,number>();
  constructor(readonly config: DroneConfig) {
    if([config.home,config.min,config.max].some(v=>!finiteVec(v)) || config.min.some((v,i)=>v>config.max[i] || config.home[i]<v || config.home[i]>config.max[i]) || !Number.isInteger(config.count) || config.count<1 || !Number.isFinite(config.speed) || config.speed<=0) throw new Error('Invalid emitter dynamics');
    this.descriptor={id:config.id,modality:'emitter',refreshHz:config.refreshHz,latency:config.latency,capacity:config.count}; this.reset();
  }
  evaluate(sample: Sample,_world: World): Feasibility {
    if(sample.kind!=='point' || sample.surfaceId) return {reason:'unsupported-primitive'};
    if(sample.position.some((v,i)=>v<this.config.min[i] || v>this.config.max[i])) return {reason:'outside-support'};
    return {candidate:{sample,score:10,position:sample.position}};
  }
  encode(candidates: readonly Candidate[]): DeviceFrame['command'] { return {kind:'emitter-targets',targets:candidates.map(c=>({sampleId:c.sample.id,position:c.position,color:c.sample.color}))}; }
  advance(dt: number): void {
    if(this.frame?.command.kind!=='emitter-targets') return;
    for(const t of this.frame.command.targets) {
      const slot=this.bindings.get(t.sampleId)!;
      const p=this.positions[slot], d=distance(p,t.position);
      if(d>0) this.positions[slot]=add(p,scale(sub(t.position,p),Math.min(1,this.config.speed*dt/d)));
    }
  }
  apply(frame: DeviceFrame): void {
    if(frame.command.kind!=='emitter-targets') throw new Error('Expected emitter targets');
    const next=new Map<string,number>();
    for(const t of frame.command.targets) { const slot=this.bindings.get(t.sampleId); if(slot!==undefined) next.set(t.sampleId,slot); }
    for(const t of frame.command.targets) if(!next.has(t.sampleId)) {
      const slot=this.positions.findIndex((_,i)=>![...next.values()].includes(i));
      if(slot<0) throw new Error('Emitter capacity exceeded'); next.set(t.sampleId,slot);
    }
    this.bindings=next; this.frame=frame;
  }
  observe(now: number): readonly Observation[] {
    if(!this.frame || now>=this.frame.expiresAt || this.frame.command.kind!=='emitter-targets') return [];
    return this.frame.command.targets.map(t=>({sampleId:t.sampleId,position:this.positions[this.bindings.get(t.sampleId)!],color:t.color}));
  }
  reset(): void { this.frame=null; this.positions=Array.from({length:this.config.count},()=>[...this.config.home] as Vec3); this.bindings.clear(); }
}
