"""Generate a standalone viewer from Python without Node or a Python UI package."""
import json
from importlib.resources import files
from pathlib import Path
from .scenario import world_dict


def viewer_source():
    return files('spatialgl').joinpath('viewer.html').read_text(encoding='utf-8')


def write_view(path, state, world, *, settings=None):
    """Save an interactive 3D camera view of one C++ observation as standalone HTML."""
    payload = {'schema_version': 2, 'world': world_dict(world),
               'state': state.to_dict(), 'settings': settings or {}}
    encoded = json.dumps(payload, allow_nan=False).replace('<', '\\u003c')
    result = viewer_source().replace('/*__SPATIALGL_DATA__*/null', encoded)
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(result, encoding='utf-8')
    return path
