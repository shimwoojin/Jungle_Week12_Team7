# 머티리얼 시스템 개편 설계/계획 (Material Rework Plan)

> 작업 브랜치: `feature/material-rework`
> 담당: P3 (렌더)
> 상태: **리뷰 대기** — P1(직렬화·마샬링) / P2(에디터·모듈) 검토 요청
> 관련: 직전 머지된 `feature/serialize-template-method` (UObject::Serialize Template Method)

이 문서는 머티리얼 시스템의 네 가지 문제(반투명 블렌드 처리, 셰이더/RenderPass 커플링, JSON 직렬화, 에디터)를 진단하고, 이를 **하나의 일관된 개편**으로 해결하는 설계와 작업 순서를 정리한다. 구현 전 팀 합의를 받기 위한 문서다.

---

## 1. 배경 / 목적

현재 머티리얼은 셰이더, `ERenderPass`, 블렌드/뎁스/래스터 상태를 **직접** 들고 다니며, 저장 포맷도 다른 에셋과 달리 **JSON 단독**이다. 그 결과:

- 머티리얼을 새로 만들거나 적용할 때 저수준 렌더상태를 손으로 맞춰야 하고,
- 같은 정보(Pass/블렌드)가 머티리얼·Proxy·JSON·에디터에 **중복**되어 어긋날 여지가 크다.

목표는 **"머티리얼은 고수준 의도(Domain + BlendMode)만 선언하고, 엔진이 렌더상태를 도출"**하는 UE 스타일로 바꾸면서, 저장을 바이너리(.uasset)로 통합하고 에디터를 객체 기반으로 강화하는 것이다.

---

## 2. 현재 상태 진단

### 2.1 Translucent 패스 / 블렌드 — 패스 구조는 건전, 선언 방식이 문제

- 반투명은 **단일 패스**에서 모든 블렌드를 처리하고(`Render/RenderPass/TranslucentPass.cpp:6`), back-to-front depth 정렬(`Render/Command/DrawCommand.h:135` `ComputeTranslucentSortKey`)을 쓴다. **이 구조는 올바르며 유지한다.**
- 블렌드 모드는 `EBlendState{Opaque, AlphaBlend, Additive, Modulate, NoColor}`(`Render/Types/RenderStateTypes.h:24`). D3D11 상태는 `FBlendStateManager`가 생성(`Render/RenderState/BlendStateManager.cpp:5`).
- 블렌드는 **per-DrawCommand**로 적용된다: `Material->GetBlendState()` → `ApplyMaterialRenderState`(`Render/Command/DrawCommandBuilder.cpp:136`) → StateCache 비교 후 `OMSetBlendState`(`Render/Command/DrawCommandList.cpp:173`).
- **결론**: 패스 내 처리 방식 자체는 명확하다. 불명확한 것은 패스가 아니라 **머티리얼이 "나는 Additive다"를 선언하는 방법**이며, 이는 2.2와 같은 뿌리다.

### 2.2 셰이더 + RenderPass 직접 커플링 — 핵심 문제

- `UMaterial`이 저수준 렌더상태 4개를 직접 소유한다(`Materials/Material.h:87`):
  `ERenderPass RenderPass`, `EBlendState BlendState`, `EDepthStencilState DepthStencilState`, `ERasterizerState RasterizerState`.
- **이중 진실(dual source of truth)**: Proxy가 그릴 Pass를 정하는데 머티리얼도 `RenderPass`를 갖는다. DrawCommand 빌드 시 **둘이 일치할 때만** 머티리얼 렌더상태가 적용된다:
  ```cpp
  // Render/Command/DrawCommandBuilder.cpp:248
  if (Pass == Mat->GetRenderPass())
      ApplyMaterialRenderState(Cmd.RenderState, Mat, BaseState);
  ```
  불일치 시 머티리얼 상태가 **조용히 무시**된다 — 디버깅이 어려운 함정.
- UE식 **Material Domain 개념이 없다**. `ERenderPass`(16개, `Render/Types/RenderTypes.h:34`)가 "머티리얼 종류"와 "파이프라인 단계"를 뭉뚱그린다.
- 부분 자동화는 있다: `FMaterialManager::StringToBlendState/StringToDepthStencilState/StringToRasterizerState`가 Pass 기반 기본값을 도출한다(`Materials/MaterialManager.cpp:249-317`). 그러나 JSON에 Pass를 직접 기입해야 동작한다.

### 2.3 직렬화 — 머티리얼만 JSON, 그러나 바이너리 코드는 이미 존재

- 머티리얼만 `.mat`(JSON, 텍스트): `FMaterialManager::SaveToJSON`(`Materials/MaterialManager.cpp:319`), 로드 `GetOrCreateMaterial`(`...:41`).
- 다른 에셋은 `.uasset`(바이너리): `FWindowsBinWriter` + `FAssetPackageHeader` + `Asset->Serialize(FArchive&)` (예: `Particle/ParticleSystemManager.cpp:73`).
- **핵심 발견**: `UMaterial::Serialize(FArchive&)`가 이미 존재하여 CB CPU 데이터 + 텍스처 경로를 바이너리로 직렬화한다(`Materials/Material.cpp:260-350`). **그러나 디스크 저장에는 쓰이지 않고 JSON 경로가 대신 쓰인다.** 직렬화가 이원화돼 있다.
- 따라서 바이너리 통합은 "없는 것을 새로 짜는" 작업이 아니라, **이미 있는 `FArchive` 경로를 디스크에 연결하고 JSON 경로를 제거하는** 작업에 가깝다.

### 2.4 에디터 — JSON 텍스트에 직접 결합

- `FEditorMaterialInspector`가 `CachedJson`을 직접 읽고 쓴다(`Editor/UI/Panel/EditorMaterialInspector.cpp:18-149`). 스칼라/Vector3/Vector4/Matrix 파라미터와 텍스처 drag-drop만 편집 가능.
- **편집 불가**: ShaderPath, RenderPass, BlendState, DepthStencil, Rasterizer → JSON 수동 편집 필요.
- **"Create Material" 팩토리 없음** — FBX import(`Mesh/.../FbxMaterialImporter.cpp:237`) 또는 수동 파일 생성만 가능.
- 에디터가 JSON 포맷에 묶여 있어, 바이너리 전환 시 **객체(프로퍼티) 기반으로 재구축**해야 하며, 그 과정에서 Domain/Blend 드롭다운이 자연히 생긴다.

### 2.5 MaterialInstance (참고)

- `UMaterialInstance : UMaterial`이 존재하고 main에 머지됨(`Materials/MaterialInstance.h:14`, `.cpp:8`). `.mat`의 `Parent` 키로 자동 생성, Parent의 Template/CB/Texture 복제 + 렌더상태 오버라이드.
- **개편 시 같은 직렬화·도출 경로를 타야 한다** — 설계에 포함.

---

## 3. 핵심 통찰: 네 문제가 한 뿌리

저수준 렌더상태가 **(a) 머티리얼에 손으로 박히고, (b) Proxy와 중복되고, (c) JSON에 직렬화되고, (d) 에디터가 그 JSON을 직접 만진다.** 따라서 **"머티리얼은 고수준 의도만 선언하고 엔진이 렌더상태를 도출"**로 바꾸면 네 문제가 동시에 풀린다.

---

## 4. 제안 설계

### 4.1 고수준 의도 enum 도입

```cpp
enum class EMaterialDomain : uint8 { Surface, PostProcess, UI, Decal };
enum class EBlendMode      : uint8 { Opaque, Masked, Translucent, Additive, Modulate };
//  Masked(알파 클립)은 신규. 나머지는 기존 EBlendState 와 의미 1:1.
```

머티리얼은 `Domain`과 `BlendMode`만 저장한다. 저수준 4개 상태(`ERenderPass/EBlendState/EDepthStencilState/ERasterizerState`)는 **더 이상 저장하지 않고 로드 시 도출**한다.

### 4.2 도출 테이블 (단일 진실)

`(EMaterialDomain, EBlendMode)` → `(ERenderPass, EBlendState, EDepthStencilState, ERasterizerState)` 매핑을 한 곳에 둔다. 현재 `MaterialManager::StringToXxx`(`MaterialManager.cpp:249-317`)에 흩어진 로직을 여기로 모은다.

| Domain | BlendMode | RenderPass | BlendState | DepthStencil | 비고 |
|---|---|---|---|---|---|
| Surface | Opaque | Opaque | Opaque | Default | |
| Surface | Masked | Opaque | Opaque | Default | 셰이더 clip() |
| Surface | Translucent | Translucent | AlphaBlend | DepthReadOnly | |
| Surface | Additive | Translucent | Additive | DepthReadOnly | |
| Surface | Modulate | Translucent | Modulate | DepthReadOnly | |
| PostProcess | — | PostProcess | (도메인 기본) | NoDepth | |
| UI | — | UI | AlphaBlend | NoDepth | |
| Decal | Opaque/Translucent | Decal/AdditiveDecal | (블렌드별) | DepthReadOnly | |

> 표는 초안이며, 정확한 기본값은 기존 `StringToXxx`의 Pass별 기본값을 옮겨 확정한다.

### 4.3 이중 진실 제거

`if (Pass == Mat->GetRenderPass())` 조건부 적용(`DrawCommandBuilder.cpp:248`)을 없앤다. 머티리얼의 `Domain`이 Pass의 권위가 되도록 하여, Proxy가 머티리얼에 질의해 Pass를 결정한다. (Proxy가 강제 Pass를 갖는 경우의 처리 규칙은 구현 시 확정.)

### 4.4 바이너리 직렬화 통합

- 기존 `UMaterial::Serialize(FArchive&)`(`Material.cpp:260`)를 `.uasset` writer(`FWindowsBinWriter` + `FAssetPackageHeader`)에 연결하고 JSON 경로를 제거한다.
- 저장 항목: `Domain`, `BlendMode`, ShaderPath(문자열), 텍스처 슬롯→경로, CB blob. **저수준 렌더상태 4개는 저장 안 함(도출).**
- 셰이더/텍스처 참조는 당분간 **문자열 경로 유지**(느슨한 결합). GUID화는 별도 과제로 분리.

### 4.5 에디터 재구축

- JSON(`CachedJson`)이 아니라 `UMaterial` 객체 기반으로 편집.
- 신규 UI: Domain/BlendMode 콤보박스, 셰이더 선택 드롭다운, "Create Material" 팩토리.

---

## 5. 직전 Serialize 리팩터와의 연결

직전 머지된 Template Method 덕분에, 현재 group-D(완전 수동, `UObject::Serialize` 미호출)인 `UMaterial`을 바이너리로 옮기면서 **`ShouldReflectProperties()=true` + `SerializeExtra()` 훅**으로 깔끔히 편입할 수 있다:

- 단순 필드(`Domain`, `BlendMode`, ShaderPath, 텍스처 경로)는 `UPROPERTY(Save)` 반사 자동 직렬화.
- 셰이더 리플렉션 기반 CB blob은 `SerializeExtra(Ar)` 훅에서 수동 직렬화.

즉 추가된 직렬화 훅이 이 개편에 그대로 활용된다.

---

## 6. 작업 순서 (Phase)

각 Phase는 독립적으로 빌드·검증 가능하도록 쪼갠다.

1. **Phase 1 — 의도 enum + 도출 테이블 추가**: `EMaterialDomain`/`EBlendMode`와 도출 테이블 도입. 기존 Pass 필드와 병행(런타임 동작 불변). 검증: 빌드.
2. **Phase 2 — 도출 사용 + 이중 진실 제거**: 머티리얼이 도출 테이블로 렌더상태 결정, `Pass==RenderPass` 조건 제거. 검증: 런타임(불투명/반투명/파티클 시연).
3. **Phase 3 — 바이너리 직렬화 전환**: `.uasset` 저장/로드, JSON 제거. 기존 `.mat` 마이그레이션(일괄 변환 또는 한시적 양쪽 로드). 검증: 기존 에셋 round-trip.
4. **Phase 4 — 에디터 재구축**: 객체 기반 + Domain/Blend UI + Create 팩토리. 검증: 에디터 편집→저장→재로드.

핵심은 1→2(커플링/껄끄러움 해소), 통합/UX는 3→4.

---

## 7. 리스크 / 오픈 이슈

- **기존 `.mat` 마이그레이션**: JSON→바이너리 전환 시 기존 머티리얼 변환 필요. 일괄 변환 스크립트 또는 한시적 양쪽 로드 중 택일.
- **MaterialInstance**: `Parent` 기반 인스턴스도 동일 직렬화·도출 경로를 타야 함.
- **Proxy 강제 Pass**: 일부 Proxy가 머티리얼과 무관하게 Pass를 강제하는 경우(예: ShadowMap, SelectionMask)의 규칙 확정 필요.
- **셰이더/텍스처 참조**: 문자열 경로 유지 권장. GUID/AssetRegistry화는 범위에서 분리.
- **Masked 도입 여부**: 셰이더 clip() 경로가 필요. Phase 1 범위에 포함할지 결정 필요.

---

## 8. 리뷰 요청 포인트 (P1 / P2)

- **P1 (직렬화)**: Phase 3의 `.uasset` 통합 방식 — 기존 자산 마이그레이션 전략, `SerializeExtra` 훅을 통한 CB blob 직렬화 접근에 대한 검토.
- **P2 (에디터)**: Phase 4 에디터 재구축 범위 — Domain/Blend UI, Create 팩토리, 기존 `EditorMaterialInspector`의 JSON 의존 제거.
- **공통**: §4.2 도출 테이블의 기본값이 현 동작과 일치하는지 확인.

---

## 부록: 주요 참조 파일

| 영역 | 파일:라인 |
|---|---|
| 렌더 패스 enum | `Render/Types/RenderTypes.h:34` |
| 블렌드 상태 enum | `Render/Types/RenderStateTypes.h:24` |
| Translucent 패스 | `Render/RenderPass/TranslucentPass.cpp:6` |
| Translucent 정렬키 | `Render/Command/DrawCommand.h:135` |
| 블렌드 상태 매니저 | `Render/RenderState/BlendStateManager.cpp:5` |
| 머티리얼 렌더상태 적용 | `Render/Command/DrawCommandBuilder.cpp:136`, 조건 `:248` |
| 블렌드 적용(StateCache) | `Render/Command/DrawCommandList.cpp:173` |
| UMaterial 정의 | `Materials/Material.h:75`, 렌더상태 필드 `:87` |
| UMaterial::Serialize(FArchive) | `Materials/Material.cpp:260` |
| MaterialManager (JSON 저장/로드/도출) | `Materials/MaterialManager.cpp:41`, `:249`, `:319` |
| MaterialInstance | `Materials/MaterialInstance.h:14`, `.cpp:8` |
| 머티리얼 에디터 | `Editor/UI/Panel/EditorMaterialInspector.cpp:18` |
| 바이너리 에셋 저장 참조 | `Particle/ParticleSystemManager.cpp:73` |
| FBX 머티리얼 import | `Mesh/.../FbxMaterialImporter.cpp:237` |
