import { add, distance, dot, finiteVec, scale } from './math.js';
import type { Primitive, Sample, SceneFrame, Vec3, World } from './types.js';
export function validateFrame(frame: SceneFrame): void {
  if (!frame.id || !Number.isFinite(frame.presentAt) || !Number.isFinite(frame.expiresAt) || frame.presentAt < 0 || frame.expiresAt<=frame.presentAt) throw new Error('Invalid scene interval');
  const ids=new Set<string>();
  for(const p of frame.primitives) {
    if(!p.id || ids.has(p.id)) throw new Error('Primitive IDs must be unique'); ids.add(p.id);
    if(p.color.length!==3 || p.color.some(c=>!Number.isFinite(c) || c<0 || c>1)) throw new Error('RGB must be in [0,1]');
    const vectors=p.kind==='point' ? [p.position] : p.kind==='polyline' ? p.vertices : [p.origin,p.u,p.v];
    if(vectors.some(v=>!finiteVec(v))) throw new Error('Non-finite geometry');
    if(p.kind==='polyline' && p.vertices.length<2) throw new Error('Polyline requires two vertices');
    if(p.kind==='patch' && (dot(p.u,p.u)===0 || dot(p.v,p.v)===0 || Math.abs(dot(p.u,p.v))>1e-6)) throw new Error('Patch requires nonzero perpendicular edges');
  }
}
export function validateWorld(world: World): void {
  const ids=new Set<string>();
  for(const s of world.surfaces) {
    if(!s.id || ids.has(s.id)) throw new Error('Surface IDs must be unique'); ids.add(s.id);
    if([s.origin,s.u,s.v].some(v=>!finiteVec(v)) || dot(s.u,s.u)===0 || dot(s.v,s.v)===0 || Math.abs(dot(s.u,s.v))>1e-6) throw new Error('Invalid surface basis');
  }
  for(const b of world.occluders) if(!finiteVec(b.min) || !finiteVec(b.max) || b.min.some((v,i)=>v>b.max[i])) throw new Error('Invalid occluder');
}
/** Fixed sample IDs preserve correspondence across scene revisions with the same topology. */
export function sampleScene(primitives: readonly Primitive[], spacing=0.15): Sample[] {
  if(!Number.isFinite(spacing) || spacing<=0) throw new Error('Invalid spacing');
  const result: Sample[]=[];
  for(const p of primitives) {
    const emit=(position: Vec3)=>result.push({id:`${p.id}:${result.length-start}`,primitiveId:p.id,kind:p.kind,position,color:p.color,surfaceId:p.surfaceId});
    const start=result.length;
    if(p.kind==='point') emit(p.position);
    else if(p.kind==='polyline') {
      for(let i=1;i<p.vertices.length;i++) {
        const a=p.vertices[i-1], b=p.vertices[i];
        const n=Math.max(1,Math.ceil(distance(a,b)/spacing));
        for(let j=i===1?0:1;j<=n;j++) emit(add(scale(a,1-j/n),scale(b,j/n)));
      }
    } else {
      const nu=Math.max(1,Math.ceil(Math.sqrt(dot(p.u,p.u))/spacing));
      const nv=Math.max(1,Math.ceil(Math.sqrt(dot(p.v,p.v))/spacing));
      for(let i=0;i<=nu;i++) for(let j=0;j<=nv;j++) emit(add(p.origin,add(scale(p.u,i/nu),scale(p.v,j/nv))));
    }
  }
  return result;
}
