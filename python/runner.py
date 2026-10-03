"""Run user Python scripts or a REPL with the matching native-extension ABI."""
import code
import os
from pathlib import Path
import runpy
import sys
import spatialgl


def main():
    workdir = Path(os.environ.get('BUILD_WORKING_DIRECTORY', os.getcwd()))
    os.chdir(workdir)
    if len(sys.argv) > 1:
        script = Path(sys.argv[1]).resolve()
        sys.argv = [str(script), *sys.argv[2:]]
        sys.path.insert(0, str(script.parent))
        runpy.run_path(str(script), run_name='__main__')
    else:
        code.interact(banner='SpatialGL Python · C++20 core (import spatialgl as sgl)',
                      local={'spatialgl': spatialgl, 'sgl': spatialgl})


if __name__ == '__main__':
    main()
