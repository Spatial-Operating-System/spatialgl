/** Right-handed world coordinates in metres, +Y up; all times are seconds. */
export type Vec3 = readonly [number, number, number];
export type RGB = readonly [number, number, number];
interface BasePrimitive { id: string; color: RGB; surfaceId?: string }
export type Primitive =
  | (BasePrimitive & { kind: 'point'; position: Vec3 })
  | (BasePrimitive & { kind: 'polyline'; vertices: readonly Vec3[] })
  | (BasePrimitive & { kind: 'patch'; origin: Vec3; u: Vec3; v: Vec3 });
export interface SceneFrame {
  id: string;
  presentAt: number;
  expiresAt: number;
  primitives: readonly Primitive[];
}
/** Finite rectangular support. u and v are perpendicular edge vectors. */
export interface Surface { id: string; origin: Vec3; u: Vec3; v: Vec3 }
export interface Occluder { id: string; min: Vec3; max: Vec3 }
export interface World { surfaces: readonly Surface[]; occluders: readonly Occluder[] }
export interface Sample {
  id: string; primitiveId: string; kind: Primitive['kind'];
  position: Vec3; color: RGB; surfaceId?: string;
}
export type FailureReason = 'unsupported-primitive' | 'no-support-surface' | 'outside-support'
  | 'outside-frustum' | 'occluded' | 'capacity' | 'pixel-conflict' | 'no-device';
export interface Candidate {
  sample: Sample; score: number; position: Vec3;
  pixel?: readonly [number, number];
}
export type Feasibility = { candidate: Candidate } | { reason: FailureReason };
export interface DeviceDescriptor {
  id: string; modality: 'projection' | 'screen' | 'emitter';
  refreshHz: number; latency: number; capacity: number;
}
export interface RasterCommand {
  kind: 'raster-samples'; width: number; height: number;
  samples: readonly { sampleId: string; pixel: readonly [number, number]; color: RGB; position: Vec3 }[];
}
export interface EmitterCommand {
  kind: 'emitter-targets'; targets: readonly { sampleId: string; position: Vec3; color: RGB }[];
}
/** A device command batch, not a globally synchronous image. */
export interface DeviceFrame {
  deviceId: string; sceneId: string | null; issuedAt: number; applyAt: number;
  expiresAt: number; command: RasterCommand | EmitterCommand | { kind: 'extension'; schema: string; payload: unknown };
}
export interface Observation { sampleId: string; position: Vec3; color: RGB }
/** Backend-local mutable state is owned by an adapter instance. */
export interface Backend {
  readonly descriptor: DeviceDescriptor;
  evaluate(sample: Sample, world: World): Feasibility;
  encode(candidates: readonly Candidate[]): DeviceFrame['command'];
  advance(dt: number): void;
  apply(frame: DeviceFrame): void;
  observe(now: number): readonly Observation[];
  reset(): void;
}
export interface Unmet { sample: Sample; attempts: readonly { deviceId: string; reason: FailureReason }[] }
export interface Plan { assignments: ReadonlyMap<string, readonly Candidate[]>; unmet: readonly Unmet[] }
export interface RealizedSample {
  sample: Sample; deviceId: string | null; actual: Vec3 | null;
  error: number | null; status: 'realized' | 'tracking' | 'pending' | 'unrealizable';
  sourceSceneId: string | null; age: number | null; reasons: readonly string[];
}
export interface ObservedOutput extends Observation { deviceId: string; sourceSceneId: string | null; age: number; stale: boolean }
export interface RealizationState {
  time: number; sceneId: string | null; samples: readonly RealizedSample[];
  deviceFrames: readonly DeviceFrame[];
  outputs: readonly ObservedOutput[];
}
