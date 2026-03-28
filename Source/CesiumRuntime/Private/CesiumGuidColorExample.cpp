// Copyright 2020-2025 CesiumGS, Inc. and Contributors
// JSCZ - CesiumGuidColorManager 使用示例实现

#include "CesiumGuidColorExample.h"

#include "Cesium3DTileset.h"
#include "CesiumGuidColorManager.h"

#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

#pragma region JSCZ

// ============================================================================
//  构造函数
// ============================================================================
ACesiumGuidColorExample::ACesiumGuidColorExample() {
  PrimaryActorTick.bCanEverTick = false;
  TargetTileset = nullptr;
  ColorManager = nullptr;
}

// ============================================================================
//  BeginPlay — 游戏开始时自动初始化
// ============================================================================
void ACesiumGuidColorExample::BeginPlay() {
  Super::BeginPlay();

  // 初始化颜色管理器
  SetupColorManager();

  // ===== 以下为内联使用示例（可删除或注释掉） =====

  // ---------- 示例：直接在 BeginPlay 中设置颜色 ----------
  if (ColorManager) {
    // 示例A：将两个 GUID 设为红色高亮
    TArray<FString> HighlightGuids;
    HighlightGuids.Add(TEXT("building-guid-001"));
    HighlightGuids.Add(TEXT("building-guid-002"));
    ColorManager->SetGuidColors(
        HighlightGuids,
        FLinearColor(1.0f, 0.0f, 0.0f, 1.0f)); // 红色

    // 示例B：将另一批 GUID 设为蓝色
    TArray<FString> BlueGuids;
    BlueGuids.Add(TEXT("building-guid-003"));
    ColorManager->SetGuidColors(
        BlueGuids,
        FLinearColor(0.0f, 0.3f, 1.0f, 1.0f)); // 蓝色

    // 注意：此时新加载的瓦片会自动使用这些颜色。
    // 如果瓦片已经加载完成，需要调用 RefreshAllColors() 刷新：
    ColorManager->RefreshAllColors();
  }
}

// ============================================================================
//  初始化颜色管理器
// ============================================================================
void ACesiumGuidColorExample::SetupColorManager() {
  // 步骤1：如果未手动指定 Tileset，自动在关卡中查找第一个
  if (!TargetTileset) {
    for (TActorIterator<ACesium3DTileset> It(GetWorld()); It; ++It) {
      TargetTileset = *It;
      break; // 使用找到的第一个 Tileset
    }
  }

  if (!TargetTileset) {
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[JSCZ] ACesiumGuidColorExample: 未找到 Cesium3DTileset，"
             "请在编辑器中设置 TargetTileset 属性"));
    return;
  }

  // 步骤2：检查 Tileset 上是否已有 ColorManager 组件
  ColorManager =
      TargetTileset->FindComponentByClass<UCesiumGuidColorManager>();

  if (!ColorManager) {
    // 步骤3：动态创建 UCesiumGuidColorManager 并附加到 Tileset
    ColorManager = NewObject<UCesiumGuidColorManager>(
        TargetTileset,                          // Outer = Tileset Actor
        UCesiumGuidColorManager::StaticClass(), // 组件类
        TEXT("GuidColorManager"));              // 组件名称

    // 设置 GUID 属性名（根据数据源调整）
    ColorManager->GuidPropertyName = GuidPropertyName;

    // 设置默认颜色（未匹配 GUID 的要素显示白色半透明）
    ColorManager->DefaultColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.2f);

    // 禁用自动注册（因为我们要手动控制注册时机）
    ColorManager->bAutoRegister = false;

    // 注册组件到引擎（必须调用，否则组件不会生效）
    ColorManager->RegisterComponent();

    // 附加到 Tileset 的根组件
    ColorManager->AttachToComponent(
        TargetTileset->GetRootComponent(),
        FAttachmentTransformRules::KeepRelativeTransform);

    // 步骤4：手动注册到 Tileset 的生命周期事件
    ColorManager->RegisterWithTileset(TargetTileset);
  }

  UE_LOG(
      LogTemp,
      Log,
      TEXT("[JSCZ] ACesiumGuidColorExample: 颜色管理器初始化完成，"
           "GUID属性名=%s"),
      *ColorManager->GuidPropertyName);
}

// ============================================================================
//  演示1：高亮指定 GUID
// ============================================================================
void ACesiumGuidColorExample::Demo_HighlightGuids(
    const TArray<FString>& Guids) {
  if (!ColorManager) {
    return;
  }

  // 先清除之前的高亮
  ColorManager->ClearAllGuidColors();

  // 设置选中的 GUID 为红色高亮
  ColorManager->SetGuidColors(
      Guids,
      FLinearColor(1.0f, 0.0f, 0.0f, 1.0f)); // 红色，完全不透明

  // 立即刷新已加载的瓦片
  ColorManager->RefreshAllColors();

  UE_LOG(
      LogTemp,
      Log,
      TEXT("[JSCZ] 已高亮 %d 个 GUID"),
      Guids.Num());
}

// ============================================================================
//  演示2：清除高亮
// ============================================================================
void ACesiumGuidColorExample::Demo_ClearHighlight() {
  if (!ColorManager) {
    return;
  }

  // 清除所有颜色映射
  ColorManager->ClearAllGuidColors();

  // 刷新显示
  ColorManager->RefreshAllColors();

  UE_LOG(LogTemp, Log, TEXT("[JSCZ] 已清除所有高亮"));
}

// ============================================================================
//  演示3：分类着色
// ============================================================================
void ACesiumGuidColorExample::Demo_ClassifyBuildings(
    const TArray<FString>& ResidentialGuids,
    const TArray<FString>& CommercialGuids,
    const TArray<FString>& IndustrialGuids) {
  if (!ColorManager) {
    return;
  }

  // 清除旧的颜色映射
  ColorManager->ClearAllGuidColors();

  // 住宅 → 绿色
  if (ResidentialGuids.Num() > 0) {
    ColorManager->SetGuidColors(
        ResidentialGuids,
        FLinearColor(0.2f, 0.8f, 0.2f, 0.8f)); // 绿色
  }

  // 商业 → 蓝色
  if (CommercialGuids.Num() > 0) {
    ColorManager->SetGuidColors(
        CommercialGuids,
        FLinearColor(0.2f, 0.4f, 1.0f, 0.8f)); // 蓝色
  }

  // 工业 → 黄色
  if (IndustrialGuids.Num() > 0) {
    ColorManager->SetGuidColors(
        IndustrialGuids,
        FLinearColor(1.0f, 0.8f, 0.0f, 0.8f)); // 黄色
  }

  // 刷新所有已加载瓦片
  ColorManager->RefreshAllColors();

  UE_LOG(
      LogTemp,
      Log,
      TEXT("[JSCZ] 分类着色完成：住宅=%d, 商业=%d, 工业=%d"),
      ResidentialGuids.Num(),
      CommercialGuids.Num(),
      IndustrialGuids.Num());
}

// ============================================================================
//  演示4：数值热力图（绿→黄→红渐变）
// ============================================================================
void ACesiumGuidColorExample::Demo_HeatmapVisualization(
    const TMap<FString, float>& GuidValues) {
  if (!ColorManager) {
    return;
  }

  // 清除旧的颜色映射
  ColorManager->ClearAllGuidColors();

  // 逐个 GUID 设置颜色
  for (const auto& Pair : GuidValues) {
    const FString& Guid = Pair.Key;
    // 将数值限制在 0.0~1.0 范围内
    float Value = FMath::Clamp(Pair.Value, 0.0f, 1.0f);

    // 颜色插值：
    //   0.0 → 绿色 (0, 1, 0)
    //   0.5 → 黄色 (1, 1, 0)
    //   1.0 → 红色 (1, 0, 0)
    FLinearColor Color;
    if (Value < 0.5f) {
      // 绿色 → 黄色
      float T = Value * 2.0f; // 0.0~1.0
      Color = FLinearColor::LerpUsingHSV(
          FLinearColor(0.0f, 0.8f, 0.0f, 0.9f),  // 绿色
          FLinearColor(1.0f, 0.9f, 0.0f, 0.9f),  // 黄色
          T);
    } else {
      // 黄色 → 红色
      float T = (Value - 0.5f) * 2.0f; // 0.0~1.0
      Color = FLinearColor::LerpUsingHSV(
          FLinearColor(1.0f, 0.9f, 0.0f, 0.9f),  // 黄色
          FLinearColor(1.0f, 0.0f, 0.0f, 0.9f),  // 红色
          T);
    }

    // 单个 GUID 也需要用数组形式传入
    TArray<FString> SingleGuid;
    SingleGuid.Add(Guid);
    ColorManager->SetGuidColors(SingleGuid, Color);
  }

  // 刷新所有已加载瓦片
  ColorManager->RefreshAllColors();

  UE_LOG(
      LogTemp,
      Log,
      TEXT("[JSCZ] 热力图着色完成，共 %d 个 GUID"),
      GuidValues.Num());
}

#pragma endregion

// ============================================================================
//  以下为独立函数示例，展示纯 C++ 代码中的最简用法
//  （不需要继承 AActor，可在任意位置调用）
// ============================================================================

/*
 * ========== 最简用法示例（复制粘贴即可使用） ==========
 *
 * // ---- 前提：你有一个 ACesium3DTileset* 指针 ----
 * ACesium3DTileset* MyTileset = ...; // 你的 Tileset 引用
 *
 * // ---- 步骤1：创建并附加颜色管理器 ----
 * UCesiumGuidColorManager* ColorMgr = NewObject<UCesiumGuidColorManager>(
 *     MyTileset,
 *     UCesiumGuidColorManager::StaticClass(),
 *     TEXT("MyColorManager"));
 *
 * // 设置 GUID 属性名（根据你的 3DTiles 数据调整）
 * ColorMgr->GuidPropertyName = TEXT("guid");
 *
 * // 注册组件
 * ColorMgr->bAutoRegister = false;
 * ColorMgr->RegisterComponent();
 * ColorMgr->AttachToComponent(
 *     MyTileset->GetRootComponent(),
 *     FAttachmentTransformRules::KeepRelativeTransform);
 *
 * // 注册到 Tileset 的生命周期事件
 * ColorMgr->RegisterWithTileset(MyTileset);
 *
 * // ---- 步骤2：设置颜色 ----
 * TArray<FString> RedGuids = { TEXT("id-001"), TEXT("id-002") };
 * ColorMgr->SetGuidColors(RedGuids, FLinearColor::Red);
 *
 * TArray<FString> BlueGuids = { TEXT("id-003") };
 * ColorMgr->SetGuidColors(BlueGuids, FLinearColor::Blue);
 *
 * // ---- 步骤3（可选）：刷新已加载瓦片 ----
 * ColorMgr->RefreshAllColors();
 *
 * // ---- 之后新加载的瓦片会自动显示对应颜色 ----
 *
 * // ---- 修改颜色 ----
 * ColorMgr->RemoveGuidColors(RedGuids);          // 移除红色
 * ColorMgr->SetGuidColors(RedGuids, FLinearColor::Green); // 改为绿色
 * ColorMgr->RefreshAllColors();                   // 刷新显示
 *
 * // ---- 清除所有颜色 ----
 * ColorMgr->ClearAllGuidColors();
 * ColorMgr->RefreshAllColors();
 */

/*
 * ========== 编辑器内操作步骤（无需写代码） ==========
 *
 * 1. 在关卡中选中你的 Cesium3DTileset Actor
 * 2. 点击「添加组件」→ 搜索「Cesium GUID Color Manager」
 * 3. 在组件的细节面板中设置：
 *    - GUID Property Name = 你数据中的 GUID 字段名（如 "guid"）
 *    - Default Color = 未匹配要素的默认颜色
 *    - Auto Register = true（自动注册）
 * 4. 运行游戏
 * 5. 通过蓝图调用以下节点：
 *    - Set GUID Colors：设置 GUID 颜色
 *    - Remove GUID Colors：移除 GUID 颜色
 *    - Clear All GUID Colors：清除所有
 *    - Refresh All Colors：刷新显示
 */
