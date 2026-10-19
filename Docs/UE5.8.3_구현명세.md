# Raising Slime — Unreal Engine 5.8.3 구현 명세

유니티 프로젝트 `d:\UnityGameDevelop\raisingSlime`의 사냥 루프를 Unreal Engine 5.8.3에서 다시 구현하기 위한 명세다. 수치와 조작의 기준은 현재 플레이 코드이며, 씬에 있는 프리미티브를 변환하는 작업이 아니다. 플레이어가 보는 규칙과 문장은 `기획.md`에 있다.

작업 프로젝트는 `D:\UnrealGames\RisingSlime`이다. `RisingSlime.uproject`가 이미 있다. 프로젝트를 새로 만들지 않는다.

## 1. 옮겨야 하는 게임

플레이어는 슬라임이다. 둥지 밖으로 나가 사물과, 쓰러뜨린 개체를 몸에 붙여 삼킨다. 삼키면 이번 출정의 질량이 늘고, 형질은 확률로 붙는다. 붙은 형질은 불안정해서, 둥지 소화실에서 확정하기 전에는 다음 출정에 남지 않는다. 죽으면 이번 출정의 질량과 불안정 형질만 사라진다. 정착한 질량과 확정 형질은 남는다.

한 레벨 `Hunt`로 완결된다. 저장, 네트워크, 인벤토리 화면은 없다.

## 2. 유니티에서 막힌 지점

게임 규칙은 이미 닫혀 있다. 막힌 곳은 몸이다.

- `JellyMeshDeformer.Build`는 `(위도 - 1) * 경도 + 2`가 250을 넘으면 위도·경도를 줄인다. 유니티 볼록 `MeshCollider`의 정점 상한이 255개라서 생긴 제한이다.
- `WriteMesh`는 렌더 메시만 고친다. 충돌 메시는 생성할 때 한 번 넣고, 찌그러진 표면을 따라가지 않는다. 접촉 자국은 보이는 것에만 있다.
- 기본 설정에서 부피 보존, 충격 변형, 방사 확장은 꺼져 있다. 접촉은 표면 정점을 노멀 방향으로 미는 포즈다.
- 매 프레임 CPU에서 `RecalculateNormals`를 호출한다.
- 슬라임 셰이더는 램버트 랩, 프레넬, 스펙큘러만 있는 단색 반투명이다. 두께와 굴절이 없다.

언리얼에서 역할을 나눈다.

| 역할 | 담당 |
| --- | --- |
| 이동, 피격, 포식 판정 | 질량에 따라 커지는 구체. 조작은 이 구체가 맡는다. |
| 보이는 몸 | 용접된 구체 표면. 1차는 C++ 스프링으로 유니티 포즈를 재현하고, 정점 상한 250은 두지 않는다. |
| 부피가 있는 찌그러짐 | 1차가 붙은 뒤 Chaos Flesh 사면체. 5.8에서도 실험 기능이라 게임 루프의 전제가 아니다. |
| 삼킨 뒤의 액체 | Niagara Fluids. 몸 자체는 파티클 유체가 아니다. 유체에 조작을 맡기면 포식 구체와 질량 스케일이 깨진다. |

새 5.8 프로젝트는 Substrate가 기본이다. 슬라임 재질은 Substrate 슬래브로 만든다.

## 3. 프로젝트

`D:\UnrealGames\RisingSlime`의 현재 상태다.

- 엔진 연결: `RisingSlime.uproject`의 `EngineAssociation`은 설치된 5.8 엔진 GUID다. 에디터 버전은 5.8.3으로 연다.
- 템플릿: Blank, 블루프린트. `DefaultEngine.ini`의 게임 이름 리다이렉트 원본이 `TP_BlankBP`다. `Source` 폴더와 게임 모듈은 아직 없다.
- 콘텐츠: `Content`에는 개발자 컬렉션만 있다.
- 시작 맵: `GameDefaultMap=/Engine/Maps/Templates/OpenWorld`. 이 오픈월드 템플릿 맵에 사냥터를 만들지 않는다. `Content/Maps/Hunt`를 만들고 `GameDefaultMap`과 `EditorStartupMap`을 `/Game/Maps/Hunt`로 바꾼다.
- 이미 켜진 렌더: Substrate, Lumen 글로벌 일루미네이션, Lumen 리플렉션, Virtual Shadow Maps, 하드웨어 레이 트레이싱, DX12 SM6, 데스크톱 최대 품질.
- 이미 연결된 입력: `DefaultInput.ini`의 기본 입력 클래스가 `EnhancedPlayerInput`과 `EnhancedInputComponent`다. 같은 항목을 다시 넣지 않는다.
- `DefaultGame.ini`에 CommonUI 콘솔 변수가 있다. 소화실과 HUD는 UMG로 만든다.
- 플랫폼: Windows, 1인.
- 고정 시뮬레이션 간격: 0.02초. 유니티 `FixedUpdate`와 같다. 점프 높이는 이 간격에 의존하지 않고 중력 적분에 의존하므로, 가변 틱에서 0.02초 이하로 여러 번 적분한다.

C++ 모듈은 에디터에서 첫 클래스를 추가할 때 생긴다. 모듈 이름은 리다이렉트와 같은 `RisingSlime`이다. `uproject`에 `Modules`가 생기고 `Source/RaisingSlime`이 만들어지면, 그 뒤에 섹션 5의 클래스를 넣는다. 블루프린트만으로 출정 규칙과 스프링을 옮기지 않는다.

아직 없는 플러그인:

- Procedural Mesh Component. 1차 젤리 메시용이다.
- Niagara Fluids (Beta). 잔여 액체에만 쓴다. 1차 완료의 조건이 아니다.
- Chaos Flesh. 2차 몸 작업 때 켠다.
- Python Editor Script Plugin, Editor Scripting Utilities. 레벨 배치 스크립트용이다. 런타임에 돌지 않는다.

아직 없는 렌더 설정:

- MegaLights. 5.8에서 프로덕션 지원이며, 반투명은 프로셀 볼륨으로 받는다. 레이 트레이싱은 이미 켜져 있다.
- `r.Substrate.OpaqueMaterialRoughRefraction`은 끈다. MegaLights와 함께 쓰지 않는다.

유니티 `Camera.fieldOfView`는 세로 각이고, 런타임 값은 78이다. 씬 빌더가 95를 넣지만 `SlimeBody.Start`가 78로 덮어쓴다. 78이 세로 기준이다.

5.8의 `UCameraComponent::FieldOfView`는 항상 가로 각이다. `AspectRatio_MaintainYFOV`는 그 가로 값을 세로로 해석하지 않는다. 카메라의 `AspectRatio`로 가로 값에서 세로 각을 만든 뒤, 창 비율이 바뀌어도 그 세로 각을 유지한다. Field Of View에 78을 넣으면 16:9에서 세로 각은 약 49도가 된다.

16:9에서 세로 78이 되는 가로 각은 110.43이다. 카메라에서 직접 덮어쓴다.

- `bOverrideAspectRatioAxisConstraint = true`
- `AspectRatioAxisConstraint = AspectRatio_MaintainYFOV`
- `AspectRatio = 1.777778`
- `bConstrainAspectRatio = false`
- `FieldOfView = 110.43`

`D:\UnrealGames\RisingSlime\Config\DefaultEngine.ini`에 추가한다. 입력 클래스 두 줄은 이미 `DefaultInput.ini`에 있다.

```ini
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Game/Maps/Hunt.Hunt
EditorStartupMap=/Game/Maps/Hunt.Hunt

[/Script/Engine.LocalPlayer]
AspectRatioAxisConstraint=AspectRatio_MaintainYFOV

[/Script/Engine.Engine]
NearClipPlane=5.0
```

`AspectRatioAxisConstraint`를 `[/Script/Engine.Engine]`에 넣으면 적용되지 않는다. 읽는 클래스는 `ULocalPlayer`다.

니어 클립은 카메라 속성이 아니다. 프로젝트 전역 `UEngine::NearClipPlane`이고 단위는 cm다. 유니티 런타임 값은 0.05 m라서 5 cm로 둔다. 퍼스펙티브 파 클립은 카메라 컴포넌트에 없고, 이 맵은 60 m라 별도로 250 m를 맞출 필요가 없다.

## 4. 단위

설계 표의 길이, 속도, 중력은 유니티와 같은 미터다. 언리얼에 넣을 때 길이만 ×100 해서 센티미터로 둔다. 시간, 각도, 체력, 질량, 확률, 배율은 그대로다.

게임플레이 질량은 킬로그램이 아니다. 유니티 `Rigidbody.mass = 1.4`는 힘을 받지 않는 이동이라 결과 속도에 쓰이지 않는다. 언리얼에서도 바디를 시뮬레이트하지 않으므로 이 1.4를 Chaos 질량으로 넣지 않는다.

젤리 정점 버퍼의 단위는 cm다. 섹션 13의 길이 상수는 100을 곱하고, 속도는 cm/s를 넣는다. 강성 30과 감쇠 3.8은 그대로 둔다. `a = -k x`에서 위치와 가속도가 같은 길이 단위를 쓰면 k는 바뀌지 않는다. 길이만 미터로 남기면 접촉 반경 0.18이 반지름 50 cm인 구 위에서 거의 점이 된다.

| 유니티 | 언리얼 |
| --- | --- |
| 1 m | 100 cm |
| 중력 -20 m/s² (슬라임) | -2000 cm/s² |
| 중력 -9.81 m/s² (개체) | -981 cm/s² |
| 이동 6.2 m/s | 620 cm/s |

슬라임과 개체의 중력이 다르다. 슬라임은 코드의 `-20`, 개체는 `Physics.gravity`다.

## 5. 모듈과 클래스

모듈 하나는 `RaisingSlime`이다. 게임 규칙은 C++에 둔다. 블루프린트는 레벨 배치, 메시, 머티리얼, 위젯 상속, 이펙트 참조만 가진다.

```
Source/RaisingSlime/
  RaisingSlimeTypes.h
  RaisingSlimeGameMode.h/.cpp
  RaisingSlimePlayerController.h/.cpp
  ExpeditionDirector.h/.cpp
  SlimePawn.h/.cpp
  SlimeMovementComponent.h/.cpp
  JellyDeformerComponent.h/.cpp
  Devourable.h/.cpp
  CreatureAgent.h/.cpp
  NestZone.h/.cpp
  SlimeShot.h/.cpp
  SlimeHud.h/.cpp
```

| 유니티 | 언리얼 | 책임 |
| --- | --- | --- |
| `GameTypes` | `RaisingSlimeTypes.h` | 형질, 먹이 종류, 한글 이름 |
| `ExpeditionDirector` | `AExpeditionDirector` | 질량, 형질, 흡수, 정착, 사망, 월드 리셋 |
| `SlimeBody` | `ASlimePawn` + `USlimeMovementComponent` | 입력, 시야, 공격, 포식, 체력, 몸 크기 |
| `Jelly*` | `UJellyDeformerComponent` | 포즈, 스프링, 메시 기록 |
| `Devourable` | `ADevourable` | 먹이 데이터, 삼킴, 원위치 복구 |
| `CreatureAgent` | `ACreatureAgent` | 배회, 추격, 공격, 독, 쓰러짐 |
| `NestZone` | `ANestZone` | 둥지 입장 |
| `SlimeShot` | `ASlimeShot` | 구체 투사체 |
| `SlimeHud` | `ARaisingSlimeHUD` + UMG | 바, 조준, 소화실 |
| `HuntSceneBuilder` | `Content/Python/build_hunt.py` | 회색 상자 레벨 생성 |

`ARaisingSlimeGameMode`의 기본 폰은 `ASlimePawn`, 기본 컨트롤러는 `ARaisingSlimePlayerController`, HUD 클래스는 `ARaisingSlimeHUD`다.

`DefaultPawnClass`는 `PlayerStart` 위치에 폰을 새로 스폰한다. `PlayerStart`를 변환된 `NestSpawn`과 같은 트랜스폼에 둔다. 레벨에 `ASlimePawn`을 또 배치하면 폰이 둘이 된다.

`RaisingSlime.Build.cs`의 공개 의존성:

- `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`
- `UMG`, `Slate`, `SlateCore`
- 젤리 단계부터 `ProceduralMeshComponent`

`ChaosFleshEngine`은 2차 단계에서만 넣고, 플러그인을 켠 뒤에 컴파일한다. 1차 모듈이 이 실험 플러그인에 의존하지 않는다.

`AExpeditionDirector`와 `ANestZone`은 레벨에 하나다. 폰은 `PossessedBy`에서 디렉터에 자신을 등록한다. `BeginPlay` 순서에 의존하지 않는다. 시각 시계는 `GetWorld()->GetTimeSeconds()`다.

매핑 컨텍스트는 플레이어 컨트롤러 `BeginPlay`에서 `UEnhancedInputLocalPlayerSubsystem::AddMappingContext`로 넣는다. 시야는 폰이 직접 누적한다. `AddYawInput`, `AddPitchInput`은 쓰지 않는다. 쓰면 컨트롤러 기본 배율 2.5가 한 번 더 곱해진다. 폰의 `bUseControllerRotationPitch`, `Yaw`, `Roll`은 모두 끈다.

### 형질

```cpp
UENUM(BlueprintType)
enum class ETraitId : uint8
{
    None,
    Harden, // 경화
    Bounce, // 탄성
    Poison  // 독
};

UENUM(BlueprintType)
enum class EPreyKind : uint8
{
    Object,
    Creature
};
```

이름과 힌트는 유니티 `TraitText`와 같은 문자열이다.

| 형질 | 이름 | 힌트 |
| --- | --- | --- |
| Harden | 경화 | 우클릭으로 잠시 피해가 줄어든다 |
| Bounce | 탄성 | 공중 추가 점프, 우클릭으로 미끄러지듯 전진 |
| Poison | 독 | 좌클릭 공격에 독이 실린다 |
| None | 없음 | 빈 문자열 |

## 6. 출정 규칙

`AExpeditionDirector`가 가진다.

| 필드 | 의미 | 초기값 |
| --- | --- | --- |
| `SettledMass` | 정착 질량. 죽어도 남는다 | 0 |
| `RunMass` | 이번 출정 질량 | 0 |
| `Confirmed` | 확정 형질. 순서 유지 | 빈 배열 |
| `Unstable` | 불안정 형질. 순서 유지 | 빈 배열 |
| `Active` | 지금 효과에 쓰는 형질 | None |
| `Pity` | 형질별 연속 실패 횟수 | 빈 맵 |
| `LastEatTime` | 마지막 삼킴 시각(초) | 0 |

```
TotalMass     = SettledMass + RunMass
SlotCapacity  = 1 + floor(SettledMass / 10)
ActiveIsConfirmed = Active != None && Confirmed에 Active가 있다
```

가진 형질 목록은 `Confirmed` 순서 뒤에, 아직 없는 `Unstable`을 붙인 것이다. Q는 이 목록을 순환한다.

### 흡수 확률

```
PreviewChance(prey):
    trait == None 이면 0
    fails = Pity[trait]
    compat = Confirmed에 trait가 있으면 1.2, 아니면 1
    hunger = saturate((now - LastEatTime) / 20) * 0.1
    chance = prey.BaseChance * compat * (1 + fails * 0.35) + hunger
    return clamp(chance, 0.02, 0.9)
```

`CompleteDevour(prey)`:

1. `already = Confirmed 또는 Unstable에 prey.Trait가 있다`
2. `Trait != None` 이고 `already`가 아니면 `PreviewChance`로 한 번 굴린다.
3. `RunMass += prey.Mass`, `LastEatTime = now`
4. 형질이 없거나 이미 가지고 있으면 질량만 오른다.
5. 성공하면 `Unstable`에 넣고 `Active`를 그 형질로 바꾸고, 그 형질의 Pity를 0으로 만든다.
6. 실패하면 그 형질의 Pity를 1 올린다.
7. 먹이를 비활성화하고, 폰에 삼킴 충격 `0.16`을 주고 체력·크기를 갱신한다.

확정된 형질은 `already`라서 다시 굴리지 않는다. 호환 배율 1.2는 미리보기 식에는 있으나 성공 판정까지 도달하지 않는다. 이 동작을 유지한다.

상태 문장:

- 형질 없음, 또는 이미 보유: `"{Label}을(를) 삼켰다. 질량 +{mass:0.#}. 새 형질은 없다."`
- 성공: `"{Label}의 {형질이름} 흡수 성공 ({확률}%). 귀환 전에는 불안정하다."`
- 실패: `"{Label}을(를) 삼켰다. {형질이름} 흡수 실패 ({확률}%). 질량만 늘었다."`

확률 표시는 `round(chance * 100)` 정수다. 질량 표시는 소수 한 자리까지 반올림한 문자열이다.

### 전환, 확정, 토해내기

`CycleActive`: 보유 목록이 비면 Active는 None. 아니면 현재 Active의 다음 항목이다. 상태 문장은 `"활성 형질: {이름} (확정)"` 또는 `"(불안정)"`.

`TryConfirm(trait)`:

- Unstable에 없으면 실패.
- 이미 Confirmed에 있으면 Unstable에서만 빼고 성공. 문장: `"{이름}은(는) 이미 확정되어 있다."`
- `Confirmed.Num() >= SlotCapacity`이면 실패하고 Unstable에 남긴다. 문장: `"슬롯이 가득 찼다. 하나를 토해내야 새 형질을 확정할 수 있다."`
- 그 외에는 Unstable에서 빼서 Confirmed 끝에 넣고 Active로 만든다. 문장: `"{이름} 확정. 이후 출정에도 남는다."`

`Spit(trait)`: Confirmed에서 제거한다. 월드에 아이템으로 떨어뜨리지 않는다. Active가 그 형질이었으면 남은 보유 목록의 첫 항목, 없으면 None. 문장: `"{이름}을(를) 토해냈다."`

### 정착과 사망

`Settle`:

- 불안정 개수를 세고 `Unstable`을 비운다.
- `SettledMass += RunMass`, `RunMass = 0`
- Active가 Confirmed에 없으면 보유 목록의 첫 항목으로 바꾼다.
- 모든 먹이를 원위치로 되돌리고 개체를 되살린다.
- 슬라임을 풀피로 회복하고 둥지 스폰으로 보낸 뒤 이동을 멈춘다.
- 문장: 버린 형질이 있으면 `"질량을 정착했다. 확정하지 않은 형질 {n}개는 몸에 남지 않았다."` 없으면 `"질량을 정착했다. 다음 출정을 나갈 수 있다."`

`Die`:

- `RunMass = 0`, `Unstable`을 비운다. `SettledMass`와 `Confirmed`는 유지한다.
- Active가 확정이 아니면 보유 목록의 첫 항목으로 바꾼다.
- 먹이 복구, 풀피 회복, 둥지 스폰.
- 문장: `"몸이 풀렸다. 이번 출정의 질량과 불안정 형질은 사라졌다."`

시작 문장: `"둥지에서 나가 사물과 쓰러뜨린 개체를 삼켜라."`

## 7. 슬라임 수치

`TotalMass`를 `M`으로 둔다.

```
MaxHp   = 100 + M * 5
BodySize = clamp(1.15 + M * 0.05, 1.15, 3.2)
Speed   = clamp(6.2 - M * 0.07, 3.4, 6.2)     // m/s
Reach   = 2.15 + M * 0.03                       // m, 조준 레이
```

질량이 늘 때 `MaxHp`가 오르면 현재 체력도 같은 양만큼 오른다. 질량이 줄 때는 상한만 줄이고 현재 체력을 상한으로 자른다. `Recover`는 그 다음 체력을 `MaxHp`로 채운다. 정착과 사망은 `Recover`를 타므로 풀피가 된다. 삼키기는 `Refresh`만 타므로, 늘어난 상한만큼만 체력이 오른다.

확정된 탄성이 Active이면 이동 속도에 0.6 m/s를 더한다. 불안정 탄성은 이 보너스가 없다.

몸 크기 3.2는 질량 41에서 막힌다. 속도 3.4는 질량 40에서 막힌다.

### 몸 계층

액터 원점은 유니티 루트와 같다. 스폰 높이는 0.16 m다. 루트는 `USceneComponent`다. 구체 콜라이더를 루트로 두면 중심이 원점에 가서, 반지름 57.5 cm인 구의 절반이 땅 밑에 들어간다. 유니티 콜라이더는 자식 `Body`에 있고 로컬 높이는 0.55 m다.

- `Collision`: 루트의 자식 `USphereComponent`. 상대 위치 (0, 0, 55) cm. 반경 `BodySize * 50` cm.
- 시뮬레이트 피직스는 끈다. 매 프레임 `SetPhysicsLinearVelocity`로 속도를 덮어쓰지 않는다. Chaos 중력 -980과 코드 중력 -2000이 동시에 들어간다.
- Collision Enabled는 Query Only. Object Type은 Pawn. WorldStatic, WorldDynamic은 Block. 폰 채널은 Ignore.
- 시각 메시 `Body`도 상대 위치 (0, 0, 55) cm, 스케일 `BodySize`.
- 카메라 `MainCamera`도 루트의 자식이다. 스프링암 랙은 없다. 유니티 로컬 (오른쪽, 위, 앞) = `(0, size * 2.4, -size * 2.55)` m 이다. 언리얼 로컬은 (앞, 오른쪽, 위)이므로

```
CameraLocal = (-BodySize * 255, 0, BodySize * 240) cm
```

`FRotator`의 Pitch는 양수가 위, 음수가 아래다. 유니티 카메라 오일러 X의 양수는 아래이므로 부호를 뒤집는다. 기본 피치는 -22, 범위는 -70에서 50이다. 마우스 Y는 마우스가 위로 갈 때 양수이고, 그 값에 0.22를 곱해 피치에 더한다.

이동 스윕은 이 구체의 월드 중심과 반경으로 `SweepSingleByChannel`을 호출하고, 맞은 거리만큼만 액터를 옮긴다. `UCharacterMovementComponent`의 걷기·점프 공식은 쓰지 않는다. 중력과 점프가 속도 대입이라 `JumpZVelocity`로는 유니티 궤적이 나오지 않는다.

채널:

| 쿼리 | 채널 | 막는 것 |
| --- | --- | --- |
| 이동 스윕 | Pawn | WorldStatic, WorldDynamic |
| 조준, 공격 레이, 투사체 스윕 | Visibility | 월드, 먹이, 개체. 슬라임 액터는 `AddIgnoredActor` |
| 포식 오버랩 | Pawn | 먹이 콜라이더가 Pawn 쿼리에 걸리게 Overlap 또는 Block |

먹이 콜라이더가 Pawn을 Ignore이면 오버랩 쿼리 결과가 비고 포식이 시작되지 않는다. 유니티 기본 레이어에는 이 구분이 없다.

니어 클립은 섹션 3의 전역 값이다. 카메라 컴포넌트에 클립 거리를 넣지 않는다.

### 입력

Enhanced Input. 매핑 컨텍스트는 `/Game/Input/IMC_Slime`. 시야는 픽셀 델타에 0.22를 곱한다. 유니티 기본 Mouse 감도 0.1에 `LookSensitivity` 2.2를 곱한 값이라, 픽셀당 0.22도다.

이 프로젝트 `DefaultInput.ini`는 `bEnableLegacyInputScales=True`이고 MouseX, MouseY, Mouse2D 감도가 0.07이다. 그 0.07이 시야 델타에 곱해진 뒤 액션에서 0.22를 또 곱하면 픽셀당 0.0154도가 된다. 시야는 로우 픽셀 × 0.22만 사용한다.

| 액션 | 키 | 동작 |
| --- | --- | --- |
| `IA_Move` | WASD | 평면 이동. 카메라 요 기준 |
| `IA_Look` | 마우스 | 요, 피치 |
| `IA_Jump` | Space | 점프 예약. 누른 프레임만 |
| `IA_Attack` | 마우스 왼쪽 | 투사체 |
| `IA_Trait` | 마우스 오른쪽 | 활성 형질 |
| `IA_CycleTrait` | Q | 형질 순환 |
| `IA_Digest` | E | 둥지 안에서 소화실 |
| `IA_Cursor` | Escape | 메뉴가 열려 있으면 닫기. 아니면 커서 해제 |
| `IA_Focus` | 마우스 왼쪽 | 커서가 풀린 상태에서 다시 잠금 |

시작 시 커서를 잠그고 숨긴다. 입력 모드는 Game Only다. 소화실이 열리거나 Escape로 풀리면 Game And UI, 커서를 보인다. 소화실이 열린 동안 둥지 밖에 나가면 메뉴를 닫고 다시 잠근다.

메뉴가 열려 있으면 이동 속도를 0으로 두고, 공격·포식·점프·형질을 받지 않는다. 채널도 0으로 끊는다.

## 8. 이동

누적기는 폰 한 곳에 둔다. 이동 컴포넌트와 젤리 컴포넌트가 각자 Tick에서 0.02를 소비하지 않는다. 남은 시간이 0.02를 넘으면 0.02씩 소비하고, 한 프레임의 서브스텝은 5회로 막는다. 나머지는 다음 프레임으로 넘긴다. 각 서브스텝 안에서 이동 적분 다음에 젤리 `Step`이 온다. 젤리에 렌더 프레임 dt를 넣지 않는다. 강성 30인 준암시 오일러는 큰 dt에서 발산한다.

월드 기본 중력 -980 cm/s²는 이 적분과 더하지 않는다. 슬라임 중력은 서브스텝마다 -2000 cm/s²만 더한다.

상태: 평면 속도, 수직 속도, 요, 추가 점프 가능, 점프 예약, 지면, 벽 노멀.

가속은 유니티 `Vector3.MoveTowards`와 같다.

```
input = WASD, 길이 1로 제한
desired = yaw 회전 * input * Speed
rate = input이 있으면 16, 없으면 36          // m/s²
planar = MoveTowards(planar, desired, rate * dt)
```

벽: 벽 노멀의 수직 성분을 버리고 정규화한다. `dot(planar, wall) < 0`이면 벽으로 파고드는 성분을 뺀다. 벽은 스윕이 수평 충돌을 돌려준 프레임과 그 다음 프레임만 유효하다.

중력과 점프. 단위는 m/s, m/s².

```
지면에서 점프:     수직 속도 = 확정 탄성 ? 8.2 : 7.2
공중이고 탄성:     추가 점프 한 번, 수직 속도 = 확정 ? 7.6 : 6.6
공중:              수직 속도 += -20 * dt
지면에 붙어 있고 수직 속도 < 0: 수직 속도 = 0
지면에 붙는 프레임: 추가 점프를 다시 연다
```

지면 판정은 이동 스윕이 돌려준 노멀의 Z가 0.55보다 큰 경우다. 프로브의 시작점은 구체 바닥이고, 액터 원점 아래 8 cm가 아니다. 구체 중심은 이미 바닥에서 55 cm 위에 있다. 떨어진 뒤 0.12초까지는 지면으로 친다. 유니티 `groundedUntil`과 같다.

수직 속도가 1.5 m/s를 넘으면 상승 중이다. 상승 중에는 접촉 변형과 지면 눌림을 적용하지 않는다.

탄성 우클릭은 점프와 별도다. Active가 Bounce일 때 전방 수평 속도를 확정 7.5 m/s, 불안정 5 m/s만큼 더한다. 쿨다운은 없다.

요는 이동 적분과 같은 고정 단계가 아니라 입력 틱에서 바로 돌린다. 유니티도 `Update`에서 요를 바꾸고 `FixedUpdate`에서 그 요로 이동한다.

## 9. 공격

좌클릭 간격 0.42초. 카메라에서 앞 30 m 레이를 쏜다. 자신과 자신의 자식은 무시한다. 첫 히트 지점이 조준점이다. 히트가 없으면 카메라 앞 30 m 지점이다.

발사 원점은 `Body` 위치에서 조준 방향으로 `BodySize * 0.7` m만큼 전진한 곳이다.

`ASlimeShot`:

| 값 | 수 |
| --- | --- |
| 속도 | 18 m/s. 독이어도 속도는 같다 |
| 수명 | 2.2초 |
| 판정 반경 | 0.16 m |
| 시각 구체 지름 | 0.28 m |
| 피해 | 독이 확정이면 18, 아니면 14 |
| 독 | Active가 Poison일 때만 |
| 독 DPS | 확정 8, 불안정 4 |
| 독 시간 | 확정 3.2초, 불안정 2.2초 |

독이 아니면 DPS와 시간은 무시한다. 투사체 머티리얼은 슬라임 몸과 같은 머티리얼 인스턴스다.

속도는 1800 cm/s. 중력은 없다. `UProjectileMovementComponent`를 쓰면 `ProjectileGravityScale`을 0으로, `InitialSpeed`를 1800으로 둔다. 기본값은 중력이 켜져 있다. 수명 동안 직진하고, 그 프레임 이동 거리만큼 반경 16 cm 구체 스윕을 한다. 슬라임과 다른 투사체는 무시한다. 그 외 첫 충돌에서 사라진다. 맞은 것이 쓰러지지 않은 `ACreatureAgent`이면 `TakeHit`을 호출한다. 벽과 먹이 사물에도 막힌다.

피격 `TakeDamage(amount)`:

- 체력이 0 이하면 무시.
- `now < HardenUntil`이면 피해에 확정 0.3, 불안정 0.5를 곱한다.
- 체력을 깎고 포식 채널을 0으로 끊는다.
- 충격량은 `clamp(amount * 0.012, 0.08, 0.42)`.
- 체력이 0 이하면 `Die`.

경화 우클릭은 Active가 Harden일 때만 동작한다.

| | 지속 | 피해 배율 | 쿨다운 |
| --- | --- | --- | --- |
| 확정 | 6초 | 0.3 | 7초 |
| 불안정 | 3.5초 | 0.5 | 7초 |

쿨다운 중에는 입력을 버린다.

## 10. 포식

매 틱, 메뉴가 닫혀 있으면 `Body` 위치에서 반경 `BodySize * 0.78` m 오버랩을 한다. 자신의 콜라이더는 제외한다. `ADevourable` 중 `CanDevour`인 것 가운데 가장 가까운 하나를 고른다.

`CanDevour`:

- 활성이고 아직 삼키지 않았다.
- 사물은 항상 가능하다.
- 개체는 `ACreatureAgent.IsDowned`일 때만 가능하다.

채널 시간: 사물 0.22초, 개체 0.85초. 대상이 바뀌거나 없어지면 채널은 0이다. 같은 대상을 유지한 채 시간이 쌓여 채널 시간에 도달하면 `CompleteDevour`를 호출한다. 버튼은 없다. 몸에 붙이면 삼킨다.

조준 안내는 공격 레이와 별도다. 카메라 앞으로 `Reach`만큼 레이를 쏴서 첫 `ADevourable`을 `Aimed`로 둔다. 포식 오버랩과 달아서, 멀리 있는 먹이의 이름만 보여 준다.

삼킴 충격 `AddImpulse`: 로컬 위치는 아래 0.12 m에 해당하는 벡터, 방향은 위, 크기 `0.16 * 6`, 반경 0.55 m, 스쿼시 강도는 설정값, 늘림 강도도 설정값. 피격 충격도 같은 함수다.

## 11. 개체

`ACreatureAgent`. 벌이 첫 개체다.

| 값 | 벌 |
| --- | --- |
| MaxHp | 36 |
| MoveSpeed | 3.4 m/s |
| AttackDamage | 12 |
| AttackInterval | 1.2초 |
| AttackRange | 1.8 m, 평면 거리 |
| NoticeRange | 18 m, 평면 거리 |
| 배회 반경 | 2.2 m |
| 캡슐 | `SetCapsuleSize(40, 100)`. 반경 40 cm, 하프 하이트 100 cm. 하프 하이트에 200을 넣으면 키가 4 m가 된다 |
| 배치 | 유니티 (13.5, 1, 12.5) m. 중심 높이 1 m라 발이 땅에 붙는다 |

쓰러지기 전에는 매 틱 독을 적용한 뒤 움직인다. 평면 거리가 Notice 이하면 플레이어 위치로, 아니면 집 주변으로 간다.

```
homeOffset = (sin(time * 0.45), cos(time * 0.35), 0) * 220 cm
```

오프셋의 세 번째 성분은 위가 아니다. 언리얼 수평은 X, Y다. Z에 넣으면 벌이 뜬다. 평면 거리는 `FVector::DistXY`다.

목적지가 0.2 m보다 멀면 그 방향으로 속력 `MoveSpeed`만큼 이동하고, 회전은 `Slerp` 각속 8 rad/s에 가깝게 보간한다. 유니티는 `Quaternion.Slerp(..., 8 * dt)`라 8이 초당 라디안 한도가 아니다. `dt`가 0.016이면 계수는 0.128이다. `FMath::QInterpTo`의 속도 8로 두면 느낌이 달라지므로, `FQuat::Slerp(current, target, clamp(8 * dt, 0, 1))`로 맞춘다.

중력은 -981 cm/s²만 직접 적분한다. 땅에 붙어 있고 낙하 속도가 음수면 -200 cm/s로 붙인다. 유니티 `CharacterController`가 지면에서 수직 속도를 살짝 음수로 유지하는 값이다. 개체에 `UCharacterMovementComponent`를 붙이면 이 중력 외에 브레이크와 스텝이 더해진다.

공격은 평면 거리 ≤ 1.8 m이고 간격이 지났으면 슬라임에게 12를 준다. 시야 검사와 방향 검사는 없다.

`TakeHit`:

- 쓰러진 상태면 무시.
- 체력을 피해만큼 깎는다.
- 독이면 DPS와 종료 시각을 덮어쓴다. 중첩하지 않는다. 색은 `(0.55, 0.9, 0.2)`.
- 체력이 0 이하면 쓰러뜨린다. 색은 `(0.35, 0.32, 0.18)`. 독은 끈다.

독 틱은 `Hp -= Dps * dt`이고, 0 이하면 쓰러뜨린다. 쓰러지면 AI를 멈춘다. 콜라이더는 켜 둔다. 포식 판정이 이 콜라이더를 찾아야 한다.

`Revive`는 체력 회복, 쓰러짐 해제, 독 해제, 공격 쿨 해제, 낙하 속도 -2, 틴트 해제다. 월드 리셋에서 호출한다.

색은 머티리얼 파라미터 `Color`에 쓴다. 다이내믹 머티리얼 인스턴스 하나로 충분하다.

## 12. 먹이 데이터와 레벨

좌표는 미터다. 언리얼 배치 때는 ×100이고, 유니티 Y가 언리얼 Z, 유니티 Z가 언리얼 X가 되도록 축을 돌린다. 앞뒤가 바뀌지 않게 스폰 시점의 앞을 +X로 둔다.

권장 변환: 유니티 `(x, y, z)` → 언리얼 `(z * 100, x * 100, y * 100)` cm. 이 변환에서 유니티의 앞(+Z)이 언리얼 앞(+X)이다. 스케일도 같은 축으로 돌린다. 유니티 `(sx, sy, sz)` → 언리얼 `(sz, sx, sy)`. 돌 스케일 (0.70, 0.55, 0.70)은 언리얼 (0.70, 0.70, 0.55)이라 눌리는 축이 위다. 위치만 변환하고 스케일 숫자를 그대로 쓰면 돌과 풀이 옆으로 눕는다. 레벨 스크립트는 이 함수만 쓴다.

방향광 오일러 (48, -32, 0)은 축과 회전 순서가 달라 그대로 넣지 않는다. 유니티 `Quaternion.Euler` 결과를 축 변환한 쿼터니언으로 옮긴다. 해가 비스듬히 땅을 비추는 것만 같으면, 언리얼 피치 -48 전후에서 요를 맞춘다. 양수 피치는 위를 본다.

### 먹이

| 이름 | 메시 | 위치 m | 스케일 | 질량 | 종류 | 형질 | 기본 확률 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 돌 `Prey_Rock` | 구체 | (6.2, 0.42, 5.4) | (0.70, 0.55, 0.70) | 2 | 사물 | 경화 | 0.62 |
| 고무 `Prey_Rubber` | 큐브 | (-4.2, 0.34, 6.6) | 0.62 | 2 | 사물 | 탄성 | 0.58 |
| 벌 `Prey_Bee` | 캡슐 | (13.5, 1.0, 12.5) | 1 | 8 | 개체 | 독 | 0.18 |

유니티 기본 구체·큐브의 한 변은 1 m다. 엔진 기본 구체와 큐브도 지름·한 변이 100 cm이므로, 축 변환한 스케일을 그대로 쓴다. 유니티 원기둥은 높이 2 m, 반지름 0.5 m이고, 엔진 기본 원기둥은 높이 100 cm, 반지름 50 cm다. 둥지 패드의 결과 크기는 높이 8 cm, 반지름 210 cm이므로 엔진 원기둥 스케일은 (4.2, 4.2, 0.08)이다. 유니티 스케일 (4.2, 0.04, 4.2)를 축만 바꿔 (4.2, 4.2, 0.04)로 넣으면 높이가 4 cm가 된다.

벌의 보이는 캡슐은 유니티 기본 캡슐이라 높이 2 m, 반지름 0.5 m다. 판정 캡슐은 그와 다르다. 판정은 반경 40 cm, 하프 하이트 100 cm다. 엔진 기본 캡슐 메시의 기본 크기가 높이 2 m가 아니면, 메시 스케일로 높이 200 cm, 반지름 50 cm를 맞춘다.

각 먹이는 시작 트랜스폼을 저장한다. `Restore`는 활성, 위치, 회전, 개체 부활을 되돌린다.

### 맵

바닥은 60 m × 60 m 평면, 원점, 색 `(0.34, 0.52, 0.26)`.

둥지 패드: 원기둥, 위치 (0, 0.04, 0) m, 스케일 (4.2, 0.04, 4.2). 유니티 원기둥은 키 2, 반지름 0.5라서 결과 키 0.08 m, 반지름 2.1 m다. 색 `(0.18, 0.62, 0.68)`.

`NestZone`: 박스 트리거, 중심 (0, 1.4, 0) m, 크기 (8, 3, 8) m. 슬라임 루트가 들어오면 `PlayerInside`. 스폰 직후와 텔레포트 직후에는 박스 포함 검사로 한 번 갱신한다. 검사점은 루트에서 위 1 m다.

`NestSpawn`: (0, 0.16, 0) m, 회전 없음.

방향광은 무버블, 색 `(1, 0.96, 0.86)`, 강도 1.15, 소프트 섀도다. 회전은 위의 축 변환을 따른다. 스카이라이트는 유니티 트라이라이트에 대응한다.

| | 색 |
| --- | --- |
| 하늘 | (0.62, 0.70, 0.78) |
| 적도 | (0.42, 0.48, 0.36) |
| 땅 | (0.22, 0.20, 0.16) |

장식은 콜라이더가 없다. 돌만 콜라이더가 있다. 돌에 부딪히고, 풀은 통과한다.

| 이름 | 위치 m | 스케일 | 색 |
| --- | --- | --- | --- |
| Stone_01 | (9.5, 0.45, -1.5) | 0.85 | 돌 (0.48, 0.48, 0.50) |
| Stone_02 | (-7, 0.55, 9) | 1.05 | 돌 |
| Stone_03 | (16, 0.40, 4) | 0.70 | 돌 |
| Grass_01 | (2.4, 0.28, 2.2) | (0.12, 0.50, 0.12) | (0.18, 0.48, 0.16) |
| Grass_02 | (3.6, 0.34, 3.1) | 같은 풀 | 풀 |
| Grass_03 | (5.1, 0.24, 7.2) | 같은 풀 | 풀 |
| Grass_04 | (8.4, 0.30, 3.4) | 같은 풀 | 풀 |
| Grass_05 | (11.2, 0.36, 8.1) | 같은 풀 | 풀 |
| Grass_06 | (-2.2, 0.26, 4.4) | 같은 풀 | 풀 |
| Grass_07 | (-5.5, 0.32, 8.8) | 같은 풀 | 풀 |
| Grass_08 | (14.8, 0.30, 15.2) | 같은 풀 | 풀 |
| Bloom_01 | (1.8, 0.16, 3.5) | 0.28 | 고무 (0.90, 0.42, 0.16) |
| Bloom_02 | (7.4, 0.16, 6.8) | 0.28 | 둥지 색 |

돌 러프니스 0.82, 고무 0.38, 벌 0.60, 둥지 0.52, 풀·바닥 0.88에서 0.92. 유니티 Standard의 Gloss를 언리얼 러프니스로 뒤집으면 `1 - gloss`에 가깝다. 바닥 gloss 0.12, 돌 0.18, 고무 0.62, 벌 0.40, 둥지 0.48, 풀 0.08.

`D:\UnrealGames\RisingSlime\Content\Python\build_hunt.py`는 에디터에서 수동 실행한다. 스타트업 스크립트로 등록하지 않는다. 스폰은 `EditorActorSubsystem`을 쓴다. `EditorLevelLibrary`는 5.4 이후 `EditorActorSubsystem`이 권장 경로다. 이 스크립트가 `ASlimePawn`을 레벨에 심지 않는다. 폰은 `PlayerStart`에서 게임 모드가 만든다. 배치 대상 레벨은 `/Game/Maps/Hunt`다.

## 13. 젤리 몸

1차 구현은 유니티 솔버의 C++ 이식이다. 해상도만 올린다. 이동 구체와 분리되어 있으므로 정점을 늘려도 볼록 헐 제한이 없다.

### 휴식 메시

용접 구체. 극점 두 개와 고리마다 `longitude`개.

```
기본 목표: latitude 24, longitude 32
정점 수 = (latitude - 1) * longitude + 2 = 738
여유 목표: latitude 32, longitude 48 = 1490
```

메시 로컬은 유니티 `(x, y, z)`를 언리얼 `(x, z, y)`로 저장한다. 위는 로컬 Z, 수평은 로컬 X·Y다. 정점은 cm로 만든다. 반지름 50 cm 구에 수평 1.05, 위 0.92를 곱한다. 약간 넓고 낮은 덩어리다. 유니티 앞면은 시계 방향이고, 언리얼 앞면은 반시계 방향이다. 인덱스를 그대로 가져오면 노멀이 안쪽을 향하므로 삼각형 순서를 뒤집는다.

메시는 `UProceduralMeshComponent`에 만들고, 매 프레임은 `UpdateMeshSection`으로 정점 위치만 갱신한다. `UDynamicMeshComponent::NotifyMeshUpdated`를 매 프레임 호출하면 렌더 프록시를 다시 만든다. 1차 경로로 쓰지 않는다. 인덱스는 고정이다.

노멀은 머티리얼 그래프가 정점 노멀을 대신 계산해 주지 않는다. `VertexNormalWS`는 버퍼에 들어 있는 값이고, World Position Offset은 그 노멀을 교체하지 못한다. 738정점의 노멀은 워커 스레드에서 삼각형으로 계산해 `UpdateMeshSection`에 함께 넘긴다. 메인 스레드에서 그 루프를 돌리지 않는다. 화면 기울기 `dFdx(WorldPosition)`는 면 단위 노멀이라 1차 품질 기준이 아니다.

### 설정

`FJellySettings` 기본값. 유니티 `JellySettings.Default`와 같은 미터 값이다. 정점 버퍼가 cm이면 길이 필드만 ×100이다. 강성, 감쇠, 배율은 그대로다.

| 필드 | 값 |
| --- | --- |
| SpringStrength | 30 |
| Damping | 3.8 |
| NeighborStrength | 12 |
| MaximumDeformation | 0.28 m |
| MaximumStretch | 1.65 |
| CollisionDeformation | 0.7 |
| MovementDeformation | 1.15 |
| RecoverySpeed | 1 |
| VolumePreservation | 0.35 |
| SquashStrength | 1.1 |
| StretchStrength | 1.15 |
| MaxContactDeformation | 0.07 m |
| MaxImpactDeformation | 0.16 m |
| MaxRadialExpansion | 0.045 m |
| MaxVerticalCompression | 0.12 m |
| ContactRadius | 0.18 m |
| ContactStrength | 1 |
| ContactFalloff | 1 |

1차에서 켜는 변형:

- 접촉 변형: 켬
- 지면 접촉: 켬
- 착지 충격: 켬
- 충격 변형, 부피 보존, 방사 확장, 지면 방사 퍼짐: 끔

유니티 `deformationVersion` 3의 런타임 기본과 같다. 부피 보존은 Flesh 단계에서 켠다. 설정 필드는 미리 둔다.

지면 접촉 튜닝:

| 필드 | 값 |
| --- | --- |
| GroundDeformationHeight | 0.22 m |
| GroundContactStrength | 0.05 m |
| GroundContactFalloff | 1 |
| MaxGroundCompression | 0.06 m |
| LandingImpactStrength | 0.012 |
| MaxLandingDeformation | 0.12 m |
| LandingVelocityThreshold | 1.5 m/s |
| LandingBounceStrength | 1 |
| LandingRecoverySpeed | 16 |

### 고정 단계 순서

이동 적분이 끝난 뒤에 젤리가 속도를 읽는다.

1. 이번 물리 단계에서 모은 접촉을 활성 버퍼로 승격하고 대기 버퍼를 비운다.
2. 월드 속도와 가속도를 메시 로컬로 변환한다.
3. `BuildPose`
4. 상승 중이 아니고 접촉 변형이 켜져 있으면 접촉마다 `ApplyContactPose`. 압력은 `1 / sqrt(접촉 수)`로 나눈다. 끝나면 `ClampPose`.
5. `ApplyGround`
6. `ApplyInertia`
7. 스프링 `Step`
8. 렌더 직전 `WriteMesh`

접촉은 최대 8개. 같은 로컬 점이 0.004 m 안이면 더 큰 압력·반경으로 갱신한다. 지면은 월드 노멀 Z가 가장 큰 접촉 하나다. 착지 속도는 그 노멀 방향 상대 속도의 하강 성분이고, 첫 접촉에서만 갱신한다.

### BuildPose

로컬 속도 `v`의 위 성분은 `v.z`다.

```
elongate = min(abs(v.z) * StretchStrength * 0.16, 2.1)
pinch    = elongate * 0.42
planarSpeed = length(v.x, v.y)
trail    = min(planarSpeed * MovementDeformation * 0.1, MaximumDeformation)
```

각 휴식 정점 `r`에 대해

```
offset = (-r.x * pinch, -r.y * pinch, r.z * elongate)
```

평면 이동이 있으면 이동 방향과 `normalize(r)`의 내적에 `trail`을 곱해 offset에 더한다. offset 길이가 `MaximumDeformation`을 넘으면 자른다. `Pose = Rest + offset`. 영향도는 0으로 초기화한다.

### ApplyContactPose

로컬 점, 로컬 노멀, 압력, 반경, 방사 한도, 감쇠 지수. 1차의 방사 한도는 0이다.

노멀을 단위 벡터 `dent`로 둔다. 휴식 정점이 원점에서 `dent`의 반대쪽을 0.2 미만으로 보면 건너뛴다. 휴식 정점과 접촉 점의 거리가 반경 안일 때만

```
t = 1 - distance / radius
influence = smoothstep(t) * inverseLerp(0.2, 0.85, facing)
smoothstep(t) = t * t * (3 - 2 * t)
```

`falloff > 1.01`이면 influence를 그 지수로 제곱한다. 0.001 이하는 버린다. 정점 영향도는 최대값을 유지한다. 포즈에 `dent * press * influence`를 더한다.

접촉 압력의 계산:

```
penetration = max(0, -separation) / bodyScale
depth = penetration * ContactStrength
resting = MaxContactDeformation * 0.28
depth = max(depth, resting)
depth = min(depth, MaxContactDeformation)
```

상대 바디 질량 가중은 1차에서 1로 둔다. 맵의 먹이와 돌은 시뮬레이트되지 않는다.

### ApplyGround

지면에 새로 붙는 프레임의 하강 속도와, 접촉이 기록한 착지 속도 중 큰 값을 쓴다. 그 값이 `LandingVelocityThreshold` 이상이면

```
peak = min(impactSpeed * LandingImpactStrength, MaxLandingDeformation)
omega = max(1, LandingRecoverySpeed)
landingVelocity += peak * omega * LandingBounceStrength
```

지면이 없거나 지면 접촉이 꺼져 있으면 `landingPress`와 `landingVelocity`를 `1 - exp(-8 * dt)`로 0에 당긴다.

착지 스프링:

```
omega = max(1, LandingRecoverySpeed)
damping = 2 * 0.42 * omega
accel = -omega * omega * landingPress - damping * landingVelocity
landingVelocity += accel * dt
landingPress += landingVelocity * dt
landingPress = clamp(landingPress, -MaxLanding * 0.35, MaxLanding)
```

압축량:

```
compression = min(GroundContactStrength, MaxGroundCompression) + landingPress
cap = MaxGroundCompression + MaxLandingDeformation
compression = clamp(compression, -MaxLanding * 0.35, cap)
```

`ApplyGroundPose`는 접촉과 같은 영향도다. 거리만 점 거리가 아니라 지면 점에서 노멀 방향으로 잰 높이이고, 대역은 `min(GroundDeformationHeight, 0.35)` m다. 포즈에 `normal * compression * influence`를 더하고 `ClampPose`한다.

`ClampPose`는 수평 오프셋 길이와 수직 오프셋을 각각 `MaximumDeformation`으로 제한한다.

### 스프링

고유 간선은 삼각형 세 변에서 만든다. `a < b`로 정렬해 중복을 제거한다. 휴식 길이는 휴식 정점 거리다.

각 고정 단계:

1. 현재 위치가 휴식 위치에서 수평·수직으로 `MaximumDeformation`을 넘으면 그 한도로 당긴다.
2. 힘 = `-SpringStrength * RecoverySpeed * (current - pose) - Damping * velocity`
3. 각 간선에 대해 `(currentDelta - poseDelta) * NeighborStrength`를 a에 더하고 b에서 뺀다.
4. `velocity += force * dt`. 속력 한도는 `MaximumDeformation * 12`.
5. `current += velocity * dt`
6. 간선 길이가 `posedLength * MaximumStretch`보다 길거나 `restLength / MaximumStretch`보다 짧으면 양쪽을 반씩 당긴다. `posedLength`는 포즈 간선과 휴식 간선 중 긴 쪽이다.

부피 보존이 꺼져 있으면 그 단계는 호출하지 않는다.

### 관성

로컬 속도 변화 `delta`에 대해

```
kick = -delta * MovementDeformation
```

각 정점은 `kick * lerp(0.35, 1, facing) * StretchStrength`를 속도에 더한다. facing은 휴식 방향과 kick 방향의 내적을 0.5 기준으로 펼친 값이다.

### 충격

`AddImpulse(localPoint, localDirection, magnitude, radius, squash, stretch)`:

방향의 반대쪽을 0.2 미만으로 보는 정점은 제외한다. 반경 안에서

```
falloff = 1 - smoothstep(distance / radius)
velocity += direction * magnitude * squash * falloff
```

측면 성분이 있으면 `stretch` 방향으로 `magnitude * stretch * falloff * 0.28`을 더한다.

`ResetMotion`은 속도, 착지 스프링, 포즈, 현재 정점, 정점 속도를 휴식으로 되돌리고 메시를 한 번 쓴다. 텔레포트와 회복에서 호출한다.

### 텔레포트

루트를 `NestSpawn`에 두고 요를 스폰 요로, 언리얼 피치를 -22로, 평면·수직 속도를 0으로, 젤리를 `ResetMotion`으로 맞춘다. 직후 `NestZone` 포함 검사를 다시 한다. 스폰 지점이 트리거 안에 있어서, 오버랩 이벤트만 기다리면 메뉴 키가 안 먹는다. 텔레포트는 오버랩에 들어가지 않는다.

### Chaos Flesh는 2차다

1차 수용 기준을 통과한 뒤에 실험한다. 플러그인 이름은 Chaos Flesh, 모듈은 `ChaosFleshEngine`이다. 5.8 파이썬 API에 `DeformableSolverComponent`와 `DeformableTetrahedralComponent`가 있다. 프로덕션 지원으로 승격된 기능은 아니다.

Flesh는 절차적 메시 버퍼를 갈아 끼우는 솔버가 아니다. 구성은 다음이다.

- 닫힌 표면의 스태틱 메시 또는 단일 본 스켈레탈 메시를 만든다. 런타임 절차 메시를 솔버 입력으로 넣지 않는다.
- Dataflow에서 사면체를 하나 고른다. 구는 radial 또는 iso-surface stuffing이 맞고, TetWild는 표면이 거칠 때의 선택이다. 엔진이 이 순서로 자동 폴백하지 않는다.
- 시뮬 공간은 `ChaosDeformableSimSpace.LOCAL`이고, 기준 본은 스켈레탈 메시의 루트 본이다. 로컬 공간은 본 이름 없이 폰 트랜스폼에 붙는 기능이 아니다.
- 표면 변형은 `GenerateSurfaceBindings`와 Flesh deformer가 스켈레탈 메시에 적용한다.
- 비압축을 켠다. 유니티의 방사 밀어내기를 이 부피 항으로 대체하는 것이 목적이다.
- 게임플레이 스윕 점을 매 프레임 키네마틱 구속으로 넣는 입력은 기본 경로가 아니다. 구속은 보통 스켈레톤에서 온다. 접촉 눌림을 구속으로 만드는 작업은 별도 실험이고, 여기서 막히면 1차 스프링을 유지한다.

서브스텝을 올리기 전에 Unreal Insights로 젤리 구간을 본다. 프레임당 1 ms를 넘기면 1차로 남긴다. 게임 규칙은 Flesh 없이 이미 닫혀 있다.

## 14. 머티리얼

머티리얼 `M_Slime`. 새 프로젝트의 Substrate를 쓴다.

1차 색 맞춤과 Substrate 라이팅은 한 그래프에 더하지 않는다. 유니티 식은 이미 랩 라이팅과 스펙큘러를 포함한다. 그 식을 Substrate Slab의 라이팅 위에 또 더하면 빛이 두 번 들어간다.

색을 유니티와 같게 맞추는 동안에는 아래 식의 rgb를 슬래브 Emissive에 넣고, Diffuse Albedo는 검게 두어 라이팅이 한 번 더 더해지지 않게 한다. 새 5.8 프로젝트에는 예전 Shading Model Unlit 핀이 없고 Substrate 출력이 그 자리다.

```
facing  = saturate(dot(view, normal))
wrap    = saturate(dot(normal, light)) * 0.45 + 0.55
fresnel = pow(1 - facing, 2.6)
spec    = pow(saturate(dot(reflect(-light, normal), view)), 48)
rgb     = lerp(Base, Core, facing * 0.55) * wrap
rgb    += fresnel * (0.85, 1.0, 0.8)
rgb    += spec * 0.7
alpha   = lerp(0.42, 0.9, facing)
```

| 파라미터 | 값 |
| --- | --- |
| Base | (0.45, 0.92, 0.36, 0.86) |
| Core | (0.04, 0.38, 0.08, 1) |

그 다음 단계에서 이 식을 빼고 슬래브가 빛을 받게 한다.

- Blend Mode: `TranslucentColoredTransmittance`
- Lighting Mode: `Surface Translucency Volume`. MegaLights의 반투명은 이 프로셀 볼륨으로 들어오고, 픽셀 단위 로컬 라이트는 아니다.
- Diffuse Albedo에 Base, Transmittance Color에 Core, Opacity에 알파 식을 연결한다.
- IOR 1.33은 근사다. 반투명 굴절은 불투명 Rough Refraction과 다른 경로다.
- Two Sided를 켜고, 뒷면 노멀은 `TwoSidedSign`으로 뒤집는다.

먹이 머티리얼은 불투명 Substrate 슬래브, 섹션 12의 색과 러프니스다. 발광은 0이다.

폰트는 프로젝트에 포함한다. 맑은 고딕은 윈도우에만 있다. 본문은 Noto Sans KR 또는 Pretendard 16 pt다.

## 15. HUD

`ARaisingSlimeHUD`가 `WBP_SlimeHud`를 뷰포트에 붙인다. 해상도에 따라 픽셀 위치가 달라지지 않게, 유니티 `OnGUI`의 픽셀 값을 1920×1080 기준 앵커로 다시 적는다. 기준 해상도에서 아래 픽셀과 같으면 된다.

왼쪽 위:

- (24, 20) 280×18 체력 바. 배경 검정 알파 0.55, 채움 `(0.75, 0.18, 0.20)`.
- (24, 40) `체력 {ceil(Hp)}`
- (24, 64) `정착 질량 {settled:0.#}    이번 출정 {run:0.#}`
- (24, 86) `슬롯 {confirmed}/{capacity}`
- (24, 108) 활성 형질. 없으면 `활성 형질 없음`. 있으면 `활성 {이름} (확정)` 또는 `(불안정)`.
- (24, 130) 활성 형질의 힌트.

중앙:

- 십자 16×2, 2×16. 포식 중이면 `(0.45, 0.95, 0.40)`, 아니면 흰색.
- 포식 중: `"{Label}을(를) 몸으로 삼키는 중"`. 형질이 있으면 `"    흡수 {확률}%"`. 그 아래 160×10 바, 채움 `(0.40, 0.90, 0.45)`, 값은 `Channel01`.
- 조준만 된 개체이고 아직 못 삼키면: `"{Label}  체력 {ceil}  쓰러뜨린 뒤 몸을 붙여 삼킨다"`
- 조준한 삼킬 수 있는 대상: `"{Label}  몸을 붙여 삼킨다"`와 흡수 확률.

아래:

- 바닥에서 58 px: `WASD 이동  마우스 시점  좌클릭 공격  우클릭 형질  Q 형질 전환  몸에 닿으면 포식  E 소화실  Esc 커서`
- 메뉴가 닫혀 있고 둥지 안이면 바닥에서 82 px: `둥지 안이다. E로 소화실을 연다.`
- 바닥에서 112 px: `ExpeditionDirector.Status`

소화실 `WBP_Digest`. 화면 중앙 460×380. 제목 `소화실`.

- `이번 출정 질량 {run}  /  정착 {settled}`
- `확정 슬롯 {n}/{capacity}`
- 불안정 형질 목록. 없으면 `없음`. 각 행에 이름과 `확정` 버튼.
- 확정된 형질 목록. 없으면 `없음`. 각 행에 이름과 `토해내기` 버튼.
- `질량 정착하고 먹이 되돌리기`는 `Settle` 후 메뉴를 닫는다.
- `닫기`

버튼은 한 번만 먹게, 클릭 순간에 디렉터 함수를 호출하고 위젯을 상태로부터 다시 그린다. 배열을 위젯이 복사해 들고 있지 않는다.

## 16. 구현 순서

각 단계가 끝나야 다음 단계의 수용 기준을 본다. 젤리 고해상도와 Flesh는 규칙이 맞은 뒤에 한다.

1. `D:\UnrealGames\RisingSlime`에 C++ 모듈 `RaisingSlime`을 추가한다. `Content/Maps/Hunt`를 만들고 시작 맵을 오픈월드 템플릿에서 Hunt로 바꾼다. 게임 모드, 시야, 마우스 잠금을 붙인다. Enhanced Input 클래스는 이미 연결되어 있다.
2. `Content/Python/build_hunt.py`로 바닥, 둥지, 스폰, 빛, 장식, 먹이 세 개를 배치한다.
3. 폰 이동. 질량 0에서 점프 높이, 추가 점프 없음, 벽에서 미끄러짐, 메뉴 중 정지.
4. 디렉터와 사물 포식. 돌·고무의 확률, Pity, 질량, 상태 문장.
5. 벌. 추격, 피해, 독, 쓰러진 뒤 0.85초 포식, 사망 시 출정 질량 소멸.
6. 소화실. 확정, 슬롯, 토해내기, 정착, 먹이 리스폰, 풀피.
7. 체력·속도·크기·카메라가 질량을 따른다.
8. 젤리 738정점. 착지 눌림, 가속 시 늘어남, 삼킴 충격.
9. `M_Slime` 색 맞춤 후 굴절.
10. 여유 해상도 1490, 그 다음 Flesh 실험.

Niagara Fluids 방울은 10번 이후의 잔여 작업이다. 삼킴 성공, 토해내기, 사망 각 한 시스템이면 된다. 시뮬 영역은 몸 주변 2 m를 넘기지 않는다.

## 17. 수용 기준

측정은 질량 0, 고정 단계 0.02초, 해상도 738에서 한다.

이동:

- 수평 최대 속력 6.2 m/s. 입력을 놓으면 36 m/s²로 줄어 약 0.172초 안에 선다.
- 평지 점프의 수직 속도는 7.2 m/s, 정점 높이 `7.2² / (2 * 20) = 1.296` m, 체공 `0.72`초.
- 탄성 없이 공중 점프는 되지 않는다.
- 불안정 탄성의 공중 점프 수직 속도는 6.6, 확정은 7.6이다.
- 확정 탄성의 지면 점프는 8.2, 이동 속도는 6.8 m/s다.
- 질량 40에서 이동 속도는 3.4 m/s, 질량 41에서 몸 스케일은 3.2에서 멈춘다.

출정:

- 돌만 삼키면 RunMass 2, MaxHp 110, 현재 체력도 10이 오른다.
- 경화 실패가 쌓이면 다음 확률이 `0.62 * (1 + fails * 0.35)`에 허기를 더한 뒤 0.02와 0.9 사이다.
- 이미 가진 형질은 질량만 오른다.
- 슬롯은 정착 질량 0에서 1칸, 10에서 2칸이다. 가득 찬 확정은 실패하고 불안정이 남는다.
- 정착하면 RunMass가 Settled로 옮겨지고, 확정하지 않은 형질은 사라지며, 먹이 셋이 처음 위치에 돌아온다.
- 사망하면 Settled와 Confirmed만 남고 체력은 새 MaxHp로 찬다.
- 벌는 체력 36. 독 없이 두 발(14+14)과 세 번째 일부, 또는 확정 독 한 발 18 뒤에 DPS 8로 쓰러진다. 쓰러지기 전에는 포식 채널이 오르지 않는다.
- 경화 확정 중 벌의 12 피해는 3.6으로 들어온다. 불안정 중에는 6이다.
- 공격 연사는 0.42초보다 빠르지 않다.

몸:

- 카메라의 언리얼 피치 기본은 -22, 범위는 -70에서 50이다. 16:9 세로 시야는 78이고, 카메라 Field Of View 값은 110.43이다.
- 착지 속도 1.5 m/s 미만에서는 착지 스프링이 돌지 않는다.
- 수직 속도 1.5 m/s를 넘는 상승 중에는 바닥에 닿아도 지면 눌림을 넣지 않는다.
- 젤리 정점이 250을 넘겨도 이동 구체 판정은 그대로다.
- 젤리 길이 상수는 cm로 환산되어 있다. 접촉 반경은 18 cm, 변형 상한은 28 cm다.
- 노멀 계산은 워커 스레드에서 하고, 메인 스레드에서 삼각형 전부를 돌지 않는다.

UI 문장은 섹션 6과 15의 한글과 같다.

## 18. 원본 대조

수치가 이 문서와 코드 사이에서 어긋나면 코드를 다시 읽고 문서를 고친 뒤에 언리얼에 반영한다.

| 파일 | 확인할 것 |
| --- | --- |
| `Assets/Scripts/GameTypes.cs` | 형질 이름, 힌트 |
| `Assets/Scripts/ExpeditionDirector.cs` | 확률, 슬롯, 정착, 사망 |
| `Assets/Scripts/SlimeBody.cs` | 이동, 공격, 포식, 체력, 카메라 |
| `Assets/Scripts/SlimeShot.cs` | 투사체 |
| `Assets/Scripts/CreatureAgent.cs` | 벌 AI, 독 |
| `Assets/Scripts/Devourable.cs` | 채널 시간, 복구 |
| `Assets/Scripts/NestZone.cs` | 트리거 |
| `Assets/Scripts/SlimeHud.cs` | 문장, 레이아웃 |
| `Assets/Scripts/Jelly/JellySettings.cs` | 스프링 기본값 |
| `Assets/Scripts/Jelly/JellyController.cs` | 지면·착지 기본값, 단계 순서 |
| `Assets/Scripts/Jelly/JellyMeshDeformer.cs` | 메시, 포즈, 충격 |
| `Assets/Scripts/Jelly/JellySpringSolver.cs` | 간선, 제약 |
| `Assets/Scripts/Jelly/JellyCollisionHandler.cs` | 접촉 버퍼, 지면 |
| `Assets/Editor/HuntSceneBuilder.cs` | 배치, 먹이 수치 |
| `Assets/Shaders/SlimeFluid.shader` | 색 식 |
