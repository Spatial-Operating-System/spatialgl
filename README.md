# SpatialGL

**세계 좌표의 그래픽을 물리적 출력으로 실현하는 시뮬레이션 기반 라이브러리.** 앱은 무엇을 어디에 표시할지 지정하고, SpatialGL은 장치의 표시 가능 영역·시간·자원을 고려해 명령을 만들고 실제 상태를 보고한다.

출발점: [공유 설계 대화](https://chatgpt.com/share/6ac0c438-b800-83ee-8afc-b795cbc52104). 공유 대화의 `SceneFrame` / `DeviceFrame` / `RealizationState` 구분을 바탕으로 새로 구현했다. 대화에 언급된 ZIP의 소스는 가져오지 않았다.

## 실행

Node.js 22.12+ 또는 호환되는 최신 버전이 필요하다.

```sh
npm ci
npm run dev
```

터미널에 표시되는 로컬 주소를 열면 3D 실험실이 실행된다. 프로젝터 두 대, 모니터, 발광 드론으로 같은 장면을 실현한다. 장면 종류·갱신률·명령 지연·드론 수·이동 속도·차폐를 조절하고, 정지/100 ms 진행/초기화 및 최근 300개 관측의 JSON 저장을 사용할 수 있다. 설정 변경은 새 실험을 시작한다.

```sh
npm run check       # 계약/기하/라우팅/시간 테스트 + 데모/라이브러리 빌드
npm run simulate    # 브라우저 없이 드론 이동과 장면 만료 확인
```

## 최소 API

```ts
import { DroneBackend, SpatialRuntime } from './src/index.js';

const runtime = new SpatialRuntime(
  { surfaces: [], occluders: [] },
  [new DroneBackend({
    id: 'swarm', count: 3, speed: 1.2,
    home: [0, 0.2, 1], min: [-3, 0, -2], max: [3, 3, 3],
    refreshHz: 20, latency: 0.08,
  })],
);
runtime.submit({
  id: 'frame-0', presentAt: 0, expiresAt: 2,
  primitives: [{
    id: 'marker', kind: 'point',
    position: [1, 1.5, 0], color: [1, 0.65, 0.2],
  }],
});
const state = runtime.advance(0.5);
// 목표 위치, 실제 위치, 위치 오차, 원본 장면 ID, 장치별 명령 확인
console.log(state.samples, state.outputs, state.deviceFrames);
```

`Point`, `Polyline`, `Patch`는 장치와 독립적인 세계 좌표 그래픽이다. `surfaceId`는 특정한 물리 표면에 대한 명시적 제약이다. 빈 공간의 Patch를 프로젝터나 드론이 임의로 다른 그래픽으로 바꾸지 않는다.

## 구조

```text
Application → SceneFrame → sampling → capability + support routing
                                      ↓
                             device-specific encoding
                                      ↓
                    DeviceFrame → device clock + latency + dynamics
                                      ↓
                              RealizationState
```

| 계층 | 책임 |
|---|---|
| `src/core/types.ts` | 장면·표면·장치·출력 계약 |
| `src/core/planner.ts` | 가능성 평가, 장치 선택, 자원 충돌과 미실현 이유 |
| `src/core/runtime.ts` | 장면 유효기간, 장치 갱신, 명령 전달 이벤트, 관측 |
| `src/backends/raster.ts` | 고정 프로젝터와 모니터의 픽셀 좌표 매핑 |
| `src/backends/drone.ts` | 발광 점의 용량, 슬롯 유지, 속도 제한 이동 |
| `demo/` | 라이브러리 결과를 보여주는 Three.js 관측 뷰 |

Three.js는 관측용 뷰에만 사용한다. 핵심 라이브러리는 DOM·WebGL·Three.js 의존성이 없어 headless 환경에서 실행할 수 있다.

자세한 계약, 확장 방식, 연구 질문은 [architecture](docs/architecture.md), 구현 단계는 [roadmap](docs/roadmap.md), 검증 기록은 [validation](docs/validation.md)에 정리했다.

## 현재 모델의 범위

프로젝터는 pinhole/FOV·픽셀 양자화·유한 직사각형 표면·AABB 차폐를 계산한다. 모니터는 보정된 평면의 UV를 픽셀로 매핑한다. 드론은 Point만 지원하고, 동일한 표본 ID의 드론 슬롯을 유지하며 일정한 최대 속도로 이동한다. 장면과 장치의 갱신 주기는 서로 독립적이다.

Polyline과 Patch는 유한 표본으로 근사한다. 명령은 희소 raster samples / emitter targets이며, 완성된 이미지 버퍼나 실제 비행 제어 명령이 아니다. 광학 방사 측정, 반사율, 관찰자 시야, 가속도·충돌 회피, 자동 표면 선택과 조향은 아직 구현하지 않았다. 실제 장치 연결과 하드웨어 안전 보장은 포함하지 않는다.

초기 저장소는 로컬 독립 Git 저장소이며 원격 저장소는 아직 지정하지 않았다.
