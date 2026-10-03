"""Application scenes and demo settings. Physical realization runs in C++."""
from dataclasses import asdict, dataclass
from math import cos, sin, pi
from . import Drones, Monitor, Occluder, Patch, Point, Polyline, Projector, Runtime, SceneFrame, Surface, World


@dataclass
class Settings:
    scenario: str = 'mixed'
    projector: bool = True
    monitor: bool = True
    drone: bool = True
    occlusion: bool = False
    count: int = 3
    speed: float = 1.2
    refresh_hz: float = 30
    latency: float = 0.08
    scene_hz: float = 10

    def __post_init__(self):
        if self.scenario not in ('mixed', 'wall', 'air', 'unsupported'):
            raise ValueError('Unknown scenario')
        for key in ('projector', 'monitor', 'drone', 'occlusion'):
            if not isinstance(getattr(self, key), bool):
                raise ValueError(f'{key} must be a boolean')
        if not isinstance(self.count, int) or isinstance(self.count, bool) or not 1 <= self.count <= 12:
            raise ValueError('count must be an integer in [1,12]')
        for key, lower, upper in [('speed', 0.1, 10), ('refresh_hz', 1, 120),
                                  ('scene_hz', 1, 60), ('latency', 0, 2)]:
            value = getattr(self, key)
            if isinstance(value, bool) or not isinstance(value, (int, float)) or not lower <= value <= upper:
                raise ValueError(f'Invalid {key}')


def make_world(occlusion=False):
    return World([
        Surface('wall', (-3, 0, -2), (6, 0, 0), (0, 3, 0)),
        Surface('monitor', (1.25, 0.5, -1.5), (1.5, 0, 0), (0, 1, 0)),
    ], [Occluder('box', (-1.1, 0, -0.8), (1.1, 2.5, 0.5))] if occlusion else [])


def make_scene(t, revision, scenario):
    primitives = []
    if scenario in ('mixed', 'wall'):
        x = sin(t) * 1.1
        primitives.extend([
            Patch('wall-patch', (x - 0.5, 1.1, -2), (1, 0, 0), (0, 0.6, 0), (0.2, 0.85, 0.9), 'wall'),
            Polyline('wall-line', [(-2.5, 0.6, -2), (-1.8, 1, -2), (-2.1, 1.3, -2)], (0.2, 0.85, 0.9), 'wall'),
            Patch('screen-patch', (1.45, 0.7, -1.5), (1.05, 0, 0), (0, 0.6, 0), (0.5, 0.7, 1), 'monitor'),
        ])
    if scenario in ('mixed', 'air'):
        for i in range(3):
            a = t * 0.8 + i * pi * 2 / 3
            primitives.append(Point(f'air-{i}', (cos(a) * 1.2, 1.5 + sin(t + i) * 0.25,
                                                0.7 + sin(a) * 0.6), (1, 0.65, 0.2)))
    if scenario == 'unsupported':
        primitives.append(Patch('air-patch', (-0.5, 1.2, 0.5), (1, 0, 0), (0, 0.6, 0), (1, 0.3, 0.4)))
    return SceneFrame(f'scene-{revision}', t, t + 0.5, primitives)


def world_dict(world):
    return {
        'surfaces': [{'id': s.id, 'origin': s.origin, 'u': s.u, 'v': s.v} for s in world.surfaces],
        'occluders': [{'id': b.id, 'min': b.min, 'max': b.max} for b in world.occluders],
    }


class Experiment:
    def __init__(self, settings=None):
        self.settings = settings or Settings()
        s = self.settings
        self.world = make_world(s.occlusion)
        backends = []
        if s.projector:
            for id, x in [('projector-left', -2), ('projector-right', 2)]:
                backends.append(Projector(id, position=(x, 2.7, 2), target=(0, 1.4, -2),
                                          width=640, height=360, refresh_hz=s.refresh_hz, latency=s.latency))
        if s.monitor:
            backends.append(Monitor('monitor', surface_id='monitor', width=320, height=180,
                                    refresh_hz=s.refresh_hz, latency=s.latency))
        if s.drone:
            backends.append(Drones('drone-swarm', count=s.count, speed=s.speed,
                                   min=(-3, 0, -2), max=(3, 3, 3),
                                   refresh_hz=s.refresh_hz, latency=s.latency))
        self.runtime = Runtime(self.world, backends)
        self.time, self.revision = 0.0, 1
        self.next_scene = 1 / s.scene_hz
        self.runtime.submit(make_scene(0, 0, s.scenario))
        self.state = self.runtime.advance(0)
        self.trace = [self.state.to_dict()]

    def step(self, dt):
        if isinstance(dt, bool) or not isinstance(dt, (int, float)) or not 0 <= dt <= 1:
            raise ValueError('dt must be in [0,1] seconds')
        end = self.time + dt
        while self.next_scene <= end:
            self.runtime.advance(self.next_scene)
            self.runtime.submit(make_scene(self.next_scene, self.revision, self.settings.scenario))
            self.revision += 1
            self.next_scene = self.revision / self.settings.scene_hz
        self.time = end
        self.state = self.runtime.advance(end)
        self.trace.append(self.state.to_dict())
        self.trace = self.trace[-300:]
        return self.state

    def payload(self):
        return {'schema_version': 2, 'settings': asdict(self.settings),
                'world': world_dict(self.world), 'state': self.state.to_dict()}
