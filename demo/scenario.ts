import { DroneBackend, MonitorBackend, ProjectorBackend, SpatialRuntime } from '../src/index.js';
import type { Primitive, SceneFrame, World } from '../src/index.js';
export interface Settings { projector: boolean; monitor: boolean; drone: boolean; occlusion: boolean; count: number; speed: number; refresh: number; latency: number; sceneHz: number; scenario: string }
export const defaults: Settings={projector:true,monitor:true,drone:true,occlusion:false,count:3,speed:1.2,refresh:30,latency:0.08,sceneHz:10,scenario:'mixed'};
export function makeWorld(occlusion: boolean): World {
  return {surfaces:[{id:'wall',origin:[-3,0,-2],u:[6,0,0],v:[0,3,0]}, {id:'monitor',origin:[1.25,0.5,-1.5],u:[1.5,0,0],v:[0,1,0]}],
    occluders:occlusion?[{id:'box',min:[-0.55,0,-0.1],max:[0.55,2.5,0.5]}]:[]};
}
export function makeRuntime(s: Settings): SpatialRuntime {
  const backends=[];
  if(s.projector) for(const [id,x] of [['projector-left',-2],['projector-right',2]] as const) backends.push(new ProjectorBackend({id,position:[x,2.7,2],target:[0,1.4,-2],up:[0,1,0],fovY:Math.PI/2,near:0.1,far:12,width:640,height:360,refreshHz:s.refresh,latency:s.latency}));
  if(s.monitor) backends.push(new MonitorBackend({id:'monitor',surfaceId:'monitor',width:320,height:180,refreshHz:s.refresh,latency:s.latency}));
  if(s.drone) backends.push(new DroneBackend({id:'drone-swarm',count:s.count,speed:s.speed,home:[0,0.2,1],min:[-3,0,-2],max:[3,3,3],refreshHz:s.refresh,latency:s.latency}));
  return new SpatialRuntime(makeWorld(s.occlusion),backends);
}
export function makeScene(t: number, revision: number, scenario: string): SceneFrame {
  const primitives: Primitive[]=[];
  if(scenario==='mixed' || scenario==='wall') {
    const x=Math.sin(t)*1.1;
    primitives.push({id:'wall-patch',kind:'patch',origin:[x-0.5,1.1,-2],u:[1,0,0],v:[0,0.6,0],surfaceId:'wall',color:[0.2,0.85,0.9]},
      {id:'wall-line',kind:'polyline',vertices:[[-2.5,0.6,-2],[-1.8,1,-2],[-2.1,1.3,-2]],surfaceId:'wall',color:[0.2,0.85,0.9]},
      {id:'screen-patch',kind:'patch',origin:[1.45,0.7,-1.5],u:[1.05,0,0],v:[0,0.6,0],surfaceId:'monitor',color:[0.5,0.7,1]});
  }
  if(scenario==='mixed' || scenario==='air') for(let i=0;i<3;i++) {
    const angle=t*0.8+i*Math.PI*2/3;
    primitives.push({id:`air-${i}`,kind:'point',position:[Math.cos(angle)*1.2,1.5+Math.sin(t+i)*0.25,0.7+Math.sin(angle)*0.6],color:[1,0.65,0.2]});
  }
  if(scenario==='unsupported') primitives.push({id:'air-patch',kind:'patch',origin:[-0.5,1.2,0.5],u:[1,0,0],v:[0,0.6,0],color:[1,0.3,0.4]});
  return {id:`scene-${revision}`,presentAt:t,expiresAt:t+0.5,primitives};
}
