import { DroneBackend, SpatialRuntime } from '../dist/lib/index.js';
const backend=new DroneBackend({id:'drone',count:1,speed:1,home:[0,1,0],min:[-3,0,-3],max:[3,3,3],refreshHz:10,latency:0.1});
const runtime=new SpatialRuntime({surfaces:[],occluders:[]},[backend]);
runtime.submit({id:'hello-space',presentAt:0,expiresAt:3,primitives:[{id:'light',kind:'point',position:[1,1,0],color:[1,0.7,0.2]}]});
for(const t of [0,0.1,0.5,1,1.1,3]) {
  const state=runtime.advance(t);
  console.log(JSON.stringify({time:t,sceneId:state.sceneId,samples:state.samples.map(s=>({id:s.sample.id,status:s.status,actual:s.actual,error:s.error})),outputs:state.outputs.length}));
}
