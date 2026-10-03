"""Python-owned interactive lab. All realization calculations run in C++."""
import argparse
import json
import os
from pathlib import Path
from dataclasses import asdict
from http.server import BaseHTTPRequestHandler, HTTPServer
from spatialgl.scenario import Experiment, Settings
from spatialgl.view import viewer_source, write_view


def make_handler(initial=None):
    experiment = initial or Experiment()

    class Handler(BaseHTTPRequestHandler):
        def send(self, body, content_type='application/json', code=200, download=False):
            encoded = body.encode('utf-8')
            self.send_response(code)
            self.send_header('Content-Type', content_type + '; charset=utf-8')
            self.send_header('Content-Length', str(len(encoded)))
            self.send_header('Cache-Control', 'no-store')
            if download:
                self.send_header('Content-Disposition', 'attachment; filename="spatialgl-trace.json"')
            self.end_headers()
            self.wfile.write(encoded)

        def do_GET(self):
            if self.path == '/':
                self.send(viewer_source(), 'text/html')
            elif self.path == '/api/state':
                self.send(json.dumps(experiment.payload(), allow_nan=False))
            elif self.path == '/api/trace':
                self.send(json.dumps({'schema_version': 2, 'settings': asdict(experiment.settings),
                                      'world': experiment.payload()['world'], 'trace': experiment.trace},
                                     allow_nan=False), download=True)
            else:
                self.send(json.dumps({'error': 'Not found'}), code=404)

        def do_POST(self):
            nonlocal experiment
            try:
                length = int(self.headers.get('Content-Length', 0))
                if not 0 < length <= 65536:
                    raise ValueError('Invalid request size')
                data = json.loads(self.rfile.read(length))
                if not isinstance(data, dict):
                    raise ValueError('Expected a JSON object')
                if self.path == '/api/reset':
                    experiment = Experiment(Settings(**data))
                elif self.path == '/api/step':
                    if set(data) != {'dt'}:
                        raise ValueError('Expected dt')
                    experiment.step(data['dt'])
                else:
                    self.send(json.dumps({'error': 'Not found'}), code=404)
                    return
                self.send(json.dumps(experiment.payload(), allow_nan=False))
            except (ValueError, TypeError) as error:
                self.send(json.dumps({'error': str(error)}), code=400)

        def log_message(self, *_):
            pass

    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=5188)
    parser.add_argument('--output', help='Save a standalone snapshot HTML instead of serving')
    args = parser.parse_args()
    if args.output:
        e = Experiment()
        for _ in range(60):
            e.step(0.05)
        output = Path(args.output)
        if not output.is_absolute():
            output = Path(os.environ.get('BUILD_WORKING_DIRECTORY', os.getcwd())) / output
        print(write_view(output, e.state, e.world, settings=asdict(e.settings)))
        return
    with HTTPServer(('127.0.0.1', args.port), make_handler()) as server:
        print(f'SpatialGL Python lab: http://127.0.0.1:{args.port}/', flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == '__main__':
    main()
