// Copyright 2020-2025 CesiumGS, Inc. and Contributors
// JSCZ - CesiumGuidColorManager 使用示例实现
//
// 本文件演示 UCesiumGuidColorManager 的正确使用方法：
// 1. 确保 Tileset 上有 CesiumFeaturesMetadataComponent（含 FeatureIdSets/PropertyTables 配置）
// 2. 创建并配置 GuidColorManager
// 3. 通过 SetGuidColors() 设置 GUID → 颜色 映射
// 4. 自动或手动刷新颜色

#include "CesiumGuidColorExample.h"

#include "Cesium3DTileset.h"
#include "CesiumFeaturesMetadataComponent.h"
#include "CesiumGuidColorManager.h"

#include "EngineUtils.h"

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
//  BeginPlay — 初始化 + 内联示例
// ============================================================================
void ACesiumGuidColorExample::BeginPlay() {
  Super::BeginPlay();

  // 初始化颜色管理器（内部会检查 CesiumFeaturesMetadataComponent）
  SetupColorManager();

  // ===== 内联使用示例 =====
  // 注意：以下 GUID 是占位值，实际使用时需替换为 PropertyTable 中的真实值
  if (ColorManager) {
    // 示例：将两个 GUID 设为红色
    // GUID 值来源于 PropertyTable 中 GuidPropertyName 属性的实际值
    TArray<FString> HighlightGuids;
    HighlightGuids.Add(TEXT("building-guid-001"));
    HighlightGuids.Add(TEXT("building-guid-002"));
    ColorManager->SetGuidColors(
        HighlightGuids,
        FLinearColor(1.0f, 0.0f, 0.0f, 1.0f)); // 红色

    TArray<FString> BlueGuids;
    BlueGuids.Add(TEXT("building-guid-003"));
    ColorManager->SetGuidColors(
        BlueGuids,
        FLinearColor(0.0f, 0.3f, 1.0f, 1.0f)); // 蓝色

    // 刷新已加载瓦片（新加载的瓦片会自动使用新颜色）
    ColorManager->RefreshAllColors();
  }
}

// ============================================================================
//  初始化颜色管理器（核心设置流程）
// ============================================================================
void ACesiumGuidColorExample::SetupColorManager() {

  // ===== 步骤1：查找 Tileset =====
  if (!TargetTileset) {
    for (TActorIterator<ACesium3DTileset> It(GetWorld()); It; ++It) {
      TargetTileset = *It;
      break;
    }
  }

  if (!TargetTileset) {
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[JSCZ] ACesiumGuidColorExample: 未找到 Cesium3DTileset"));
    return;
  }

  // ===== 步骤2：确保 Tileset 上有 CesiumFeaturesMetadataComponent =====
  //
  // 这是整个 GUID 着色功能的前提条件！
  // CesiumFeaturesMetadataComponent 的 Description 中配置了：
  //   - PrimitiveFeatures.FeatureIdSets: 哪些 FeatureIdSet 需要编码到 GPU
  //   - ModelMetadata.PropertyTables:    哪些 PropertyTable 需要编码
  //
  // 没有这个组件，FeatureID 不会被编码到材质中，着色无法工作。

  UCesiumFeaturesMetadataComponent* FeaturesMetadata =
      TargetTileset->FindComponentByClass<UCesiumFeaturesMetadataComponent>();

  if (!FeaturesMetadata) {
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[JSCZ] Tileset [%s] 上没有 CesiumFeaturesMetadataComponent！"
             "请在编辑器中添加该组件，并通过「Add Properties」配置 "
             "FeatureIdSets 和 PropertyTables。"),
        *TargetTileset->GetName());

    // 注意：CesiumFeaturesMetadataComponent 通常在编辑器中通过
    // 「Add Properties」按钮自动发现和配置 FeatureIdSets / PropertyTables。
    //
    // 如果要在代码中动态创建，可以这样做（但通常不推荐，因为
    // Description 的配置需要匹配实际的 3DTiles 数据结构）：
    //
    // FeaturesMetadata = NewObject<UCesiumFeaturesMetadataComponent>(
    //     TargetTileset,
    //     UCesiumFeaturesMetadataComponent::StaticClass(),
    //     TEXT("FeaturesMetadata"));
    // FeaturesMetadata->RegisterComponent();
    //
    // // 然后需要配置 Description，例如：
    // // FeaturesMetadata->Description.PrimitiveFeatures.FeatureIdSets = ...
    // // FeaturesMetadata->Description.ModelMetadata.PropertyTables = ...
    // // 但具体配置取决于你的 3DTiles 数据，建议在编辑器中操作。

    return;
  }

  UE_LOG(
      LogTemp,
      Log,
      TEXT("[JSCZ] 已找到 CesiumFeaturesMetadataComponent，"
           "FeatureIdSets=%d, PropertyTables=%d"),
      FeaturesMetadata->Description.PrimitiveFeatures.FeatureIdSets.Num(),
      FeaturesMetadata->Description.ModelMetadata.PropertyTables.Num());

  // ===== 步骤3：创建或查找 GuidColorManager =====
  ColorManager =
      TargetTileset->FindComponentByClass<UCesiumGuidColorManager>();

  if (!ColorManager) {
    ColorManager = NewObject<UCesiumGuidColorManager>(
        TargetTileset,
        UCesiumGuidColorManager::StaticClass(),
        TEXT("GuidColorManager"));

    // 配置参数
    ColorManager->FeatureIdSetIndex = FeatureIdSetIndex;
    ColorManager->GuidPropertyName = GuidPropertyName;
    ColorManager->DefaultColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.2f);
    ColorManager->bAutoRegister = false;

    // 注册组件
    ColorManager->RegisterComponent();
    ColorManager->AttachToComponent(
        TargetTileset->GetRootComponent(),
        FAttachmentTransformRules::KeepRelativeTransform);

    // 注册到 Tileset 生命周期事件
    // （内部会检查 CesiumFeaturesMetadataComponent 是否存在）
    ColorManager->RegisterWithTileset(TargetTileset);
  }

  UE_LOG(
      LogTemp,
      Log,
      TEXT("[JSCZ] GuidColorManager 初始化完成："
           "FeatureIdSetIndex=%d, GuidPropertyName=%s"),
      ColorManager->FeatureIdSetIndex,
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

  ColorManager->ClearAllGuidColors();
  ColorManager->SetGuidColors(
      Guids,
      FLinearColor(1.0f, 0.0f, 0.0f, 1.0f)); // 红色
  ColorManager->RefreshAllColors();

  UE_LOG(LogTemp, Log, TEXT("[JSCZ] 已高亮 %d 个 GUID"), Guids.Num());
}

// ============================================================================
//  演示2：清除高亮
// ============================================================================
void ACesiumGuidColorExample::Demo_ClearHighlight() {
  if (!ColorManager) {
    return;
  }

  ColorManager->ClearAllGuidColors();
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

  ColorManager->ClearAllGuidColors();

  if (ResidentialGuids.Num() > 0) {
    ColorManager->SetGuidColors(
        ResidentialGuids,
        FLinearColor(0.2f, 0.8f, 0.2f, 0.8f)); // 绿色
  }

  if (CommercialGuids.Num() > 0) {
    ColorManager->SetGuidColors(
        CommercialGuids,
        FLinearColor(0.2f, 0.4f, 1.0f, 0.8f)); // 蓝色
  }

  if (IndustrialGuids.Num() > 0) {
    ColorManager->SetGuidColors(
        IndustrialGuids,
        FLinearColor(1.0f, 0.8f, 0.0f, 0.8f)); // 黄色
  }

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

  ColorManager->ClearAllGuidColors();

  for (const auto& Pair : GuidValues) {
    const FString& Guid = Pair.Key;
    float Value = FMath::Clamp(Pair.Value, 0.0f, 1.0f);

    // 0.0 → 绿色, 0.5 → 黄色, 1.0 → 红色
    FLinearColor Color;
    if (Value < 0.5f) {
      float T = Value * 2.0f;
      Color = FLinearColor::LerpUsingHSV(
          FLinearColor(0.0f, 0.8f, 0.0f, 0.9f),
          FLinearColor(1.0f, 0.9f, 0.0f, 0.9f),
          T);
    } else {
      float T = (Value - 0.5f) * 2.0f;
      Color = FLinearColor::LerpUsingHSV(
          FLinearColor(1.0f, 0.9f, 0.0f, 0.9f),
          FLinearColor(1.0f, 0.0f, 0.0f, 0.9f),
          T);
    }

    TArray<FString> SingleGuid;
    SingleGuid.Add(Guid);
    ColorManager->SetGuidColors(SingleGuid, Color);
  }

  ColorManager->RefreshAllColors();

  UE_LOG(
      LogTemp,
      Log,
      TEXT("[JSCZ] 热力图着色完成，共 %d 个 GUID"),
      GuidValues.Num());
}

#pragma endregion

// ============================================================================
//  以下为纯注释代码示例，展示完整的使用流程
// ============================================================================

/*
 * ========== 完整使用流程（正确架构） ==========
 *
 * ---- 前提条件 ----
 *
 * ACesium3DTileset* MyTileset = ...; // 你的 Tileset
 *
 * ---- 步骤1：确保 CesiumFeaturesMetadataComponent 已配置 ----
 *
 * // CesiumFeaturesMetadataComponent 是一切的基础。
 * // 它的 Description 告诉引擎哪些 FeatureIdSets 和 PropertyTables
 * // 需要编码到 GPU 材质中。
 * //
 * // 推荐方式：在编辑器中操作
 * //   1. 选中 Tileset Actor
 * //   2. 添加组件 → Cesium Features Metadata
 * //   3. 点击「Add Properties」自动发现并配置
 * //
 * // 或检查是否已存在：
 * UCesiumFeaturesMetadataComponent* FM =
 *     MyTileset->FindComponentByClass<UCesiumFeaturesMetadataComponent>();
 * if (!FM) {
 *     UE_LOG(LogTemp, Error, TEXT("缺少 CesiumFeaturesMetadataComponent！"));
 *     return;
 * }
 *
 * ---- 步骤2：创建并配置 GuidColorManager ----
 *
 * UCesiumGuidColorManager* ColorMgr = NewObject<UCesiumGuidColorManager>(
 *     MyTileset,
 *     UCesiumGuidColorManager::StaticClass(),
 *     TEXT("MyColorManager"));
 *
 * // 设置使用第几个 FeatureIdSet（对应 FM->Description 中的配置）
 * ColorMgr->FeatureIdSetIndex = 0;
 *
 * // 设置 PropertyTable 中哪个属性作为 GUID
 * ColorMgr->GuidPropertyName = TEXT("guid");
 *
 * // 注册组件
 * ColorMgr->bAutoRegister = false;
 * ColorMgr->RegisterComponent();
 * ColorMgr->AttachToComponent(
 *     MyTileset->GetRootComponent(),
 *     FAttachmentTransformRules::KeepRelativeTransform);
 *
 * // 注册到 Tileset 生命周期
 * ColorMgr->RegisterWithTileset(MyTileset);
 *
 * ---- 步骤3：设置 GUID → 颜色 映射 ----
 *
 * // GUID 值 = PropertyTable 中 GuidPropertyName 属性的实际值
 * TArray<FString> RedGuids = { TEXT("id-001"), TEXT("id-002") };
 * ColorMgr->SetGuidColors(RedGuids, FLinearColor::Red);
 *
 * TArray<FString> BlueGuids = { TEXT("id-003") };
 * ColorMgr->SetGuidColors(BlueGuids, FLinearColor::Blue);
 *
 * ---- 步骤4：刷新已加载瓦片 ----
 *
 * ColorMgr->RefreshAllColors();
 * // 新加载的瓦片会自动使用最新颜色，无需手动刷新
 *
 * ---- 修改颜色 ----
 *
 * ColorMgr->RemoveGuidColors(RedGuids);
 * ColorMgr->SetGuidColors(RedGuids, FLinearColor::Green);
 * ColorMgr->RefreshAllColors();
 *
 * ---- 清除所有 ----
 *
 * ColorMgr->ClearAllGuidColors();
 * ColorMgr->RefreshAllColors();
 */

/*
 * ========== 数据流总结 ==========
 *
 *   3DTiles 数据
 *     │
 *     ▼
 *   CesiumFeaturesMetadataComponent (编辑器配置)
 *     ├─ Description.PrimitiveFeatures.FeatureIdSets[]
 *     │    → 编码为材质参数 _FEATURE_ID_N
 *     └─ Description.ModelMetadata.PropertyTables[]
 *          → 包含 GUID 属性（如 "guid"、"id"）
 *     │
 *     ▼
 *   瓦片加载 → CustomizeMaterial 回调
 *     │
 *     ▼
 *   UCesiumGuidColorManager.ReadFeatureGuids()
 *     ├─ GetFeatureIDSets()[FeatureIdSetIndex]
 *     │    → 选择指定的 FeatureIdSet
 *     ├─ GetPropertyTableIndex()
 *     │    → 找到关联的 PropertyTable
 *     └─ GetProperties()[GuidPropertyName]
 *          → 读取每个要素的 GUID 值
 *     │
 *     ▼
 *   BuildColorArray() → GUID 查 GuidColorMap → 颜色数组
 *     │
 *     ▼
 *   CreateColorTexture() → 颜色查找纹理（1像素=1要素）
 *     │
 *     ▼
 *   Material.SetTextureParameterValue() → 设到材质参数
 *     │
 *     ▼
 *   材质 Shader：
 *     _FEATURE_ID_N → 获取当前面的要素ID
 *     → 以要素ID为索引采样颜色纹理
 *     → 得到该面的颜色
 */

/*
 * ========== 编辑器操作步骤（无需代码） ==========
 *
 * 1. 选中 Cesium3DTileset Actor
 *
 * 2. 添加组件 → 搜索「Cesium Features Metadata」→ 添加
 *    - 点击「Add Properties」发现并配置 FeatureIdSets 和 PropertyTables
 *    - 或手动在 Description 中配置
 *
 * 3. 添加组件 → 搜索「Cesium GUID Color Manager」→ 添加
 *    - Feature ID Set Index = 0（使用第一个 FeatureIdSet）
 *    - GUID Property Name = PropertyTable 中的 GUID 属性名
 *    - Default Color = 默认颜色
 *    - Auto Register = true
 *
 * 4. 运行游戏
 *
 * 5. 通过蓝图调用：
 *    - Set GUID Colors: 设置 GUID → 颜色
 *    - Remove GUID Colors: 移除
 *    - Clear All GUID Colors: 清除全部
 *    - Refresh All Colors: 刷新已加载瓦片
 */
