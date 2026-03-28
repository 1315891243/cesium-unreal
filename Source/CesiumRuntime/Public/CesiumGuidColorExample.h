// Copyright 2020-2025 CesiumGS, Inc. and Contributors
// JSCZ - CesiumGuidColorManager 使用示例

#pragma once

#pragma region JSCZ

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CesiumGuidColorExample.generated.h"

class ACesium3DTileset;
class UCesiumGuidColorManager;

/**
 * CesiumGuidColorExample — GUID颜色管理器使用示例
 *
 * 这是一个演示 Actor，展示如何在 C++ 中使用 UCesiumGuidColorManager
 * 来实现 3DTiles 瓦片的基于 GUID 着色功能。
 *
 * ========== 使用场景 ==========
 *
 * 场景1：建筑模型高亮
 *   - 3DTiles 模型中每栋建筑有唯一 GUID
 *   - 用户点击某栋建筑时，将其 GUID 设为红色高亮
 *
 * 场景2：批量分类着色
 *   - 按建筑用途分类（住宅=绿色，商业=蓝色，工业=黄色）
 *   - 一次性设置所有 GUID 的颜色
 *
 * 场景3：动态数据可视化
 *   - 根据实时数据（如温度、能耗）动态改变建筑颜色
 *   - 每帧或定时刷新颜色
 *
 * ========== 放置方法 ==========
 *
 * 方法A（编辑器操作）：
 *   1. 将此 Actor 拖入关卡
 *   2. 在细节面板中设置 TargetTileset 引用
 *   3. 运行游戏
 *
 * 方法B（代码动态创建）：
 *   见 SpawnExampleActor() 静态方法
 *
 * ========== 前提条件 ==========
 *
 * 1. 3DTiles 数据中的要素需包含 GUID 属性字段（如 "guid", "id" 等）
 * 2. 使用的材质中需包含以下参数（可在 UCesiumGuidColorManager 中配置名称）：
 *    - 纹理参数：CesiumGuidColorTex（颜色查找纹理）
 *    - 向量参数：CesiumGuidColorTexSize（纹理尺寸，X=宽, Y=高）
 * 3. 材质 Shader 中需根据 FeatureID 从纹理中采样颜色
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "GUID Color Example"))
class CESIUMRUNTIME_API ACesiumGuidColorExample : public AActor {
  GENERATED_BODY()

public:
  ACesiumGuidColorExample();

  // ========== 可配置属性 ==========

  /**
   * 目标 Tileset Actor 引用。
   * 在编辑器中通过下拉菜单选择关卡中已有的 Cesium3DTileset。
   * 如果不设置，BeginPlay 时会在关卡中自动查找第一个 Tileset。
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "JSCZ|示例配置",
      meta = (DisplayName = "目标 Tileset"))
  ACesium3DTileset* TargetTileset;

  /**
   * 元数据中 GUID 属性的名称。
   * 根据你的 3DTiles 数据源设置：
   *   - CityGML 转换：通常为 "gml_id" 或 "id"
   *   - Revit 转换：  通常为 "guid" 或 "GUID"
   *   - 自定义数据：  查看元数据中的实际属性名
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "JSCZ|示例配置",
      meta = (DisplayName = "GUID 属性名"))
  FString GuidPropertyName = TEXT("guid");

  // ========== 蓝图可调用的演示方法 ==========

  /**
   * 演示1：高亮指定的 GUID 列表（设为红色）
   *
   * 用法示例（蓝图或C++）：
   *   TArray<FString> Guids = { "building-001", "building-002" };
   *   ExampleActor->Demo_HighlightGuids(Guids);
   */
  UFUNCTION(
      BlueprintCallable,
      Category = "JSCZ|演示",
      meta = (DisplayName = "演示-高亮GUID"))
  void Demo_HighlightGuids(const TArray<FString>& Guids);

  /**
   * 演示2：清除所有高亮，恢复默认颜色
   */
  UFUNCTION(
      BlueprintCallable,
      Category = "JSCZ|演示",
      meta = (DisplayName = "演示-清除高亮"))
  void Demo_ClearHighlight();

  /**
   * 演示3：批量分类着色
   * 将不同组的 GUID 设为不同颜色，模拟建筑分类可视化。
   *
   * @param ResidentialGuids   住宅类建筑的 GUID 列表
   * @param CommercialGuids    商业类建筑的 GUID 列表
   * @param IndustrialGuids    工业类建筑的 GUID 列表
   */
  UFUNCTION(
      BlueprintCallable,
      Category = "JSCZ|演示",
      meta = (DisplayName = "演示-分类着色"))
  void Demo_ClassifyBuildings(
      const TArray<FString>& ResidentialGuids,
      const TArray<FString>& CommercialGuids,
      const TArray<FString>& IndustrialGuids);

  /**
   * 演示4：模拟动态数据可视化
   * 根据数值（0.0~1.0）将 GUID 映射为从绿色到红色的渐变色。
   * 可用于温度、能耗、密度等数值数据的可视化。
   *
   * @param GuidValues  GUID 到数值的映射（值域 0.0~1.0）
   */
  UFUNCTION(
      BlueprintCallable,
      Category = "JSCZ|演示",
      meta = (DisplayName = "演示-数值热力图"))
  void Demo_HeatmapVisualization(
      const TMap<FString, float>& GuidValues);

protected:
  virtual void BeginPlay() override;

private:
  /**
   * 颜色管理器组件引用（由 BeginPlay 自动创建并附加到 Tileset）
   */
  UPROPERTY()
  UCesiumGuidColorManager* ColorManager;

  /** 初始化颜色管理器，查找或创建并注册到 Tileset */
  void SetupColorManager();
};

#pragma endregion
