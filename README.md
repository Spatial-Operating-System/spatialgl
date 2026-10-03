# SpatialGL

**C++20 공간 그래픽 코어 · Bazel 빌드 · Python 사용자 API와 시뮬레이터.**

앱은 세계 좌표의 Point/Polyline/Patch와 표시 시각을 지정한다. C++ 코어가 물리 표면, 장치 자원, 갱신 주기와 지연을 고려해 장치별 명령을 만들고 실제 출력 상태를 계산한다. Python에서 장면을 작성하고 결과를 관찰·저장한다.

출발점: [공유 설계 대화](https://chatgpt.com/share/6ac0c438-b800-83ee-8afc-b795cbc52104). `SceneFrame` / `DeviceFrame` / `RealizationState` 구분을 유지한다.

## 바로 실행

Bazelisk 또는 Bazel 9.2.0과 C++20 컴파일러가 필요하다. macOS에서는 Command Line Tools가 필요하다. Bazel이 잠긴 의존성과 Python 3.13 런타임을 받아 Python extension을 함께 빌드한다.

```sh
bazel test //...
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

## Python에서 사용

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

C++ 구현: `cpp/include/spatialgl/spatialgl.h`, `cpp/src/spatialgl.cc`. Python API: `python/spatialgl/__init__.py`. native module: `python/bindings.cc`. 전체 빌드는 `MODULE.bazel`과 `BUILD.bazel`로 관리한다.

상세 계약은 [architecture](docs/architecture.md), 다음 단계는 [roadmap](docs/roadmap.md), 검증 결과는 [validation](docs/validation.md)에 기록했다.

## 현재 물리 모델

고정 프로젝터는 pinhole/FOV·픽셀 양자화·유한 평면·AABB 차폐를 계산한다. 모니터는 보정된 평면을 UV/pixel로 매핑한다. 드론은 Point만 지원하고, 표본 ID의 슬롯을 유지하며 속도 제한으로 이동한다. 각 장치의 refresh/latency는 서로 독립적이다.

Polyline/Patch는 유한 표본 근사이며, 출력 명령은 희소 raster sample / emitter target이다. 광학 방사 측정·반사율·비평면 clipping·관찰자 시야·가속도·충돌 회피·조향과 실제 장치 제어는 후속 단계다. 장면 만료 시 출력이 꺼지는 모델도 실제 장치 보장을 뜻하지 않는다.

로컬 독립 Git 저장소이며 원격 저장소는 아직 지정하지 않았다. 이전 TypeScript prototype은 Git 이력에 보존되어 있다.
