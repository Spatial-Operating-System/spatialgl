import { add, blocked, cross, dot, finiteVec, scale, sub, surfaceUV, unit } from '../core/math.js';
import type { Backend, Candidate, DeviceDescriptor, DeviceFrame, Feasibility, Observation, Sample, Vec3, World } from '../core/types.js';
interface RasterConfig { id: string; refreshHz: number; latency: number; width: number; height: number }
export interface ProjectorConfig extends RasterConfig { position: Vec3; target: Vec3; up: Vec3; fovY: number; near: number; far: number }
export interface MonitorConfig extends RasterConfig { surfaceId: string }
abstract class RasterBackend implements Backend {
  abstract readonly descriptor: DeviceDescriptor;
  protected frame: DeviceFrame | null=null;
  constructor(readonly config: RasterConfig) {
    if(!Number.isInteger(config.width) || !Number.isInteger(config.height) || config.width<2 || config.height<2) throw new Error('Invalid raster dimensions');
  }
  abstract evaluate(sample: Sample,world: World): Feasibility;
  encode(candidates: readonly Candidate[]): DeviceFrame['command'] {
    return {kind:'raster-samples',width:this.config.width,height:this.config.height,samples:candidates.map(c=>({sampleId:c.sample.id,pixel:c.pixel!,color:c.sample.color,position:c.position}))};
  }
  advance(_dt: number): void {}
  apply(frame: DeviceFrame): void { this.frame=frame; }
  observe(now: number): readonly Observation[] {
    if(!this.frame || now>=this.frame.expiresAt || this.frame.command.kind!=='raster-samples') return [];
    return this.frame.command.samples.map(s=>({sampleId:s.sampleId,position:s.position,color:s.color}));
  }
  reset(): void { this.frame=null; }
}
export class MonitorBackend extends RasterBackend {
  readonly descriptor: DeviceDescriptor;
  constructor(readonly monitor: MonitorConfig) {
    super(monitor); this.descriptor={id:monitor.id,modality:'screen',refreshHz:monitor.refreshHz,latency:monitor.latency,capacity:Infinity};
  }
  evaluate(sample: Sample,world: World): Feasibility {
    const s=world.surfaces.find(s=>s.id===this.monitor.surfaceId);
    if(!s || (sample.surfaceId && sample.surfaceId!==s.id)) return {reason:'no-support-surface'};
    const uv=surfaceUV(sample.position,s);
    if(!uv) return {reason:'outside-support'};
    const pixel=[Math.round(uv[0]*(this.config.width-1)),Math.round(uv[1]*(this.config.height-1))] as const;
    const position=add(s.origin,add(scale(s.u,pixel[0]/(this.config.width-1)),scale(s.v,pixel[1]/(this.config.height-1))));
    return {candidate:{sample,score:100,position,pixel}};
  }
}
export class ProjectorBackend extends RasterBackend {
  readonly descriptor: DeviceDescriptor;
  constructor(readonly projector: ProjectorConfig) {
    super(projector);
    if([projector.position,projector.target,projector.up].some(v=>!finiteVec(v)) || [projector.fovY,projector.near,projector.far].some(v=>!Number.isFinite(v)) || projector.fovY<=0 || projector.fovY>=Math.PI || projector.near<=0 || projector.far<=projector.near || dot(sub(projector.target,projector.position),sub(projector.target,projector.position))===0 || dot(cross(sub(projector.target,projector.position),projector.up),cross(sub(projector.target,projector.position),projector.up))===0) throw new Error('Invalid projector calibration');
    this.descriptor={id:projector.id,modality:'projection',refreshHz:projector.refreshHz,latency:projector.latency,capacity:Infinity};
  }
  evaluate(sample: Sample,world: World): Feasibility {
    const p=this.projector;
    const s=world.surfaces.find(s=>(!sample.surfaceId || s.id===sample.surfaceId) && surfaceUV(sample.position,s));
    if(!s) return {reason:'no-support-surface'};
    const forward=unit(sub(p.target,p.position)), right=unit(cross(forward,p.up)), up=cross(right,forward);
    const d=sub(sample.position,p.position), z=dot(d,forward), h=Math.tan(p.fovY/2), aspect=p.width/p.height;
    const x=dot(d,right)/(z*h*aspect), y=dot(d,up)/(z*h);
    if(z<p.near || z>p.far || Math.abs(x)>1 || Math.abs(y)>1) return {reason:'outside-frustum'};
    if(blocked(p.position,sample.position,world.occluders)) return {reason:'occluded'};
    const pixel=[Math.round((x+1)*0.5*(p.width-1)),Math.round((1-y)*0.5*(p.height-1))] as const;
    const ray=add(forward,add(scale(right,(pixel[0]/(p.width-1)*2-1)*h*aspect),scale(up,(1-pixel[1]/(p.height-1)*2)*h)));
    const normal=cross(s.u,s.v), denominator=dot(ray,normal);
    if(Math.abs(denominator)<1e-12) return {reason:'outside-support'};
    const position=add(p.position,scale(ray,dot(sub(s.origin,p.position),normal)/denominator));
    if(!surfaceUV(position,s)) return {reason:'outside-support'};
    if(blocked(p.position,position,world.occluders)) return {reason:'occluded'};
    return {candidate:{sample,score:50-z,position,pixel}};
  }
}
