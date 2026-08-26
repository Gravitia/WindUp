# 블루프린트 노출 카테고리

레벨 디자이너가 에디터 디테일 패널에서 **값을 조절하는** `UPROPERTY`(`EditAnywhere` / `EditDefaultsOnly` / `EditInstanceOnly`)의 `Category`는 **`CSEditable|<시스템>`** 으로 쓴다. 하위 묶음이 필요하면 `CSEditable|<시스템>|<그룹>` 까지 쓰고, 그 이상 깊게 만들지 않는다.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|SplineRider|Move")
float MaxMoveSpeed = 600.0f;
```

`<시스템>` 은 클래스 이름에서 `CS` 접두를 뗀 것을 쓴다 (`ACSSplineRider` → `SplineRider`). 한 시스템이 여러 클래스에 걸쳐 있으면 같은 이름으로 묶는다 (`ACSConveyorManager` / `ACSConveyorPlatform` → `Conveyor`).

`"Default"` 나 카테고리 미지정은 쓰지 않는다. 디테일 패널에서 프로젝트 값과 엔진 값이 섞여 보인다.

## 여기에 해당하지 않는 것

`CSEditable` 은 **디자이너가 조절하는 값** 전용이다. 아래는 붙이지 않는다.

- `VisibleAnywhere` 컴포넌트 — `"Components"`
- `BlueprintCallable` 함수 — 기존 이름 유지
- 코드 내부에서만 쓰는 `UPROPERTY` — 카테고리 자체가 필요 없다

## 기존 코드

평문 카테고리는 그 파일을 수정할 때 함께 바꾼다. 카테고리만 바꾸려고 파일을 건드리지 않는다.
