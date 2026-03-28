// Copyright 2020-2025 CesiumGS, Inc. and Contributors
// JSCZ - GUID颜色管理器：通过 CesiumFeaturesMetadataComponent 获取
//        FeatureIdSets 和 PropertyTables，以属性值作为GUID进行着色

#pragma once

#pragma region JSCZ

#include "Cesium3DTilesetLifecycleEventReceiver.h"
#include "Components/ActorComponent.h"

#include "CesiumGuidColorManager.generated.h"

class ACesium3DTileset;
class UTexture2D;
class UMaterialInstanceDynamic;
class UCesiumMaterialUserData;
class UCesiumFeaturesMetadataComponent;

namespace CesiumGltf {
struct Material;
}

/**
 * GUID颜色管理组件 (CesiumGuidColorManager)
 *
 * 将此组件添加到 ACesium3DTileset Actor 上，可实现基于GUID的自动着色。
 *
 * ========== 核心数据流（正确架构） ==========
 *
 * 前提：Tileset 上必须存在 UCesiumFeaturesMetadataComponent，
 * 并在其 Description 中配置好 FeatureIdSets 和 PropertyTables。
 * 这样 Cesium 引擎才会将 FeatureID 和元数据编码到 GPU 材质参数中。
 *
 * 数据流：
 *   CesiumFeaturesMetadataComponent.Description
 *     → PrimitiveFeatures.FeatureIdSets[FeatureIdSetIndex]
 *       → FeatureIdSet 关联一个 PropertyTable（通过 PropertyTableIndex）
 *         → PropertyTable 中的 GuidPropertyName 属性
 *           → 每个要素的属性值 = GUID 标识符
 *             → 查 GuidColorMap 得到颜色
 *               → 生成颜色纹理 → 设到材质参数
 *
 * 材质 Shader 端：
 *   顶点属性 _FEATURE_ID_N → 获取当前面的要素ID
 *   → 以要素ID为索引，从颜色纹理采样
 *   → 得到面的颜色
 *
 * ========== 使用步骤 ==========
 *
 * 1. 在 Tileset 上添加 UCesiumFeaturesMetadataComponent，
 *    配置 Description 中的 FeatureIdSets 和 PropertyTables
 * 2. 在 Tileset 上添加本组件
 * 3. 设置 FeatureIdSetIndex = 要使用的 FeatureIdSet 索引（默认 0）
 * 4. 设置 GuidPropertyName = PropertyTable 中作为 GUID 的属性名
 * 5. 调用 SetGuidColors() 设定 GUID → 颜色 映射
 * 6. 新瓦片自动着色；已加载瓦片调用 RefreshAllColors() 刷新
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
   * 要使用的 FeatureIdSet 索引（在 PrimitiveFeatures 中的序号）。
   *
   * 此索引对应 CesiumFeaturesMetadataComponent.Description
   * .PrimitiveFeatures.FeatureIdSets 数组中的位置。
   * 通常第一个 FeatureIdSet（索引0）即为主要的要素ID集合。
   *
   * 该 FeatureIdSet 通过 PropertyTableIndex 关联到一个 PropertyTable，
   * 组件从该表中读取 GuidPropertyName 属性的值作为 GUID。
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "JSCZ|GUID Color",
      meta = (DisplayName = "Feature ID Set Index"))
  int32 FeatureIdSetIndex = 0;

  /**
   * PropertyTable 中用作 GUID 标识的属性名称。
   *
   * 组件通过 FeatureIdSet → PropertyTable → 此属性名
   * 读取每个要素的 GUID 字符串值。
   *
   * 示例：
   *   CityGML 数据："gml_id" 或 "id"
   *   Revit 数据：  "guid" 或 "GUID"
   *   自定义数据：  查看 PropertyTable 中的实际属性名
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
   * @param Guids  GUID 字符串数组（值来源于 PropertyTable 中的属性值）
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
   * 注册时会自动检查 Tileset 上是否已有 UCesiumFeaturesMetadataComponent。
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
   *
   * 正确的数据获取流程：
   * 1. 从图元获取 FeatureIdSets（由 CesiumFeaturesMetadataComponent 配置编码）
   * 2. 使用 FeatureIdSetIndex 选择指定的 FeatureIdSet
   * 3. 通过 FeatureIdSet.PropertyTableIndex 定位关联的 PropertyTable
   * 4. 从 PropertyTable 中读取 GuidPropertyName 属性的值
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
   * @param FeatureCount 要素数量（来自 FeatureIdSet.GetFeatureCount）
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
