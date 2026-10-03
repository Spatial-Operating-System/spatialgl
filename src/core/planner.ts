import type { Backend, Candidate, Plan, Sample, Unmet, World } from './types.js';
/** Deterministic capacity-aware routing. Device ID breaks equal-score ties. */
export function plan(samples: readonly Sample[], backends: readonly Backend[], world: World): Plan {
  const assignments=new Map<string,Candidate[]>(backends.map(b=>[b.descriptor.id,[]]));
  const unmet: Unmet[]=[];
  for(const sample of samples) {
    const attempts: Unmet['attempts'][number][]=[];
    const candidates: { backend: Backend; candidate: Candidate }[]=[];
    for(const backend of backends) {
      const result=backend.evaluate(sample,world);
      if('reason' in result) attempts.push({deviceId:backend.descriptor.id,reason:result.reason});
      else candidates.push({backend,candidate:result.candidate});
    }
    candidates.sort((a,b)=>b.candidate.score-a.candidate.score || a.backend.descriptor.id.localeCompare(b.backend.descriptor.id));
    let assigned=false;
    for(const {backend,candidate} of candidates) {
      const list=assignments.get(backend.descriptor.id)!;
      if(list.length>=backend.descriptor.capacity) { attempts.push({deviceId:backend.descriptor.id,reason:'capacity'}); continue; }
      if(candidate.pixel && list.some(c=>c.pixel?.[0]===candidate.pixel![0] && c.pixel?.[1]===candidate.pixel![1])) { attempts.push({deviceId:backend.descriptor.id,reason:'pixel-conflict'}); continue; }
      list.push(candidate); assigned=true; break;
    }
    if(!assigned) unmet.push({sample,attempts:attempts.length?attempts:[{deviceId:'none',reason:'no-device'}]});
  }
  return {assignments,unmet};
}
