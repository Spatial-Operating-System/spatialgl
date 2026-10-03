import type { Occluder, Surface, Vec3 } from './types.js';
export const finiteVec = (v: Vec3): boolean => v.length===3 && v.every(Number.isFinite);
export const add = (a: Vec3, b: Vec3): Vec3 => [a[0]+b[0], a[1]+b[1], a[2]+b[2]];
export const sub = (a: Vec3, b: Vec3): Vec3 => [a[0]-b[0], a[1]-b[1], a[2]-b[2]];
export const scale = (a: Vec3, n: number): Vec3 => [a[0]*n, a[1]*n, a[2]*n];
export const dot = (a: Vec3, b: Vec3): number => a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
export const length = (a: Vec3): number => Math.sqrt(dot(a,a));
export const distance = (a: Vec3, b: Vec3): number => length(sub(a,b));
export const unit = (a: Vec3): Vec3 => scale(a, 1/length(a));
export const cross = (a: Vec3,b: Vec3): Vec3 => [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]];
export function surfaceUV(p: Vec3, s: Surface): readonly [number,number] | null {
  const d = sub(p,s.origin);
  const u = dot(d,s.u)/dot(s.u,s.u), v = dot(d,s.v)/dot(s.v,s.v);
  const reconstructed = add(s.origin,add(scale(s.u,u),scale(s.v,v)));
  return distance(p,reconstructed) <= 1e-6 && u >= -1e-8 && u <= 1+1e-8 && v >= -1e-8 && v <= 1+1e-8 ? [u,v] : null;
}
/** Slab intersection with the open segment, so boxes beyond the target do not occlude. */
export function blocked(a: Vec3,b: Vec3, boxes: readonly Occluder[]): boolean {
  const d = sub(b,a);
  return boxes.some(box => {
    let near = 1e-7, far = 1-1e-7;
    for (let k=0;k<3;k++) {
      if (Math.abs(d[k]) < 1e-12) { if(a[k]<box.min[k] || a[k]>box.max[k]) return false; }
      else {
        const x=(box.min[k]-a[k])/d[k], y=(box.max[k]-a[k])/d[k];
        near=Math.max(near,Math.min(x,y)); far=Math.min(far,Math.max(x,y));
        if(near>far) return false;
      }
    }
    return true;
  });
}
