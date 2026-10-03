# SpatialGL abstraction contract · v0.2

## 연구 경계

SpatialGL의 입력은 이미 좌표가 정해진 그래픽이다. `highlight(robot)`처럼 의미에서 객체와 위치를 찾는 작업은 상위 계층이 수행한다. 핵심 연구 질문은 “같은 공간 그래픽을 서로 다른 물리적 출력 방식에 어떤 조건으로 실현할 수 있는가?”다.

공통 분모는 픽셀이 아니라 **공간의 목표, 실현 제약, 시간, 관측**이다. 장치별 명령 형식을 하나로 강제하면 드론의 궤적과 프로젝터의 이미지를 잘못 같은 것으로 취급하게 된다.

## Primitive

| 형식 | 세계 좌표 표현 | 현재 실현 모델 |
|---|---|---|
| Point | 위치 + RGB | 표면상의 raster sample 또는 공간의 발광 드론 |
| Polyline | 두 개 이상의 꼭짓점 + RGB | 선분을 표본화해 raster backend에 전달 |
| Patch | origin + 직교하는 u/v edge + RGB | 유한 직사각형을 표본화해 raster backend에 전달 |

모든 길이는 metre, 시간은 second, 좌표는 오른손계이며 +Y가 위쪽이다. RGB는 [0,1]의 표시 색상이다. 복사휘도나 실제 광원의 에너지를 뜻하지 않는다. `surface_id`가 있으면 그 표면에만 표시할 수 있다. 없으면 backend가 정확히 그 위치를 포함하는 표면을 찾을 수 있다. 표면으로 투영하거나 위치를 자동 보정하지 않는다.

표본 간격은 기본 0.15 m이며 runtime 생성 시 지정할 수 있다. 표본 ID는 primitive ID + 생성 순서다. 장면 간 같은 토폴로지와 같은 표본 수에서만 대응이 유지된다. 기하가 변해 표본 수가 바뀌는 Polyline/Patch에는 영구적인 표면 파라미터 ID를 제공하지 않는다. 표본 사이의 가시성과 coverage는 보장하지 않는다.

## 세 종류의 frame/state

`SceneFrame`은 전체 목표 장면을 대체하는 immutable submission이다. `present_at`부터 유효하고 `expires_at`에 만료한다. 미래 장면을 미리 제출할 수 있다. 제출 시 복사한다. 동일 장면 ID, 과거 표시 시각, 잘못된 geometry는 거부한다. 해당 시각까지 제출된 가장 최근 장면이 우선하며, 같은 표시 시각이면 나중 제출이 우선한다. 새 장면이 만료해도 예전 장면을 다시 살리지 않는다.

`DeviceFrame`은 장치 하나의 명령 묶음이다. `issued_at`은 장치의 갱신 tick, `apply_at`은 고정 지연 후 도착 시각이다. `scene_id`와 `expires_at`을 보존한다. raster에서는 해상도와 픽셀·RGB·세계 위치 표본을, emitter에서는 발광 점의 목표 위치를 담는다. 새 명령은 장치의 이전 명령 전체를 대체한다. 비어 있는 장면도 clear 명령을 전달한다. 다른 물리 방식은 versioned `ExtensionCommand`의 schema/string payload로 새 명령을 추가할 수 있다.

`RealizationState`는 원하는 표본마다 실현/추적/대기/불가능, 실제 위치, 위치 오차, 명령의 원본 장면 ID, 명령 나이, 실패 이유를 제공한다. `outputs`에는 현재 목표에서 사라졌어도 아직 실제로 출력 중인 표본까지 포함한다. 이전 장면의 출력은 `stale`로 구분한다. `device_frames`는 마지막 적용 명령이며, 이미 만료된 명령 기록도 남는다. 현재 출력 여부는 `outputs`로 확인한다.

`realized` 판정은 현재 목표와 실제 위치의 거리만 평가한다. 기본 허용 오차는 2 cm다. 색 정확도·시간 deadline·지각적 동등성을 보장하지 않는다. 이전 장면의 표본이 현재 위치와 가까우면 위치 기준으로 realized일 수 있으므로 provenance와 stale도 함께 확인해야 한다.

## 실제 실행 시간

`advance(t)`는 장치 tick과 전달 이벤트를 시각 순서대로 처리한 뒤 남은 이동 시간을 적분한다. 입력 `t`는 단조 증가해야 한다. 장치별 refresh_hz와 latency를 다르게 지정할 수 있다. 브라우저의 화면 갱신 빈도는 물리 장치의 시계가 아니다. 드론의 일정 속도 이동은 각 명령 도착 사이에서 적분한다. 부동소수점 반올림 수준을 제외하면 작은 step과 큰 step의 결과가 일치한다.

장면 만료 시 출력은 어댑터의 expiry gate에서 소등된다. 실제 장치가 이 기능을 지원한다는 뜻이 아니다. 드론의 위치 이동은 마지막 명령을 계속 따라가지만 출력은 만료 시 꺼진다. 실기 연결 때는 watchdog, 취소, acknowledgement, 이동 중단과 안전 정책을 별도로 정의해야 한다.

장면 교체는 global atomic present가 아니다. 서로 다른 refresh/latency 때문에 이전 출력, 새 출력, 빈 출력이 공존할 수 있다. 현재는 고정 latency, FIFO 전달, 손실 없는 명령을 가정한다.

## Backend 확장 경계

`Backend` 구현은 다음 책임을 가진다.

1. `descriptor`: 출력 방식, 갱신률, 지연, 표본 용량을 선언한다.
2. `evaluate(sample, world) const`: 부작용 없이 지원 여부, 장치 점수, 실제 공간 위치, 장치 좌표를 계산한다.
3. `encode(candidates) const`: 선택된 표본을 장치 고유 명령으로 변환한다.
4. `advance(dt)`와 `apply(frame)`: 장치 내부 동역학과 명령 적용을 처리한다.
5. `observe(now)`와 `reset()`: 실제 출력의 관측 및 새 실험의 초기화를 제공한다.
6. `clone()`: 동일한 설정의 새 실행 인스턴스를 만든다. custom backend도 이 소유권 계약을 구현한다.

어댑터의 mutable 실행 상태는 각 runtime이 소유한다. 생성 시 `clone()`으로 fresh adapter를 얻고 reset하므로 입력 backend 설정을 여러 runtime에서 재사용할 수 있다. v0.2에서는 adapter가 명령을 정상 수신·수행한다고 가정한다. 센서로 얻은 실제 관측, 드라이버 연결 상태, 오류 반환과 큐 계약은 후속 API다.

## Routing과 physical realizability

각 표본을 모든 backend가 평가한다. 가능한 후보를 점수와 장치 ID로 정렬하고 남은 자원을 가진 첫 후보에 배정한다. 현재 점수는 monitor=100, projector=50-depth, drone=10이다. 물리 표면을 이미 가진 monitor가 선호된다. 표본 용량 초과나 같은 raster pixel 충돌이 있으면 다른 후보를 시도하고, 모두 실패하면 이유를 보고한다.

프로젝터는 현재 고정 pose만 지원한다. 발광 점은 드론 하나를 계속 점유한다. 드론 한 대로 Polyline을 빠르게 순회해 잔상 효과를 만들지는 않는다. 모니터 표면을 backend에서 제거해도 World의 해당 표면은 물리적으로 남으므로 프로젝터가 그 표면에 표시할 수 있다.

이 planner는 결정론적인 greedy baseline이다. primitive 전체의 atomic allocation, 관찰자별 quality, 장치 전환 비용, 최적 배치, blending을 풀지 않는다. 서로 다른 장치의 광학 간섭이나 겹침도 평가하지 않는다. raster sample의 색상이 같은 픽셀에 겹치면 합성 대신 pixel-conflict를 반환한다.

## 다음 추상화 결정

새 물리 방식은 `Point`만 지원해도 backend로 넣을 수 있다. 그러나 haptics·audio까지 지원하려면 현재 RGB primitive를 재사용하기보다 content/channel contract를 먼저 확장해야 한다. 움직이는 모니터는 support geometry와 device pose 업데이트가 필요하고, steerable projector는 후보 평가와 motion planning의 결합이 필요하다. 공간 그래픽의 의미적 API와 장치 고유 명령 간 경계를 유지하면서 증거가 필요한 기능부터 확장한다.


## 구현·언어 경계

C++20의 `std::variant<Point, Polyline, Patch>`가 primitive 계약이다. DeviceCommand는 RasterCommand / EmitterCommand / ExtensionCommand로 구분한다. C++ 코어는 Python, DOM, 그래픽 라이브러리를 참조하지 않는다. 새 rendering 방식은 C++ Backend를 구현하며 core runtime을 수정할 필요가 없다. 현재 Python은 내장 backend만 노출하고 Python subclass callback은 제공하지 않는다.

Python에서 `Point`, `Polyline`, `Patch`, `Surface`, `World`, `SceneFrame`을 작성하고 `Runtime.submit()` / `advance()` / `snapshot()`을 호출한다. pybind11을 통해 실제 C++ 구현이 실행된다. 결과는 읽기 전용 관측 타입으로 돌아오고 `to_dict()`로 JSON에 저장할 수 있다. 입력은 값 복사, 결과도 독립 snapshot이며 Python 객체 해제 후에도 runtime은 유효하다.

Bazel 9.2.0, Bzlmod lock, rules_cc, rules_python, pybind11_bazel을 사용한다. Python 3.13 toolchain이 extension과 Python 실행 target의 ABI를 맞춘다. 사용자 스크립트와 REPL은 `//python:python`, 실험 화면은 `//python:demo`로 실행한다. 별도 Python ABI로 빌드한 extension을 시스템 Python에 끼워 넣지 않는다. notebook/pip wheel은 아직 후속 배포 기능이다.

Python demo 서버는 single-threaded loopback HTTPServer로 실행한다. Python이 장면을 생성하고 C++ runtime 호출을 직렬화한다. viewer의 JavaScript는 이미 계산된 목표/관측을 그리며 물리 모델을 구현하지 않는다. static `write_view()`는 Python 관측을 단독 HTML에 담으며 카메라 회전/확대만 가능하다.

코어의 runtime instance는 thread-safe하지 않다. pybind11의 advance/snapshot은 GIL을 해제하므로 다중 Python thread에서 같은 instance를 호출하려면 외부 lock이 필요하다. 서로 다른 instance는 backend 상태를 공유하지 않는다. sample_scene은 한 장면에 최대 100,000 표본의 예산을 적용하고, 과도한 요청은 submit 단계에서 거부한다.
