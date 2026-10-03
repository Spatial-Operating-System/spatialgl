# SpatialGL roadmap

## 0.1 · 완료: simulation baseline

세계 좌표 Point/Polyline/Patch, 유한 평면 support, static projector·monitor·point emitter backend, capacity/pixel conflict, AABB 차폐, refresh/latency/expiry, observed output와 provenance, headless 예제와 3D 비교 UI를 구현했다.

## 0.2 · 실험을 재현 가능한 단위로

- Scene/World/device profile의 versioned JSON 입력과 저장된 trace replay.
- sampling density convergence와 analytic geometry fixtures로 coverage 오차 측정.
- full primitive 단위 allocation, quality/error/deadline 요청 및 명시적인 degradation policy.
- 장치별 profile UI와 clock drift, variable delay, dropped commands, clear acknowledgements.

수락 기준: 동일한 입력과 seed로 같은 command/observation trace를 생성하고, 미실현·시간 위반·위치 오차를 독립적으로 집계한다.

## 0.3 · 다양한 물리 rendering

- 조향 프로젝터: pose의 속도/가속도 제한과 optical frustum의 시간 변화.
- 이동 모니터: support transform의 갱신과 이동 비용.
- 드론/로봇 emitter: collision-free trajectory, 최소 간격, 가속도, persistent identity.
- 다중 프로젝터: pixel framebuffer, overlap/blending, photometric calibration.
- 비평면 표면: mesh support, visibility clipping, surface coordinate mapping.

수락 기준: 동일 SceneFrame이 각 기법에서 실제로 가능한 부분, 시간 비용, 공간 오차를 비교 가능하게 보고한다. 공중 Patch를 표면 projection으로 조용히 대체하지 않는다.

## 0.4 · 첫 실제 장치

우선 고정 모니터나 프로젝터 하나를 사용한다. World↔device calibration, driver transport, measured feedback, watchdog/expiry와 command acknowledgement를 구현한다. 시뮬레이션 예측과 측정 출력의 차이를 비교한다. 드론 실기는 검증된 별도 제어·안전 계층이 마련된 다음 진행한다.
