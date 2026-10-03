# Validation · 2026-10-03

## Project checks

`npm run check` passed: **20 tests**, strict TypeScript checking, Vite demo build, and declaration-emitting headless library build.

The tests cover world support, projector pixels and quantization, frustum clipping, source-to-target occlusion, multiple-projector fallback, monitor preference, raster pixel conflict, drone capacity and primitive compatibility, presentation time and latency, bounded motion, step-size consistency, stale provenance, expiry, full replacement clears, superseded-scene expiry, removed-but-still-emitting outputs, independent device clocks, a fourth custom command schema, calibration validation, and malformed time/device inputs.

`npm run simulate` passed. A point emitter starts pending, moves at 1 m/s after 100 ms delivery latency, reaches its target at 1.1 s, and has no visible output after the scene expires at 3 s.

## Browser checks

The actual local demo was opened in the Codex in-app browser. The 3D room, surfaces, projector sources, desired samples, actual samples, position errors, diagnostics, and pause/step controls rendered correctly.

- A free-space Patch produced 40 unrealizable samples with backend-specific reasons.
- Three free-space Points with one drone produced two capacity failures.
- The default mixed scene routed wall graphics to projectors, screen graphics to the monitor, and airborne Points to the drone backend.
- The record-save button wrote `spatialgl-trace.json` into Downloads. Its parsed JSON contained `schemaVersion: 1` and 125 observations. Browser download-event automation timed out, but the actual saved file was verified directly.
- A screenshot of the final demo is saved locally at `.artifacts/simulator.jpg` (generated, gitignored).

The visual checks cover the desktop viewport. Narrow-screen layout is provided by CSS but was not separately inspected.

## Workspace integration

SpatialGL is registered in `workspace.yaml` as an active local repository without a remote URL. The required active-cluster task rules were added, and the rules-file count test was intentionally updated from 11 to 12.

The mandatory `scripts/test-workspace-tools` completed successfully in an isolated Python environment with pytest, PyYAML, and editable blacksmith/anvil/foreman dependencies. Earlier runs exposed missing local Python dependencies and the initially absent SpatialGL rules file; both were resolved before the passing run.

## Limits

The Vite build emits a bundle-size advisory for the Three.js viewer (~576 kB minified, ~145 kB gzipped); the build succeeds. The headless library imports no Three.js code. These checks validate the geometry/time model and interfaces, not physical hardware performance or optical/flight fidelity.
