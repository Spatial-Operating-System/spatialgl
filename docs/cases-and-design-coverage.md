# 다중 기기 사례와 SpatialGL 설계 coverage

검토일: 2026-10-03. 아래의 기존 구현 판정은 `19baf72`, architecture v0.2를 기준으로 한 설계 감사다. 이후의 표면 광학 구현은 [core API](core-api.md)와 [검증 기록](validation.md)에서 확인한다. 문헌 사례를 완전히 재현했다는 판정으로 확대하지 않는다.

## 결론과 검토 범위

현재 구현과 대화에서 제안한 설계는 구분해야 한다. 구현에는 고정 projector/monitor, point drone, static plane/AABB, sparse samples, 장치별 독립 refresh/latency/expiry가 있다. `SurfaceTarget`, `DisplayList`, sampling/traversal/presentation state는 아직 제안이다.

제안만으로도 모든 사례를 커버하지는 못한다. 부족한 핵심은 **공동 광학 합성, 공유 하드웨어 자원, 시간에 따른 세계 상태, 관찰자/채널, 단계별 실행 확인**이다. 장치별 독립 `evaluate(sample)`와 단일 장치 배정은 이 문제들을 표현하지 못한다.

아래 R01–R12는 논문·연구자·공식 구현·표준에서 확인한 사례다. 표준의 기능은 실제 하드웨어의 성능 보장으로 해석하지 않는다. T01–T08은 설계를 검증하기 위해 구성한 반례이며 실제 배포 사례라는 주장이 아니다. coverage 판정과 계약 변경은 SpatialGL에 대한 설계 추론이다. 문헌/초록/프로젝트 설명을 대조한 설계 감사이며, 논문 시스템 재현이나 새 simulator fixture 실행 결과가 아니다.

## 문헌·공식 자료 기반 사례

| ID / 사례 | 물리 구성과 확인한 동작 | 현재 v0.2 | 제안 설계에서 빠진 계약 / 핵심 교환 |
|---|---|---|---|
| R01 RoomAlive | 여러 projector와 Kinect를 보정하고 depth data로 방의 projection mapping을 구성. 공식 toolkit 예시는 6 projector + 6 Kinect. [공식 코드](https://github.com/microsoft/RoomAliveToolkit) | 독립 고정 plane 출력의 일부만 근사. 깊이 mesh, 영상, 합성 미구현. | 공유 세계 좌표와 calibration revision, dynamic mesh. 넓은 coverage ↔ 정렬·합성 비용. |
| R02 LightSpace | 다중 depth camera/projector의 세계 좌표 보정, table/wall/손 사이 projected object 이동. [연구자 설명](https://www.microsoft.com/en-us/research/project/lightspace/) | static surface 지정만 가능. 손의 이동·surface 간 anchor 변경 미지원. | 시간에 따른 target binding, handoff, stale tracking. 낮은 지연 ↔ 추정 안정성. |
| R03 Shader Lamps | 복수 projector로 물리 object의 외관을 변경하고 겹치는 영역을 합성. [저자 논문](https://web.media.mit.edu/~raskar/Shaderlamps/ShaderLamps_2000.pdf) | sparse RGB는 실제 광량/외관 모델이 아니다. | 한 목표에 여러 장치의 기여, material/photometric response, gamut. 밝기·정렬 ↔ 정확한 외관. |
| R04 Beamatron | motorized pan/tilt projector와 depth sensing, 조향 중 안정화와 물리 geometry에 맞춘 graphics. [저자 논문](https://www.microsoft.com/en-us/research/wp-content/uploads/2012/10/WilsonUIST2012.pdf) | fixed pose라 조향 자체 미구현. | pose trajectory, feedback, timed image warp, sensor/actuator dependency. 넓은 공간 ↔ 이동·안정화 시간. |
| R05 Lumipen | high-speed camera와 Saccade Mirror를 동축으로 구성해 움직이는 target에 pattern projection. [연구실 설명](https://ishikawa-vision.org/mvf/Lumipen/index-e.html) | static World와 fixed projector로 표현 불가. | 측정 시각·예측·유효기간을 가진 object pose, sample/exposure 시각의 광학 변환. 최신성 ↔ 예측 오차. |
| R06 Synthetic-aperture shadowless projection | 다수 projector의 중첩으로 손 등에 의한 그림자 영향을 줄이고, 정렬 오차에 의한 blur를 보정하는 연구. 2026 preprint. [원 논문 초록](https://arxiv.org/abs/2603.11551) | 차폐 후보 fallback만 가능. 동시 중첩 미지원. | 동일 target의 동시 복수 기여, 부분 visibility, blur/energy 합성. 그림자 강건성 ↔ blur·비용. |
| R07 Heterogeneous projectors as room lights | 다른 projector들을 환경 조명으로 사용하며 projection target 이외의 공간을 선택적으로 밝힘. [저자 논문](https://arxiv.org/abs/2403.02547) | ambient light나 환경 조명 목표 없음. | graphical pass와 illumination pass의 공동 자원 사용, controllable/uncontrollable ambient 구분. target contrast ↔ 주변 조명 품질. |
| R08 REFLCT | tracked head-mounted projector와 retroreflective surface로 여러 사용자에게 각자의 perspective-correct image를 제공. [저자 연구 자료](https://vgl.ict.usc.edu/bibtexbrowser.php?bib=ICT.bib&key=krum_head_2016) | 하나의 RGB/position 목표만 있고 observer 없음. | observer-indexed targets, 방향 의존 surface response, 시점별 cross-talk. 개인화 ↔ 광학 분리/보정. |
| R09 Multifocal stereoscopic PM | high-speed projector, active shutter glasses, electrically tunable lenses를 동기화해 눈별 영상과 초점 조건을 맞춤. [저자 논문](https://arxiv.org/abs/2110.07726) | 장치별 apply_at만 있고 눈/phase/focus dependency 없음. | coupled rig, eye/focus channel, phase window, trigger/clock capability. depth planes·눈별 출력 ↔ duty/밝기. |
| R10 Multi-projector structured-light vision | 복수 projector pattern이 겹치면 camera에서 원 pattern 식별이 어려워지는 sensing 사례. [저자 논문](https://arxiv.org/abs/1508.07859) | emitter 출력만 있고 capture task 없음. | measurement pass와 display pass, camera exposure interval, pattern identity, interference scheduling. display continuity ↔ measurement quality. |
| R11 Multi-head laser / IDN | 공식 자료가 timestamped streams, 복수 channel과 sample-synchronized multi-head output을 다룸. **표준의 표현 능력**을 확인한 사례. [ILDA 공식 자료](https://www.ilda.com/technical.htm) | laser scan command와 clock/stream queue 없음. | timed scan samples, clock domains, phase relation, buffer/underrun/cancel. 시간 정합성 ↔ buffering delay. |
| R12 MATD | acoustic trap으로 입자를 움직이고 RGB로 비추며 audio/tactile content도 만드는 복합 volumetric display. [원 연구 논문](https://www.nature.com/articles/s41586-019-1739-5) | point drone는 기하 목표의 일부만 유사. 해당 물리나 복합 출력 미지원. | volume target, trapping와 illumination의 공유 rig, 별도 audio/haptic channel. **광학 surface v1 밖의 경계 사례.** |

RoomAlive 등의 문헌에서 상호작용과 인식 기능을 제공한다고 해서 SpatialGL이 객체 인식 전체를 소유해야 한다는 뜻은 아니다. 상위 sensing/tracking이 만든 시간 정보와 uncertainty를 렌더러가 소비하는 계약이 필요하다는 추론이다.

## 구성한 반례와 수락 조건

이 표의 조건은 구현할 simulator fixture의 사양이며, 아직 실행한 테스트가 아니다.

| ID | 기기/상황 | 현재 또는 제안 설계의 실패 | 필요한 수락 조건 |
|---|---|---|---|
| T01 | raster background + laser cursor. cursor의 배경을 가려야 함. | 레이저 명령만 보내서는 배경 빛을 제거할 수 없음. | 공동 composition이 raster mask와 laser path를 함께 만들고, sync 불가 때 독립 출력 또는 거절을 명시. |
| T02 | projector 두 대 중 하나가 끊기고 옛 명령이 뒤늦게 도착. | 미래 장면이 폐기한 출력을 재점등할 수 있음. | command generation/epoch, bounded cancellation capability, stale packet rejection, failover provenance. 소프트웨어 reject와 실제 소등 확인을 구분. |
| T03 | 두 앱 또는 두 logical backend가 같은 steering mirror를 사용. | 각각의 local plan은 가능하지만 동시에 다른 방향을 요구. | physical resource ID를 공유하는 예약은 겹치지 않으며, 실행 직전 lease/epoch 확인. simulator clone과 실제 hardware ownership을 구분. |
| T04 | Polyline 편집으로 sample 수가 바뀜; 다른 device로 handoff. | 생성 순서 sample ID는 의미적으로 같은 점을 가리키지 않음. | stable draw ID + surface/path parameter correspondence. 대응 불가하면 명시적으로 재배정하고 잘못된 추적을 이어가지 않음. |
| T05 | 조향 projector 한 대에 두 벽 모두 최소 방문 주기와 밝기를 요구. | 평균 refresh만으로 어두운 긴 공백을 숨김. | 위치별 max dark gap/revisit, duty, age를 보고. 제한된 후보 탐색 실패와 입증된 물리 불가능을 구분. |
| T06 | 공중의 선/면을 표면 projector나 laser spot으로 요청. | 임의로 벽에 투영하면 다른 graphic으로 바뀜. | surface/volume/ray-path target 종류를 명시. 적절한 매질/장치가 없으면 unsupported; fog/scattering은 별도 모델. |
| T07 | 여러 장치의 clock drift와 jitter; 전부 visible이라는 fence 요청. | 명령 수신이나 apply_at을 실제 동시 발광으로 오해. | clock uncertainty와 phase/skew bounds, 단계별 receipts, unavailable 측정값. 보장 못 하는 sync group은 reject/relax negotiation. |
| T08 | 새 장면 만료 뒤 이전 명령이 아직 출력; world calibration도 바뀜. | old output을 새 geometry에 붙여 realized로 오판. | source scene/world/calibration revision과 age 보존. v0.2 expiry·stale·no-resurrection 계약은 유지하되 실제 관측과 예측을 구분. |

## 보완해야 하는 공통 계약

### 1. 그림의 목표와 장치 배정을 분리: 공동 contribution

`sample -> one device`는 R03/R06/T01을 커버하지 못한다. 목표 하나를 여러 장치가 함께 실현하거나 하나의 emission이 여러 목표에 영향을 줄 수 있다.

첫 광학 모델은 보정된 비간섭성 광량과 직접 광 전달을 가정하고 다음 형태로 제한할 수 있다.

```text
E_target(x, t) ≈ E_uncontrolled(x, t) + Σ_i A_i(q_i(t), world(t)) u_i(t)
0 ≤ u_i(t) ≤ device_limit_i
```

`A_i`는 pose/visibility/footprint와 calibration에 따른 표면 광량 전달이다. 반사율과 observer response를 추가해야 실제 외관을 평가할 수 있다. sRGB 값을 이 식에 바로 더하지 않는다. 알려진 diffuse surface와 coarse spatial bins부터 시작한다. 이 모델은 coherent wave interference, 임의 BRDF, volumetric scattering을 보장하지 않는다.

가상 layer의 `OVER`와 장치 기여의 `EXCLUSIVE / NORMALIZED / ADDITIVE`는 별도 단계다. 평균 광량이 같아도 최대 출력 공백이 다르면 같은 품질로 판정하지 않는다. negative light가 필요한 외관 목표는 policy로 만들 수 없으며 photometric residual을 보고한다.

### 2. Device list를 ResourceGraph가 있는 Rig로 확장

장치 목록만으로는 R09/T03의 공동 액추에이터를 표현할 수 없다. 여러 logical output이 같은 mirror, laser DAC, DLP engine, camera, trigger line을 쓸 수 있다.

`Rig`는 physical resource ID, actuator coupling, mode exclusivity, clock domain, supported trigger, buffer/cancel/ack capability를 선언한다. `TaskPlan`은 해당 자원의 시간 구간을 예약한다. stereo/focus/image는 하나의 coupled schedule이 된다. 실제 backend를 만들 때 simulator의 fresh clone이 실제 기기를 복제한 것으로 해석되어서는 안 된다.

최초 planner는 작은 후보 집합을 결정론적으로 비교해도 된다. 모든 경우에 최적 계획을 구한다는 계약은 필요 없다. 결과는 `unsupported`, `proved_infeasible`, `no_plan_found`, `accepted`, `invalidated`를 구분한다. 탐색 budget 소진을 물리 불가능의 증명으로 표시하지 않는다.

### 3. Command envelope 안에 TimedProgram을 둠

레이저 궤적·조향·DLP exposure·shutter는 순간 apply가 아니라 interval/phase를 가진다. `SceneFrame / DeviceFrame / RealizationState` 분리는 유지하고, device command를 다음처럼 확장한다.

```text
DeviceFrame envelope:
  generation, source_scene, world_revision, calibration_revision
  issued_at, valid_interval, clock_domain, timing_uncertainty
  update_mode: replace | append | cancel
  TimedProgram: actuator trajectory + raster exposures | scan samples | trigger events
```

스캔 샘플은 순서와 발광/소등, dwell 및 출력 시각을 보존한다. 그림을 점으로 먼저 분해하면 이 정보가 없어지므로 DisplayList에서 raster/curve 의미를 유지한다. `PipelineState`, `TraversalState`, `PresentationState`는 proposal 생성 및 scheduling 입력이며 물리 동역학을 수정하지 않는다.

`atomic=stroke`는 지원 가능한 경계에서 새 content로 전환한다는 뜻이다. 여러 장치의 전 세계적 atomic visibility는 자동으로 따라오지 않는다. 동기화는 하드웨어 capability와 clock uncertainty를 근거로 범위를 선언한다. legacy backends는 단일 hold interval로 기존 명령 의미를 보존할 수 있다.

### 4. World를 시간 정보가 있는 입력 snapshot으로

R02/R04/R05는 geometry뿐 아니라 그 geometry가 **언제 관측된 것인지**가 중요하다. `WorldSnapshot`에는 source time, reference frame, transform/mesh revision, valid horizon, uncertainty와 선택적 prediction이 필요하다. target에는 local coordinates와 surface/object anchor를 둔다.

renderer는 tracker를 재구현하지 않고 snapshot을 소비한다. calibration/tracking이 바뀌면 영향 받는 계획을 revalidate한다. 과거에 찍힌 관측을 새 world 좌표로 해석하지 않는다. observer 역시 timestamped pose이며, emitted ray visibility와 observer visibility를 구분한다.

### 5. Target과 pass의 종류를 명시

`SurfaceTarget`만으로 R08/R09/R10/R12/T06 모두를 표현하지 않는다. 첫 범위는 surface optics로 고정하고 확장 지점을 명시한다.

- `SurfaceTarget`: 표면에 붙은 graphic 또는 보정된 appearance.
- `ViewTarget`: 특정 observer/eye/channel에 보이는 graphic. 추가 광학 분리 모델 필요.
- `MeasurementPass`: pattern emit + sensor exposure의 resource schedule. reconstruction은 외부 책임.
- `VolumeTarget` 및 audio/haptic channel: 후속 계약이며 v1에서는 명시적 unsupported.

`GraphicsPass`, `IlluminationPass`, `MeasurementPass`가 같은 physical resource를 공유할 수 있다. policy scope는 draw, composition group, rig schedule로 나눈다. draw 하나의 상태가 다른 앱의 자원 한계를 완화하지 않는다. 앱들은 별도 AppId/LayerId를 갖고, composition 이후 physical allocation을 수행한다.

### 6. Feedback을 위치 오차 하나에서 시간·광량·증거로 확장

R06/R09/T05/T07은 동일한 위치에서도 출력 품질이 다르다. 위치/형상 오차 외에 frame age, max revisit/dark gap, duty, photometric residual, group skew/phase, 누락 coverage, budget consumption을 집계한다.

receipts는 `accepted`, `queued`, `latched`, `actuator_settled`, `emission_predicted`, `emission_measured`를 capability에 따라 구분한다. GPU fence나 driver acknowledgement가 photon measurement가 되지는 않는다. 예상 값에는 model/world/calibration revision, 관측에는 sensor time과 uncertainty를 보존한다. 측정할 수 없는 값은 unavailable이며 0이나 success가 아니다.

## OpenGL에서 가져올 state의 coverage

| 선택 | 커버되는 사례 | state만으로 해결되지 않는 부분 |
|---|---|---|
| sampling/filtering/LOD | R01/R03/R06, laser detail | 물리 footprint와 시간 budget 모델이 있어야 함. |
| virtual depth/mask/composition | R01/R03/T01 | emitter visibility, observer transfer, negative-light 불가능은 별도. |
| traversal/batching | R04/R05/R11 | shared actuator와 trajectory cost를 Rig planner가 계산해야 함. |
| buffering/presentation | R05/R09/R11/T07 | hardware phase/clock uncertainty 없이는 sync 보장 불가. |
| render pass/FBO | R03/R07/R10 | compositing 이후 공동 광학 합성과 sensor capture scheduling 필요. |

이 표는 state들이 유용하다는 근거이지, policy를 추가하면 모든 물리 모델이 자동으로 구현된다는 뜻이 아니다.

## 구현 우선순위와 simulator 검증 묶음

### v1: surface optics에 집중

1. DisplayList + stable DrawId + SurfaceTarget. raster와 ordered curve를 보존.
2. Rig/resource IDs + timed raster/scan/steering programs. scoped pipeline/traversal/presentation states.
3. diffuse/직접광/coarse bins의 joint contribution, fixed 및 timestamped planar targets.
4. generation-aware queue/cancel, device clock uncertainty, prediction/measurement provenance.
5. 다음 fixture로 정책별 Pareto trade-off를 trace로 비교.

| Fixture | 구성 | 확인할 관측 |
|---|---|---|
| F01 overlap | fixed projector 2대, 동일 wall; 한 대 부분 차폐 | 한 target에 양쪽 기여, 광량 합, ownership 변경, max dark gap |
| F02 hybrid | raster background + galvo cursor | virtual OVER와 physical additive의 구분, 공동 mask, 버전 skew |
| F03 shared steering | 두 앱, logical output 2개, mirror 1개, 표면 2개 | resource conflict, revisit/duty/age, no-plan-found vs infeasible |
| F04 moving target | moving plane + sensor delay + steerable projector | capture time/예측/출력 시간의 정렬, tracking loss와 revalidation |
| F05 async laser pair | sample rates와 latency가 다른 laser 2대 | 순서·blanking·phase, drift/jitter/underrun, 가능한 sync 범위 |
| F06 fault/handoff | 일부 장치 끊김, 늦은 old command, calibration 변경 | generation rejection, 기존 output provenance, cancellation evidence |

이 6개가 돌아가도 문헌의 완전한 시스템을 재현했다는 뜻은 아니다. 각각 구조적인 반례를 통과하는 최소 모델이며, photometric bin 크기와 integration time을 바꿔 convergence도 확인해야 한다. physical truth는 simulator 가정에 한정된다.

### 후속 범위

R08/R09의 방향 의존/eye/focus targets, R10의 measurement scheduler, R07의 환경 조명 목표는 확장된 계약으로 별도 milestone에 둔다. R12와 fog/volume, coherent optics, audio/haptic physics는 첫 optical implementation의 지원 범위에 넣지 않는다. 첫 API는 불가능한 목표를 명시적으로 거절해야 한다.

## 감사 판정

**현재 v0.2는 baseline이며 다중 기기 전체 coverage를 주장할 수 없다.** 이전의 SurfaceTarget + 작은 OpenGL형 state들 역시 물리 기여와 공유 자원·시간·observer 계약을 보완해야 한다. 사례를 더 많이 backend enum으로 추가하는 것보다, 위 6개 계약을 먼저 검증하는 편이 확장 가능성을 높인다.
