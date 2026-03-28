// Copyright 2020-2024 CesiumGS, Inc. and Contributors

#pragma once

// ============================================================
// JSCZ: 本文件为 UCesiumGuidColorManager 的示例用法演示
// ACesiumGuidColorExample 展示了4种常见使用场景
// ============================================================

#pragma region JSCZ

#include "CesiumGuidColorManager.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CesiumGuidColorExample.generated.h"

// ============================================================
// JSCZ: ACesiumGuidColorExample
//
// 示例 Actor，演示如何使用 UCesiumGuidColorManager 对
// Cesium 3D Tiles 动态加载的建筑模型进行 GUID 批量高亮。
//
// 包含以下4种使用场景：
//   1. HighlightBuildings      - 按 GUID 批量高亮建筑
//   2. ClearHighlights         - 清除所有高亮
//   3. ClassifyBuildingsByType - 按类型分三色分类高亮
//   4. SendClassifyCommand     - 通过命令字符串分类（模拟外部推送）
//
// 使用方法：
//   1. 将此 Actor 拖入场景
//   2. 在场景中放置 ACesium3DTileset，或让此 Actor 自动发现
//   3. 在编辑器或 Blueprint 中调用上述函数
//   4. 确保 Tileset 的材质中包含 "JsczHighlightColor" Vector 参数
//      （或勾选 bUseCustomDepthStencil 改用 Stencil 方式）
// ============================================================
UCLASS(
    Blueprintable,
    BlueprintType,
    DisplayName = "Cesium GUID Color Example [JSCZ]")
class CESIUMRUNTIME_API ACesiumGuidColorExample : public AActor {
  GENERATED_BODY()

public:
  ACesiumGuidColorExample();

  // ============================================================
  // JSCZ: 子组件
  // ============================================================

  /** GUID颜色管理器组件（核心功能组件） */
  UPROPERTY(
      VisibleAnywhere,
      BlueprintReadOnly,
      Category = "Cesium|JSCZ",
      DisplayName = "GUID颜色管理器")
  TObjectPtr<UCesiumGuidColorManager> GuidColorManager;

  // ============================================================
  // JSCZ: 示例配置参数
  // ============================================================

  /**
   * 需要高亮显示的建筑 GUID 列表（红色示例组）。
   * 可在编辑器中直接填写，也可在运行时通过 Blueprint 赋值。
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "Cesium|JSCZ",
      DisplayName = "红色GUID列表")
  TArray<FString> RedGuids;

  /**
   * 需要高亮显示的建筑 GUID 列表（黄色示例组）。
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "Cesium|JSCZ",
      DisplayName = "黄色GUID列表")
  TArray<FString> YellowGuids;

  /**
   * 需要高亮显示的建筑 GUID 列表（绿色示例组）。
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "Cesium|JSCZ",
      DisplayName = "绿色GUID列表")
  TArray<FString> GreenGuids;

  // ============================================================
  // JSCZ: 示例功能函数
  // ============================================================

  /**
   * 场景1：批量高亮建筑
   * 将 RedGuids / YellowGuids / GreenGuids 中的建筑分别高亮为对应颜色。
   * 动态加载的模型在加载完成后会自动应用颜色，无需手动处理。
   */
  UFUNCTION(
      BlueprintCallable,
      CallInEditor,
      Category = "Cesium|JSCZ",
      DisplayName = "1. 批量高亮建筑")
  void HighlightBuildings();

  /**
   * 场景2：清除所有高亮
   * 重置所有分类，恢复全部建筑为默认无高亮状态。
   */
  UFUNCTION(
      BlueprintCallable,
      CallInEditor,
      Category = "Cesium|JSCZ",
      DisplayName = "2. 清除所有高亮")
  void ClearHighlights();

  /**
   * 场景3：按类型分三色分类
   * 演示如何将预设的三组 GUID 分别设为红/黄/绿，
   * 模拟按建筑类型（如：危房/待修/正常）进行颜色分类显示。
   */
  UFUNCTION(
      BlueprintCallable,
      CallInEditor,
      Category = "Cesium|JSCZ",
      DisplayName = "3. 按类型三色分类")
  void ClassifyBuildingsByType();

  /**
   * 场景4：通过命令字符串分类
   * 演示 ApplyColorCommand 的用法，模拟外部系统推送颜色分类命令。
   * 命令格式：  "Red:guid1,guid2" / "Yellow:guid3" / "Clear:All" 等
   */
  UFUNCTION(
      BlueprintCallable,
      CallInEditor,
      Category = "Cesium|JSCZ",
      DisplayName = "4. 通过命令字符串分类")
  void SendClassifyCommand();

protected:
  virtual void BeginPlay() override;
};

#pragma endregion
