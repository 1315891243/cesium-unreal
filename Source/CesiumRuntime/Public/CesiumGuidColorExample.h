// Copyright 2020-2025 CesiumGS, Inc. and Contributors
// JSCZ - CesiumGuidColorManager 使用示例
// 演示正确的数据流：CesiumFeaturesMetadataComponent → FeatureIdSets
// → PropertyTables → GUID属性值 → 颜色映射 → 材质着色

#pragma once

#pragma region JSCZ

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CesiumGuidColorExample.generated.h"

class ACesium3DTileset;
class UCesiumGuidColorManager;
class UCesiumFeaturesMetadataComponent;

/**
 * CesiumGuidColorExample — GUID颜色管理器使用示例
 *
 * 这是一个演示 Actor，展示如何在 C++ 中使用 UCesiumGuidColorManager
 * 实现 3DTiles 瓦片的基于 GUID 自动着色。
 *
 * ========== 架构说明（正确的数据流） ==========
 *
 * Cesium 3DTiles 的要素数据访问架构：
 *
 *   UCesiumFeaturesMetadataComponent（配置层）
 *     ├─ Description.PrimitiveFeatures.FeatureIdSets[]
 *     │    → 告诉引擎哪些 FeatureIdSet 需要编码到 GPU
 *     │    → 编码后材质中可通过 _FEATURE_ID_N 获取要素ID
 *     │
 *     └─ Description.ModelMetadata.PropertyTables[]
 *          → 告诉引擎哪些 PropertyTable 需要编码
 *          → PropertyTable 存储每个要素的属性（含 GUID 字段）
 *
 *   UCesiumGuidColorManager（着色层）
 *     ├─ FeatureIdSetIndex → 选择第几个 FeatureIdSet
 *     ├─ GuidPropertyName → PropertyTable 中哪个属性作为 GUID
 *     ├─ GuidColorMap → GUID 字符串 → 颜色 映射
 *     └─ CustomizeMaterial 回调 → 生成颜色纹理 → 设到材质参数
 *
 * ========== 使用场景 ==========
 *
 * 场景1：建筑高亮
 *   点击建筑 → 获取其 GUID → SetGuidColors() → 红色高亮
 *
 * 场景2：分类着色
 *   住宅=绿色, 商业=蓝色, 工业=黄色 → 按GUID列表分组着色
 *
 * 场景3：数据可视化
 *   温度/能耗等数值 → 映射为绿→黄→红渐变色
 *
 * ========== 前提条件 ==========
 *
 * 1. Tileset 上必须有 UCesiumFeaturesMetadataComponent：
 *    - Description 中配置了 FeatureIdSets（至少一个）
 *    - Description 中配置了对应的 PropertyTables
 *    - 编辑器中可通过「Add Properties」自动发现并配置
 *
 * 2. PropertyTable 中必须包含一个字符串类型的属性作为 GUID
 *
 * 3. 材质中需包含：
 *    - 纹理参数 CesiumGuidColorTex（颜色查找纹理）
 *    - 向量参数 CesiumGuidColorTexSize（纹理尺寸）
 *    - 通过 _FEATURE_ID_N 获取要素ID，以此索引颜色纹理
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "GUID Color Example"))
class CESIUMRUNTIME_API ACesiumGuidColorExample : public AActor {
  GENERATED_BODY()

public:
  ACesiumGuidColorExample();

  // ========== 可配置属性 ==========

  /**
   * 目标 Tileset Actor 引用。
   * 在编辑器中选择关卡中已有的 Cesium3DTileset。
   * 不设置则 BeginPlay 时自动查找。
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "JSCZ|Config",
      meta = (DisplayName = "Target Tileset"))
  ACesium3DTileset* TargetTileset;

  /**
   * PropertyTable 中用作 GUID 的属性名称。
   * 根据数据源调整：
   *   CityGML: "gml_id" 或 "id"
   *   Revit:   "guid" 或 "GUID"
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "JSCZ|Config",
      meta = (DisplayName = "GUID Property Name"))
  FString GuidPropertyName = TEXT("guid");

  /**
   * 使用第几个 FeatureIdSet（默认第0个）。
   * 对应 CesiumFeaturesMetadataComponent.Description
   *   .PrimitiveFeatures.FeatureIdSets[N]
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "JSCZ|Config",
      meta = (DisplayName = "Feature ID Set Index"))
  int32 FeatureIdSetIndex = 0;

  // ========== 蓝图可调用的演示方法 ==========

  /**
   * 演示1：高亮指定 GUID（红色）
   * GUID 值来源于 PropertyTable 中 GuidPropertyName 属性的值。
   */
  UFUNCTION(
      BlueprintCallable,
      Category = "JSCZ|Demo",
      meta = (DisplayName = "Demo - Highlight GUIDs"))
  void Demo_HighlightGuids(const TArray<FString>& Guids);

  /** 演示2：清除所有高亮 */
  UFUNCTION(
      BlueprintCallable,
      Category = "JSCZ|Demo",
      meta = (DisplayName = "Demo - Clear Highlight"))
  void Demo_ClearHighlight();

  /**
   * 演示3：分类着色
   * @param ResidentialGuids  住宅 GUID 列表
   * @param CommercialGuids   商业 GUID 列表
   * @param IndustrialGuids   工业 GUID 列表
   */
  UFUNCTION(
      BlueprintCallable,
      Category = "JSCZ|Demo",
      meta = (DisplayName = "Demo - Classify Buildings"))
  void Demo_ClassifyBuildings(
      const TArray<FString>& ResidentialGuids,
      const TArray<FString>& CommercialGuids,
      const TArray<FString>& IndustrialGuids);

  /**
   * 演示4：数值热力图（绿→黄→红）
   * @param GuidValues  GUID → 数值（0.0~1.0）映射
   */
  UFUNCTION(
      BlueprintCallable,
      Category = "JSCZ|Demo",
      meta = (DisplayName = "Demo - Heatmap"))
  void Demo_HeatmapVisualization(
      const TMap<FString, float>& GuidValues);

protected:
  virtual void BeginPlay() override;

private:
  UPROPERTY()
  UCesiumGuidColorManager* ColorManager;

  /** 初始化：确保 CesiumFeaturesMetadataComponent 存在 + 创建 GuidColorManager */
  void SetupColorManager();
};

#pragma endregion
