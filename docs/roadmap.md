# SpatialGL roadmap

## 0.1 · 완료: simulation baseline

세계 좌표 Point/Polyline/Patch, 유한 평면 support, static projector·monitor·point emitter backend, capacity/pixel conflict, AABB 차폐, refresh/latency/expiry, observed output와 provenance, headless 예제와 3D 비교 UI를 구현했다.

## 0.2 · 완료: C++ core / Python API / Bazel

물리 realization 구현을 C++20으로 이전했다. pybind11 Python API, Bazel C++/Python toolchain, native/user API/server 테스트, Python 사용자 스크립트·REPL, Python 실시간 실험실과 standalone 관측 뷰를 추가했다. TypeScript 구현은 Git 이력으로 보존한다.

## 0.3 · 모듈별 표면 광학과 C ABI

표면에 붙은 DisplayList, Rig와 공유 자원, 고정·조향 raster 및 ordered galvo 모델, 가상 합성과 물리 광량 합성, world/calibration revision, 취소와 장애 상태를 추가했다. C++ 구현을 기능별 Bazel 라이브러리로 분리하고 C11 ABI와 Python optics API를 연결했다. [기본 기능 계약](core-api.md), [C API](c-api.md), [모듈 구조](modules.md)에 지원 범위와 근사를 명시한다. F01–F06은 최소 시뮬레이터 시나리오로 검증하며 문헌 시스템 전체의 재현을 뜻하지 않는다.

## 다음 · 실험을 재현 가능한 단위로

- 별도 Python 환경과 notebook용 wheel 배포.
- Scene/World/device profile의 versioned JSON 입력과 저장된 trace replay.
- sampling density convergence와 analytic geometry fixtures로 coverage 오차 측정.
- full primitive 단위 allocation, quality/error/deadline 요청 및 명시적인 degradation policy.
- 장치별 profile UI와 clock drift, variable delay, dropped commands, clear acknowledgements.

수락 기준: 동일한 입력과 seed로 같은 command/observation trace를 생성하고, 미실현·시간 위반·위치 오차를 독립적으로 집계한다.

## 다음 · 다양한 물리 rendering

다중 기기 [coverage 감사](cases-and-design-coverage.md)의 F01–F06을 optical simulation의 구조 검증 묶음으로 사용한다. 공동 contribution, 공유 resource, timed program과 world/clock/calibration provenance의 기본 계약을 바탕으로 정확도와 하드웨어 지원을 확장한다. observer/eye/focus와 sensing pass, volume/audio/haptic 지원은 별도 후속 milestone이다.

- 조향 프로젝터: 가속도·관성·동적 warp를 포함하는 actuator 모델과 실측 profile.
- 이동 모니터: support transform의 갱신과 이동 비용.
- 드론/로봇 emitter: collision-free trajectory, 최소 간격, 가속도, persistent identity.
- 다중 프로젝터: 연속 raster framebuffer, footprint/반사율과 실측 photometric calibration.
- Galvo: 경로별 travel·corner dynamics, fill approximation과 실제 DAC/stream queue.
- Rig: trigger 기반 동기화, 유실·underrun과 hardware cancellation acknowledgement.
- 비평면 표면: mesh support, visibility clipping, surface coordinate mapping.

수락 기준: 동일 SceneFrame이 각 기법에서 실제로 가능한 부분, 시간 비용, 공간 오차를 비교 가능하게 보고한다. 공중 Patch를 표면 projection으로 조용히 대체하지 않는다.

## 이후 · 첫 실제 장치

우선 고정 모니터나 프로젝터 하나를 사용한다. World↔device calibration, driver transport, measured feedback, watchdog/expiry와 command acknowledgement를 구현한다. 시뮬레이션 예측과 측정 출력의 차이를 비교한다. 드론 실기는 검증된 별도 제어·안전 계층이 마련된 다음 진행한다.
