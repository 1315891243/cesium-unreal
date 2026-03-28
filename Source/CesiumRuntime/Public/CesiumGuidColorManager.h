// Copyright 2020-2024 CesiumGS, Inc. and Contributors

#pragma once

// ============================================================
// JSCZ: 本文件由JSCZ定制开发，用于3D Tiles GUID批量高亮着色
// 请勿删除 #pragma region JSCZ / #pragma endregion 标记
// ============================================================

#pragma region JSCZ

#include "Cesium3DTileset.h"
#include "Cesium3DTilesetLifecycleEventReceiver.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "CesiumGuidColorManager.generated.h"

// ============================================================
// JSCZ: GUID颜色分类枚举
// None=无高亮, Red=红色, Yellow=黄色, Green=绿色
// 可通过命令或Blueprint批量设置，用于城市模型楼宇等要素高亮
// ============================================================
UENUM(BlueprintType)
enum class ECesiumGuidColorCategory : uint8 {
  /** 无高亮（默认状态） */
  None UMETA(DisplayName = "无"),
  /** 红色高亮 */
  Red UMETA(DisplayName = "红色"),
  /** 黄色高亮 */
  Yellow UMETA(DisplayName = "黄色"),
  /** 绿色高亮 */
  Green UMETA(DisplayName = "绿色"),
};

// ============================================================
// JSCZ: 已加载图元的记录结构体
// 存储每个已加载的UCesiumGltfPrimitiveComponent的材质实例与其
// 包含的所有GUID列表，供分类变更时实时刷新高亮颜色使用
// ============================================================
USTRUCT()
struct CESIUMRUNTIME_API FJsczLoadedPrimitive {
  GENERATED_BODY()

  /** 材质实例弱引用（Tile卸载后自动失效，无需手动清理） */
  TWeakObjectPtr<UMaterialInstanceDynamic> Material;

  /** 图元所属的渲染组件弱引用（用于CustomDepth Stencil方式） */
  TWeakObjectPtr<UPrimitiveComponent> Primitive;

  /** 该图元属性表中读取到的所有GUID列表 */
  TArray<FString> Guids;
};

// ============================================================
// JSCZ: UCesiumGuidColorManager
//
// 功能说明：
//   本组件通过实现 ICesium3DTilesetLifecycleEventReceiver 接口，
//   在 ACesium3DTileset 动态加载/卸载 3D Tiles 时自动关联，
//   从每个加载图元的 FeatureIdSets / PropertyTables 中解析出
//   实际关联的 GUID，并根据预设的分类（红/黄/绿）对对应模型
//   进行高亮渲染。
//
// 支持规模：
//   可处理最多10万条 GUID，使用 TMap 实现 O(1) 查找，不卡顿。
//
// 使用方式：
//   1. 在场景中任意 Actor 上添加此组件
//   2. 设置 GuidPropertyName 为你的 glTF metadata 属性名（如 "guid"）
//   3. 在你的基础材质中添加名为 "JsczHighlightColor" 的 Vector 参数
//      （或启用 bUseCustomDepthStencil，则无需修改材质）
//   4. 调用 SetGuidCategory() 批量设置 GUID 分类
//   5. 或发送命令字符串 ApplyColorCommand("Red:guid1,guid2,...")
//
// 命令格式（ApplyColorCommand / 控制台命令）：
//   "Red:guid1,guid2,guid3"   - 将GUID分类为红色
//   "Yellow:guid1,guid2"      - 将GUID分类为黄色
//   "Green:guid1"             - 将GUID分类为绿色
//   "None:guid1,guid2"        - 清除GUID高亮
//   "Clear:Red"               - 清除所有红色分类
//   "Clear:All"               - 清除全部分类
// ============================================================
UCLASS(
    ClassGroup = (Cesium),
    meta = (BlueprintSpawnableComponent),
    DisplayName = "Cesium GUID Color Manager [JSCZ]")
class CESIUMRUNTIME_API UCesiumGuidColorManager
    : public UActorComponent,
      public ICesium3DTilesetLifecycleEventReceiver {
  GENERATED_BODY()

public:
  UCesiumGuidColorManager();

  // ============================================================
  // JSCZ: 配置属性
  // ============================================================

  /**
   * PropertyTable 中存储 GUID 的属性名称。
   * 例如 "guid"、"id"、"buildingId" 等，需与数据实际字段名一致。
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "Cesium|JSCZ",
      DisplayName = "GUID属性名称")
  FString GuidPropertyName = TEXT("guid");

  /**
   * 指定要搜索的属性表名称（可选）。
   * 留空则搜索所有属性表，找到第一个包含 GuidPropertyName 的表为止。
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "Cesium|JSCZ",
      DisplayName = "属性表名称（空=搜索所有）")
  FString PropertyTableName = TEXT("");

  /**
   * 材质中高亮颜色的 Vector 参数名称。
   * 需要在基础材质中添加同名 Vector 参数并连接到自发光或叠加层。
   * 仅在 bUseCustomDepthStencil=false 时使用。
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "Cesium|JSCZ",
      DisplayName = "材质颜色参数名")
  FName HighlightColorParamName = TEXT("JsczHighlightColor");

  /**
   * 是否使用 CustomDepth + Stencil 方式实现高亮（无需修改材质）。
   * 需要在后处理材质中处理对应的 Stencil 值：
   *   Red=RedStencilValue, Yellow=RedStencilValue+1, Green=RedStencilValue+2
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "Cesium|JSCZ",
      DisplayName = "使用CustomDepth Stencil方式")
  bool bUseCustomDepthStencil = false;

  /**
   * CustomDepth Stencil 方式中红色分类对应的起始 Stencil 值。
   * 黄色=此值+1，绿色=此值+2，None=0（关闭CustomDepth）。
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "Cesium|JSCZ",
      meta = (EditCondition = "bUseCustomDepthStencil"),
      DisplayName = "红色Stencil起始值")
  int32 RedStencilValue = 1;

  /** 红色分类的高亮颜色（材质参数方式） */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "Cesium|JSCZ",
      DisplayName = "红色")
  FLinearColor RedColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);

  /** 黄色分类的高亮颜色（材质参数方式） */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "Cesium|JSCZ",
      DisplayName = "黄色")
  FLinearColor YellowColor = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);

  /** 绿色分类的高亮颜色（材质参数方式） */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "Cesium|JSCZ",
      DisplayName = "绿色")
  FLinearColor GreenColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);

  /**
   * 无高亮时的颜色（材质参数方式）。
   * 通常设为 (0,0,0,0) 使材质忽略该参数影响。
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "Cesium|JSCZ",
      DisplayName = "无高亮颜色")
  FLinearColor NoneColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

  /**
   * 要自动注册的目标 Tileset 列表。
   * 留空则在 BeginPlay 时自动搜索场景中所有 ACesium3DTileset。
   */
  UPROPERTY(
      EditAnywhere,
      BlueprintReadWrite,
      Category = "Cesium|JSCZ",
      DisplayName = "目标Tileset列表（空=自动搜索全场景）")
  TArray<TObjectPtr<ACesium3DTileset>> TargetTilesets;

  // ============================================================
  // JSCZ: 批量GUID分类接口（核心API）
  // ============================================================

  /**
   * 批量设置一组 GUID 的颜色分类。
   * 同时实时更新所有已加载的对应模型的高亮颜色。
   * 支持一次性传入10万条 GUID，使用 TMap 批量写入。
   *
   * @param Guids   要分类的 GUID 字符串数组
   * @param Category 目标颜色分类
   */
  UFUNCTION(BlueprintCallable, Category = "Cesium|JSCZ")
  void SetGuidCategory(
      const TArray<FString>& Guids,
      ECesiumGuidColorCategory Category);

  /**
   * 清除全部 GUID 颜色分类，恢复所有已加载模型为无高亮状态。
   */
  UFUNCTION(BlueprintCallable, Category = "Cesium|JSCZ")
  void ClearAllCategories();

  /**
   * 清除指定颜色分类下的所有 GUID。
   *
   * @param Category 要清除的颜色分类
   */
  UFUNCTION(BlueprintCallable, Category = "Cesium|JSCZ")
  void ClearCategory(ECesiumGuidColorCategory Category);

  /**
   * 查询某个 GUID 的当前颜色分类。
   *
   * @param Guid 要查询的 GUID 字符串
   * @return 该 GUID 的颜色分类，不存在则返回 None
   */
  UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cesium|JSCZ")
  ECesiumGuidColorCategory GetGuidCategory(const FString& Guid) const;

  /**
   * 获取某颜色分类下的所有 GUID 列表。
   *
   * @param Category 要查询的颜色分类
   * @return 该分类下的 GUID 字符串数组
   */
  UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cesium|JSCZ")
  TArray<FString> GetGuidsInCategory(ECesiumGuidColorCategory Category) const;

  /**
   * 获取当前已注册的 GUID 总数（含所有分类）。
   */
  UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cesium|JSCZ")
  int32 GetTotalGuidCount() const;

  // ============================================================
  // JSCZ: 命令接口
  // 支持通过字符串命令批量分类，便于外部系统/服务器推送
  // ============================================================

  /**
   * 解析并执行颜色命令字符串。
   *
   * 命令格式：
   *   "Red:guid1,guid2,guid3"   将 GUID 设为红色
   *   "Yellow:guid1,guid2"      将 GUID 设为黄色
   *   "Green:guid1"             将 GUID 设为绿色
   *   "None:guid1,guid2"        清除 GUID 高亮
   *   "Clear:Red"               清除所有红色分类
   *   "Clear:Yellow"            清除所有黄色分类
   *   "Clear:Green"             清除所有绿色分类
   *   "Clear:All"               清除全部分类
   *
   * @param Command 命令字符串
   */
  UFUNCTION(BlueprintCallable, Category = "Cesium|JSCZ")
  void ApplyColorCommand(const FString& Command);

  // ============================================================
  // JSCZ: ICesium3DTilesetLifecycleEventReceiver 接口实现
  // 以下方法在 Tile 加载/卸载时被 ACesium3DTileset 自动调用
  // ============================================================

  /**
   * Tile图元材质定制回调。
   * 从图元的 FeatureIdSets / PropertyTables 中提取 GUID，
   * 查找颜色分类并设置材质高亮颜色参数。
   * 同时将该图元注册到 LoadedPrimitives 列表供后续更新使用。
   */
  virtual void CustomizeMaterial(
      ICesiumLoadedTilePrimitive& TilePrimitive,
      UMaterialInstanceDynamic& Material,
      const UCesiumMaterialUserData* CesiumData,
      const CesiumGltf::Material& GltfMaterial) override;

  /**
   * Tile卸载回调。
   * 触发无效图元引用的清理（使用弱指针，此处仅做主动剪枝优化）。
   */
  virtual void OnTileUnloading(ICesiumLoadedTile& Tile) override;

protected:
  /** 游戏开始时自动注册到目标 Tileset */
  virtual void BeginPlay() override;

  /** 游戏结束时从 Tileset 注销 */
  virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
  // ============================================================
  // JSCZ: 内部数据
  // ============================================================

  /**
   * 主 GUID 颜色分类映射表。
   * 键=GUID字符串, 值=颜色分类枚举。
   * TMap 哈希表实现 O(1) 查找，支持10万条记录高效运行。
   */
  TMap<FString, ECesiumGuidColorCategory> GuidColorMap;

  /**
   * 已加载图元列表。
   * 记录当前场景中所有已加载的图元材质及其 GUID，
   * 当分类变更时遍历此列表实时刷新材质颜色。
   * 弱指针在 Tile 卸载后自动失效，定期调用 PruneInvalidPrimitives 清理。
   */
  TArray<FJsczLoadedPrimitive> LoadedPrimitives;

  // ============================================================
  // JSCZ: 内部辅助方法
  // ============================================================

  /** 根据分类枚举返回对应的 FLinearColor */
  FLinearColor GetColorForCategory(ECesiumGuidColorCategory Category) const;

  /**
   * 根据一组 GUID 计算最终应用到图元的颜色分类。
   * 优先级：Red > Yellow > Green > None。
   *
   * @param Guids 图元包含的 GUID 列表
   * @return 应用的最终颜色分类
   */
  ECesiumGuidColorCategory
  ComputeEffectiveCategory(const TArray<FString>& Guids) const;

  /**
   * 将颜色分类应用到材质实例和/或图元组件。
   * 根据 bUseCustomDepthStencil 选择材质参数或 Stencil 方式。
   *
   * @param pMaterial  目标材质实例（材质参数方式需有效）
   * @param pPrimitive 目标图元组件（Stencil方式需有效）
   * @param Category   颜色分类
   */
  void ApplyColorToComponents(
      UMaterialInstanceDynamic* pMaterial,
      UPrimitiveComponent* pPrimitive,
      ECesiumGuidColorCategory Category);

  /**
   * 遍历 LoadedPrimitives 重新计算并应用所有图元的颜色。
   * 在 SetGuidCategory / ClearAllCategories 等分类变更后调用。
   */
  void RefreshAllLoadedMaterials();

  /**
   * 清理 LoadedPrimitives 中材质/图元引用已失效（Tile已卸载）的条目，
   * 释放内存，防止列表无限增长。
   */
  void PruneInvalidPrimitives();

  /**
   * 将当前已注册为 LifecycleEventReceiver 的 Tileset 集合。
   * 用于在 EndPlay 时恢复（置回 nullptr）。
   */
  TArray<TWeakObjectPtr<ACesium3DTileset>> RegisteredTilesets;
};

#pragma endregion
