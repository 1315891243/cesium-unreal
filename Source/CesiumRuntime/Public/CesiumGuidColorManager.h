// Copyright 2020-2025 CesiumGS, Inc. and Contributors
// JSCZ - GUID颜色管理器：瓦片动态加载时自动根据GUID着色

#pragma once

#pragma region JSCZ

#include "Cesium3DTilesetLifecycleEventReceiver.h"
#include "Components/ActorComponent.h"

#include "CesiumGuidColorManager.generated.h"

class ACesium3DTileset;
class UTexture2D;
class UMaterialInstanceDynamic;
class UCesiumMaterialUserData;

namespace CesiumGltf {
struct Material;
}

/**
 * GUID颜色管理组件 (CesiumGuidColorManager)
 *
 * 将此组件添加到 ACesium3DTileset Actor 上，可实现基于GUID的自动着色。
 * 当3DTiles瓦片动态加载时（UCesiumGltfComponent），组件会自动：
 * 1. 读取每个要素（Feature）的GUID属性值
 * 2. 根据预设的 GUID → 颜色 映射表查找颜色
 * 3. 生成颜色查找纹理并设置到材质参数中
 *
 * 用法示例：
 * - 将此组件添加到 Cesium3DTileset Actor
 * - 设置 GuidPropertyName 为元数据中 GUID 属性的名称
 * - 调用 SetGuidColors() 设定 GUID 数组和对应颜色
 * - 新加载的瓦片会自动显示正确颜色
 * - 调用 RefreshAllColors() 可刷新已加载瓦片的颜色
 *
 * 注意：使用的材质中需包含名为 ColorTexParameterName 的纹理参数，
 * 以及 ColorTexSizeParameterName 的向量参数（存储纹理尺寸，用于计算UV）。
 */
UCLASS(
    ClassGroup = "Cesium",
    BlueprintType,
    Blueprintable,
    meta =
        (BlueprintSpawnableComponent,
         DisplayName = "Cesium GUID Color Manager"))
class CESIUMRUNTIME_API UCesiumGuidColorManager
    : public UActorComponent,
      public ICesium3DTilesetLifecycleEventReceiver {
  GENERATED_BODY()

public:
  UCesiumGuidColorManager();

  /**
   * 元数据中 GUID 属性的名称。
   * 例如："guid"、"GUID"、"BatchId"、"id" 等。
   * 组件会在 PropertyTable 中查找此属性，读取每个要素的 GUID 值。
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "JSCZ|GUID Color",
      meta = (DisplayName = "GUID Property Name"))
  FString GuidPropertyName = TEXT("guid");

  /**
   * 材质中用于接收颜色查找纹理的纹理参数名称。
   * 材质 Shader 中应通过此参数获取颜色纹理。
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "JSCZ|GUID Color",
      meta = (DisplayName = "Color Texture Parameter Name"))
  FName ColorTexParameterName = FName(TEXT("CesiumGuidColorTex"));

  /**
   * 材质中用于接收颜色纹理尺寸的向量参数名称。
   * X = 纹理宽度，Y = 纹理高度。
   * 材质 Shader 可通过此参数计算 UV 坐标。
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "JSCZ|GUID Color",
      meta = (DisplayName = "Color Texture Size Parameter Name"))
  FName ColorTexSizeParameterName = FName(TEXT("CesiumGuidColorTexSize"));

  /**
   * 未匹配到 GUID 的要素使用的默认颜色。
   * Alpha = 0 表示完全透明（不着色）。
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "JSCZ|GUID Color",
      meta = (DisplayName = "Default Color"))
  FLinearColor DefaultColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.0f);

  /**
   * 是否在 BeginPlay 时自动注册到父级 Tileset 的生命周期事件。
   * 如果为 false，需要手动调用 RegisterWithTileset()。
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "JSCZ|GUID Color",
      meta = (DisplayName = "Auto Register"))
  bool bAutoRegister = true;

  /**
   * 为一组 GUID 设置颜色。
   * 同一个 GUID 重复设置时，后设置的颜色会覆盖之前的。
   *
   * @param Guids  GUID 字符串数组
   * @param Color  要应用的颜色
   */
  UFUNCTION(
      BlueprintCallable,
      Category = "JSCZ|GUID Color",
      meta = (DisplayName = "Set GUID Colors"))
  void SetGuidColors(const TArray<FString>& Guids, FLinearColor Color);

  /**
   * 移除一组 GUID 的颜色设置。
   * 移除后，这些 GUID 对应的要素将恢复为默认颜色。
   *
   * @param Guids  要移除颜色的 GUID 字符串数组
   */
  UFUNCTION(
      BlueprintCallable,
      Category = "JSCZ|GUID Color",
      meta = (DisplayName = "Remove GUID Colors"))
  void RemoveGuidColors(const TArray<FString>& Guids);

  /**
   * 清除所有 GUID 颜色映射。
   */
  UFUNCTION(
      BlueprintCallable,
      Category = "JSCZ|GUID Color",
      meta = (DisplayName = "Clear All GUID Colors"))
  void ClearAllGuidColors();

  /**
   * 刷新所有已加载瓦片的颜色。
   * 修改颜色映射后，调用此方法可立即更新已加载瓦片的显示。
   * 注意：新加载的瓦片会自动使用最新的颜色映射，无需手动刷新。
   */
  UFUNCTION(
      BlueprintCallable,
      Category = "JSCZ|GUID Color",
      meta = (DisplayName = "Refresh All Colors"))
  void RefreshAllColors();

  /**
   * 手动注册到指定的 Tileset。
   * 通常在 bAutoRegister = false 时使用。
   *
   * @param Tileset  目标 Cesium3DTileset Actor
   */
  UFUNCTION(
      BlueprintCallable,
      Category = "JSCZ|GUID Color",
      meta = (DisplayName = "Register With Tileset"))
  void RegisterWithTileset(ACesium3DTileset* Tileset);

protected:
  virtual void BeginPlay() override;
  virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

  // ---- ICesium3DTilesetLifecycleEventReceiver 接口实现 ----

  /** 材质自定义回调：在每个图元的材质创建后调用，用于设置颜色纹理参数 */
  virtual void CustomizeMaterial(
      ICesiumLoadedTilePrimitive& TilePrimitive,
      UMaterialInstanceDynamic& Material,
      const UCesiumMaterialUserData* CesiumData,
      const CesiumGltf::Material& GltfMaterial) override;

  /** 瓦片卸载回调：清理已失效的缓存数据 */
  virtual void OnTileUnloading(ICesiumLoadedTile& Tile) override;

private:
  /** GUID 到颜色的映射表 */
  TMap<FString, FLinearColor> GuidColorMap;

  /** 缓存的图元颜色信息，用于刷新时更新已加载瓦片 */
  struct FPrimitiveColorInfo {
    /** 图元材质的弱引用 */
    TWeakObjectPtr<UMaterialInstanceDynamic> Material;

    /** 颜色查找纹理 */
    UTexture2D* ColorTexture;

    /** 每个要素ID对应的GUID（索引 = 要素ID） */
    TArray<FString> FeatureGuids;
  };

  /** 已缓存的图元列表 */
  TArray<FPrimitiveColorInfo> CachedPrimitives;

  /** 保持纹理引用，防止被GC回收 */
  UPROPERTY()
  TArray<TObjectPtr<UTexture2D>> ManagedTextures;

  /**
   * 从图元的要素数据中读取 GUID 列表。
   * 遍历要素ID集合(FeatureIdSet)，查找关联的属性表(PropertyTable)中的GUID属性，
   * 读取每个要素的GUID值。
   *
   * @return 按要素ID索引的GUID数组，无GUID的要素对应空字符串
   */
  TArray<FString>
  ReadFeatureGuids(ICesiumLoadedTilePrimitive& TilePrimitive) const;

  /**
   * 根据 GUID 列表和当前颜色映射，构建颜色数组。
   * 在 GuidColorMap 中查找每个 GUID 的颜色，未找到则使用 DefaultColor。
   *
   * @return 按要素ID索引的颜色数组
   */
  TArray<FLinearColor>
  BuildColorArray(const TArray<FString>& FeatureGuids) const;

  /**
   * 创建颜色查找纹理。
   * 纹理宽度最大4096像素，超出部分自动换行到下一行。
   * 每个像素对应一个要素ID的颜色。
   *
   * @param FeatureCount 要素数量
   * @param Colors 颜色数组
   * @return 新创建的纹理，失败返回 nullptr
   */
  UTexture2D*
  CreateColorTexture(int64 FeatureCount, const TArray<FLinearColor>& Colors);

  /**
   * 更新已有纹理的颜色数据（原地更新，不重新创建）。
   *
   * @param Texture 要更新的纹理
   * @param Colors 新的颜色数组
   */
  void UpdateColorTexture(
      UTexture2D* Texture,
      const TArray<FLinearColor>& Colors);

  /**
   * 清理无效的缓存条目（材质已被销毁的图元）。
   * 同时释放不再引用的纹理。
   */
  void CleanupStaleCacheEntries();
};

#pragma endregion
