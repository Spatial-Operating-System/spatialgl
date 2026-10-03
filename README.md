# SpatialGL

**C++20 공간 그래픽 코어 · Bazel 빌드 · Python 사용자 API와 시뮬레이터.**

앱은 표면에 붙은 Point/Polyline/Patch와 표시 시각을 지정한다. C++ 코어가 장치의 광학 범위, 공유 자원과 지연을 고려해 프로그램과 예측 관측을 만든다. 사용자 작성 API는 Python이며, 다른 언어에서는 C11 API를 사용할 수 있다.

출발점: [공유 설계 대화](https://chatgpt.com/share/6ac0c438-b800-83ee-8afc-b795cbc52104). `SceneFrame` / `DeviceFrame` / `RealizationState` 구분을 유지한다.

## 바로 실행

Bazelisk 또는 Bazel 9.2.0과 C++20 컴파일러가 필요하다. macOS에서는 Command Line Tools가 필요하다. Bazel이 잠긴 의존성과 Python 3.13 런타임을 받아 Python extension을 함께 빌드한다.

```sh
bazel test //...
bazel run //examples/optics:scenarios
bazel run //examples/c:hello
bazel run //python:demo
```

[http://127.0.0.1:5188/](http://127.0.0.1:5188/)에서 Python 실험실을 열 수 있다. 프로젝터 두 대, 모니터, 드론을 같은 장면에 연결하고 장면 갱신률·장치 갱신률·지연·차폐·드론 수와 속도를 변경할 수 있다. 정지·100 ms 진행·초기화·최근 300개 관측의 JSON 저장도 지원한다.

```sh
bazel run //python:simulate                             # headless 예제
bazel run //python:python -- examples/hello.py          # 내 Python 스크립트
bazel run //python:python                               # Python REPL; sgl 미리 import
bazel run //python:demo -- --output .artifacts/view.html # 단독 HTML 관측 뷰
bazel build //:spatialgl                                # C++ 라이브러리
```

Python extension은 Bazel이 지정한 Python 3.13 ABI로 빌드된다. 별도 Python 환경이나 노트북용 wheel은 아직 제공하지 않는다. `//python:python`을 통해 사용자 스크립트와 REPL을 실행하면 일치하는 Python과 native module을 사용한다. Node/npm은 필요하지 않다.

## 표면 광학 API

```python
from spatialgl import optics as sgl

world = sgl.world([sgl.plane("wall")])
devices = sgl.rig([
    sgl.fixed_raster("left", position=(-1, 1, 3), target=(0, 1, 0)),
    sgl.fixed_raster("right", position=(1, 1, 3), target=(0, 1, 0)),
])
sim = sgl.OpticalRuntime(world, devices)
diagnostics = sim.submit(sgl.display(
    "frame-0", [sgl.draw("marker", "wall", color=(0, 1, 0))], expires_at=2,
))
state = sim.advance(0.1)
for target in state.targets:
    print(target.draw_id, target.aggregate_linear_rgb, target.provenance)
for program in state.programs:
    print(program.device_id, program.generation)
    for event in program.events:
        print(event.kind, event.time, event.position, event.linear_rgb)
```

`WorldSnapshot → Rig + RuntimeOptions → DisplayList → TimedProgram → Snapshot`이 기본 계약이다. 가상 layer 합성은 `OVER / REPLACE / ADD`, 장치 광량 합성은 `EXCLUSIVE / NORMALIZED / ADDITIVE`로 구분한다. 지원 범위와 제한은 [C++ 기본 기능](docs/core-api.md), Python 작성법은 [Python API](docs/python-api.md)에 정의한다.

고정 프로젝터, 속도·안정화 시간을 가진 조향 프로젝터, 순서와 blanking을 가진 galvo 모델을 제공한다. 예측 광량은 보정 gain을 사용하는 단순 직접광 모델이다. 입력과 제출은 값 복사이고, 반환 관측은 독립 snapshot이다. 지원하지 않는 기하와 정책은 diagnostic으로 반환한다. 실제 하드웨어 제어와 photon measurement는 후속 범위다.

## C API

공개 헤더는 `include/spatialgl/capi/spatialgl.h`다. ABI v1 descriptor 초기화, opaque context/frame/snapshot, submit/cancel/advance, count/index 관측 질의와 명시적 status를 제공한다. C++ 예외와 STL 타입을 C 경계에 노출하지 않는다. 입력은 복사되고 snapshot은 원본 context 파괴 후에도 유효하다.

```sh
bazel build //bindings/c:spatialgl_c
bazel run //examples/c:hello
```

[C API 계약](docs/c-api.md)과 [C11 예제](examples/c/hello.c)에서 소유권, 오류, 버전과 실행 방법을 확인할 수 있다.

## 호환 Python API와 기존 viewer

```python
import spatialgl as sgl
from spatialgl.view import write_view

world = sgl.World()
sim = sgl.Runtime(world, [
    sgl.Drones("lights", count=3, speed=1.2, latency=0.08),
])
sim.submit(sgl.SceneFrame(
    id="frame-0", present_at=0, expires_at=2,
    primitives=[sgl.Point("marker", position=(1, 1.5, 0), color=(1, 0.65, 0.2))],
))
state = sim.advance(0.5)
for result in state.samples:
    print(result.status, result.actual, result.error, result.reasons)

trace = state.to_dict()  # JSON으로 직렬화 가능한 관측과 장치 명령
write_view("view.html", state, world)
```

이 코드를 파일로 저장하고 `bazel run //python:python -- path/to/script.py`로 실행한다. `examples/hello.py`는 실행 가능한 예제다. `write_view()`는 네트워크나 외부 UI 패키지 없이 카메라 회전·확대가 가능한 단독 HTML 파일을 만든다. 저장된 snapshot은 고정 관측이며, 실시간 설정 변경은 `//python:demo`에서 한다.

## Abstraction layer

| 계층 | 역할 | 소유 코드 |
|---|---|---|
| SceneFrame | 무엇을 어디에, 언제 표시할지 | C++ 값 타입, Python 작성 API |
| Backend | 장치의 지원 범위·실현 방식·동역학 | C++ 추상 인터페이스 |
| DeviceFrame | 장치 고유 명령과 적용 시각 | C++ raster/emitter/extension 명령 |
| RealizationState | 실제 출력, 위치 오차, 원본 장면, 미실현 이유 | C++ 관측, Python 접근·JSON export |
| Experiment / viewer | 예제 장면과 실험 조작, 관측 표시 | Python 앱 + 관측 전용 Canvas 뷰 |

핵심 geometry, sampling, routing, event scheduling, projector/monitor pixel mapping과 drone motion은 모두 C++에 있다. pybind11은 타입과 호출만 연결한다. Python은 앱의 목표 장면을 작성하고 관측을 보여준다. 브라우저의 JavaScript는 카메라·표시·Python API 호출만 수행한다.

C++ 공개 API는 `include/spatialgl/`, 구현은 `libs/core`, `libs/backends`, `libs/runtime`, `libs/optics`, `libs/simulation`에 있다. C와 Python adapter는 각각 `bindings/c`, `bindings/python`에 있다. 모듈마다 별도 Bazel target을 갖고 `//:spatialgl`, `//:optics`, `//python:*`가 진입점을 제공한다. [모듈 구조](docs/modules.md)에 의존성과 책임을 기록했다.

상세 계약은 [architecture](docs/architecture.md), 다음 단계는 [roadmap](docs/roadmap.md), 검증 결과는 [validation](docs/validation.md)에 기록했다.

다중 물리 기기 설계 검토는 [사례와 coverage](docs/cases-and-design-coverage.md)에 기록했다. 문헌·공식 자료 기반 사례 12개와 구성한 반례 8개를 대조해 공동 광학 합성, 공유 하드웨어 자원, 시간·관찰자·관측 계약의 누락을 정리했다. 제안과 현재 구현, 향후 simulator fixture를 구분한다.

## 기존 viewer의 물리 모델

고정 프로젝터는 pinhole/FOV·픽셀 양자화·유한 평면·AABB 차폐를 계산한다. 모니터는 보정된 평면을 UV/pixel로 매핑한다. 드론은 Point만 지원하고, 표본 ID의 슬롯을 유지하며 속도 제한으로 이동한다. 각 장치의 refresh/latency는 서로 독립적이다.

기존 viewer는 호환 runtime의 유한 표본과 희소 raster/emitter 출력 모델을 사용한다. 새 표면 광학 API의 동작은 optical scenario runner와 snapshot에서 확인한다. 방사 측정·반사율·비평면 clipping·관찰자 시야·가속도·충돌 회피와 실제 장치 제어는 후속 단계다. 장면 만료 시 출력이 꺼지는 모델도 실제 장치 보장을 뜻하지 않는다.

로컬 독립 Git 저장소이며 원격 저장소는 아직 지정하지 않았다. 이전 TypeScript prototype은 Git 이력에 보존되어 있다.
