# SpatialGL

Independent repository in the dostos workspace. Keep implementation and code-adjacent contracts here; register ownership in the workspace index only.

- Core is dependency-free TypeScript; Three.js belongs only to the demo.
- Metres, seconds, right-handed coordinates with +Y up. Never silently snap world geometry onto another support surface.
- Preserve SceneFrame / DeviceFrame / RealizationState separation. A scene frame is desired state, not a device image or guarantee of atomic physical presentation.
- Add rendering techniques behind Backend; expose unsupported geometry, support constraints and observed error.
- Simulator assumptions must be distinguished from measured hardware performance.
- Run `npm run check` for contract/backend/runtime changes. Check the browser for demo changes.
- Hardware drivers, autonomous device movement and real-world safety policies are future work; this repository currently runs simulation only.
