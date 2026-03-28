// Copyright 2020-2024 CesiumGS, Inc. and Contributors

// ============================================================
// JSCZ: 本文件由JSCZ定制开发
// UCesiumGuidColorManager 实现
// 通过 ICesium3DTilesetLifecycleEventReceiver 接口实现
// 3D Tiles 动态加载时自动批量高亮 GUID 对应的建筑模型
// ============================================================

#pragma region JSCZ

#include "CesiumGuidColorManager.h"
#include "Cesium3DTileset.h"
#include "Cesium3DTilesetLifecycleEventReceiver.h"
#include "CesiumFeatureIdSet.h"
#include "CesiumFeaturesMetadataComponent.h"
#include "CesiumLoadedTile.h"
#include "CesiumModelMetadata.h"
#include "CesiumPropertyTable.h"
#include "CesiumPropertyTableProperty.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EncodedFeaturesMetadata.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UObjectGlobals.h"

namespace {

// ============================================================
// JSCZ: 判断某个属性表是否被 CesiumFeaturesMetadataComponent 配置启用
// 若场景中未挂该组件，或其描述数组为空，则默认不过滤，保持兼容
// ============================================================
bool isPropertyTableEnabledForGuid(
    const UCesiumFeaturesMetadataComponent* pFeaturesMetadataComponent,
    const FString& requestedPropertyTableName,
    const FString& tableName,
    const FString& guidPropertyName) {
  if (!requestedPropertyTableName.IsEmpty() &&
      !tableName.Equals(requestedPropertyTableName)) {
    return false;
  }

  if (!IsValid(pFeaturesMetadataComponent)) {
    return true;
  }

  const TArray<FCesiumPropertyTableDescription>& propertyTableDescriptions =
      pFeaturesMetadataComponent->Description.ModelMetadata.PropertyTables;
  if (propertyTableDescriptions.Num() == 0) {
    return true;
  }

  const FCesiumPropertyTableDescription* pDescription =
      propertyTableDescriptions.FindByPredicate(
          [&tableName](const FCesiumPropertyTableDescription& description) {
            return description.Name == tableName;
          });
  if (!pDescription) {
    return false;
  }

  return pDescription->Properties.Num() == 0 ||
         pDescription->Properties.ContainsByPredicate(
             [&guidPropertyName](
                 const FCesiumPropertyTablePropertyDescription& property) {
               return property.Name == guidPropertyName;
             });
}

// ============================================================
// JSCZ: 判断某个 FeatureIdSet 是否被 CesiumFeaturesMetadataComponent 描述选中
// 需要同时匹配自动生成/配置的 FeatureIdSet 名称以及其关联的属性表名
// ============================================================
bool isFeatureIdSetEnabled(
    const UCesiumFeaturesMetadataComponent* pFeaturesMetadataComponent,
    const FString& featureIdSetName,
    const FString& propertyTableName) {
  if (!IsValid(pFeaturesMetadataComponent)) {
    return true;
  }

  const TArray<FCesiumFeatureIdSetDescription>& featureIdSetDescriptions =
      pFeaturesMetadataComponent->Description.PrimitiveFeatures.FeatureIdSets;
  if (featureIdSetDescriptions.Num() == 0) {
    return true;
  }

  return featureIdSetDescriptions.ContainsByPredicate(
      [&featureIdSetName,
       &propertyTableName](const FCesiumFeatureIdSetDescription& description) {
        return description.Name == featureIdSetName &&
               description.PropertyTableName == propertyTableName;
      });
}

// ============================================================
// JSCZ: 收集当前图元实际关联到的 FeatureID
// Attribute / Implicit 通过顶点遍历，Instance 通过实例遍历，
// Texture 类型退化为 featureCount 范围，以避免整张属性表误扫
// ============================================================
void collectFeatureIdsForSet(
    const FCesiumPrimitiveFeatures& primitiveFeatures,
    const FCesiumFeatureIdSet& featureIdSet,
    const UPrimitiveComponent& primitiveComponent,
    TSet<int64>& outFeatureIds) {
  const ECesiumFeatureIdSetType featureIdSetType =
      UCesiumFeatureIdSetBlueprintLibrary::GetFeatureIDSetType(featureIdSet);
  const int64 nullFeatureId =
      UCesiumFeatureIdSetBlueprintLibrary::GetNullFeatureID(featureIdSet);

  if (featureIdSetType == ECesiumFeatureIdSetType::Instance ||
      featureIdSetType == ECesiumFeatureIdSetType::InstanceImplicit) {
    const UInstancedStaticMeshComponent* pInstancedComponent =
        Cast<UInstancedStaticMeshComponent>(&primitiveComponent);
    const int32 instanceCount = IsValid(pInstancedComponent)
                                    ? pInstancedComponent->GetInstanceCount()
                                    : 0;

    for (int32 instanceIndex = 0; instanceIndex < instanceCount;
         ++instanceIndex) {
      const int64 featureId =
          UCesiumFeatureIdSetBlueprintLibrary::GetFeatureIDForInstance(
              featureIdSet,
              instanceIndex);
      if (featureId >= 0 && featureId != nullFeatureId) {
        outFeatureIds.Add(featureId);
      }
    }
    return;
  }

  if (featureIdSetType == ECesiumFeatureIdSetType::Texture) {
    const int64 featureCount =
        UCesiumFeatureIdSetBlueprintLibrary::GetFeatureCount(featureIdSet);
    for (int64 featureId = 0; featureId < featureCount; ++featureId) {
      if (featureId != nullFeatureId) {
        outFeatureIds.Add(featureId);
      }
    }
    return;
  }

  const int64 vertexCount =
      UCesiumPrimitiveFeaturesBlueprintLibrary::GetVertexCount(
          primitiveFeatures);
  for (int64 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
    const int64 featureId =
        UCesiumFeatureIdSetBlueprintLibrary::GetFeatureIDForVertex(
            featureIdSet,
            vertexIndex);
    if (featureId >= 0 && featureId != nullFeatureId) {
      outFeatureIds.Add(featureId);
    }
  }
}

} // namespace

// ============================================================
// JSCZ: 构造函数
// 初始化组件基本参数，默认在游戏开始时激活
// ============================================================
UCesiumGuidColorManager::UCesiumGuidColorManager() {
  // 允许 Tick（若需要异步更新可在此开启，当前不需要 Tick）
  PrimaryComponentTick.bCanEverTick = false;
}

// ============================================================
// JSCZ: BeginPlay
// 在游戏开始时自动搜索并注册到所有目标 Tileset，
// 使本组件成为这些 Tileset 的 LifecycleEventReceiver，
// 从而在 Tile 加载时自动触发颜色回调
// ============================================================
void UCesiumGuidColorManager::BeginPlay() {
  Super::BeginPlay();

  UWorld* pWorld = GetWorld();
  if (!pWorld) {
    return;
  }

  // 判断是否使用手动配置的 Tileset 列表
  if (TargetTilesets.Num() > 0) {
    // 使用手动指定的 Tileset 列表注册
    for (TObjectPtr<ACesium3DTileset>& pTileset : TargetTilesets) {
      if (IsValid(pTileset)) {
        // 将本组件注册为该 Tileset 的生命周期事件接收器
        pTileset->SetLifecycleEventReceiver(this);
        RegisteredTilesets.Add(pTileset);
      }
    }
  } else {
    // 自动搜索场景中所有 ACesium3DTileset 并注册
    // 对每个 Tileset 调用 SetLifecycleEventReceiver 使本组件
    // 能在 Tile 加载/卸载时收到回调
    for (TActorIterator<ACesium3DTileset> It(pWorld); It; ++It) {
      ACesium3DTileset* pTileset = *It;
      if (IsValid(pTileset)) {
        pTileset->SetLifecycleEventReceiver(this);
        RegisteredTilesets.Add(pTileset);
      }
    }
  }
}

// ============================================================
// JSCZ: EndPlay
// 游戏结束时从所有已注册的 Tileset 注销，
// 防止 Tileset 持有悬空指针
// ============================================================
void UCesiumGuidColorManager::EndPlay(
    const EEndPlayReason::Type EndPlayReason) {
  // 逐一注销已注册的 Tileset
  for (TWeakObjectPtr<ACesium3DTileset>& WeakTileset : RegisteredTilesets) {
    if (WeakTileset.IsValid()) {
      // 将 LifecycleEventReceiver 置为 nullptr 以解除关联
      WeakTileset->SetLifecycleEventReceiver(nullptr);
    }
  }
  RegisteredTilesets.Empty();

  // 清理已加载图元列表
  LoadedPrimitives.Empty();

  Super::EndPlay(EndPlayReason);
}

// ============================================================
// JSCZ: SetGuidCategory
// 批量设置 GUID 的颜色分类，并实时更新所有已加载模型的高亮
// 使用 TMap Reserve 预分配内存，保障10万条 GUID 批量写入性能
// ============================================================
void UCesiumGuidColorManager::SetGuidCategory(
    const TArray<FString>& Guids,
    ECesiumGuidColorCategory Category) {
  if (Guids.Num() == 0) {
    return;
  }

  // 预扩容：一次性分配所需空间，避免多次 rehash（对10万条GUID尤为重要）
  GuidColorMap.Reserve(GuidColorMap.Num() + Guids.Num());

  // 批量写入分类映射
  for (const FString& Guid : Guids) {
    if (!Guid.IsEmpty()) {
      GuidColorMap.Add(Guid, Category);
    }
  }

  // 刷新所有已加载图元的材质颜色
  RefreshAllLoadedMaterials();
}

// ============================================================
// JSCZ: ClearAllCategories
// 清除全部 GUID 分类，恢复所有已加载模型为无高亮状态
// ============================================================
void UCesiumGuidColorManager::ClearAllCategories() {
  GuidColorMap.Empty();
  // 刷新所有已加载图元，将颜色重置为 None
  RefreshAllLoadedMaterials();
}

// ============================================================
// JSCZ: ClearCategory
// 清除指定颜色分类下的所有 GUID，并实时刷新对应高亮
// ============================================================
void UCesiumGuidColorManager::ClearCategory(ECesiumGuidColorCategory Category) {
  // 收集需要移除的 GUID 键
  TArray<FString> KeysToRemove;
  KeysToRemove.Reserve(GuidColorMap.Num() / 4); // 预估25%的条目（初始容量估算）

  for (const TPair<FString, ECesiumGuidColorCategory>& Pair : GuidColorMap) {
    if (Pair.Value == Category) {
      KeysToRemove.Add(Pair.Key);
    }
  }

  // 批量移除
  for (const FString& Key : KeysToRemove) {
    GuidColorMap.Remove(Key);
  }

  // 仅在有实际变更时才刷新
  if (KeysToRemove.Num() > 0) {
    RefreshAllLoadedMaterials();
  }
}

// ============================================================
// JSCZ: GetGuidCategory
// 查询某 GUID 的当前颜色分类，不存在则返回 None
// ============================================================
ECesiumGuidColorCategory
UCesiumGuidColorManager::GetGuidCategory(const FString& Guid) const {
  const ECesiumGuidColorCategory* pCategory = GuidColorMap.Find(Guid);
  return pCategory ? *pCategory : ECesiumGuidColorCategory::None;
}

// ============================================================
// JSCZ: GetGuidsInCategory
// 返回指定颜色分类下所有 GUID 的列表
// ============================================================
TArray<FString> UCesiumGuidColorManager::GetGuidsInCategory(
    ECesiumGuidColorCategory Category) const {
  TArray<FString> Result;
  for (const TPair<FString, ECesiumGuidColorCategory>& Pair : GuidColorMap) {
    if (Pair.Value == Category) {
      Result.Add(Pair.Key);
    }
  }
  return Result;
}

// ============================================================
// JSCZ: GetTotalGuidCount
// 返回当前已注册的 GUID 总数（含所有分类）
// ============================================================
int32 UCesiumGuidColorManager::GetTotalGuidCount() const {
  return GuidColorMap.Num();
}

// ============================================================
// JSCZ: ApplyColorCommand
// 解析并执行颜色命令字符串，支持外部系统推送命令
//
// 命令格式：
//   "Red:guid1,guid2,guid3"   - 将 GUID 设为红色
//   "Yellow:guid1,guid2"      - 将 GUID 设为黄色
//   "Green:guid1"             - 将 GUID 设为绿色
//   "None:guid1,guid2"        - 清除 GUID 高亮
//   "Clear:Red"               - 清除所有红色分类
//   "Clear:Yellow"            - 清除所有黄色分类
//   "Clear:Green"             - 清除所有绿色分类
//   "Clear:All"               - 清除全部分类
// ============================================================
void UCesiumGuidColorManager::ApplyColorCommand(const FString& Command) {
  if (Command.IsEmpty()) {
    return;
  }

  // 以冒号分割命令头部与 GUID 列表
  FString CategoryStr, GuidListStr;
  if (!Command.Split(TEXT(":"), &CategoryStr, &GuidListStr)) {
    // 格式错误，忽略
    return;
  }

  CategoryStr.TrimStartAndEndInline();
  GuidListStr.TrimStartAndEndInline();

  // 处理 Clear 命令
  if (CategoryStr.Equals(TEXT("Clear"), ESearchCase::IgnoreCase)) {
    if (GuidListStr.Equals(TEXT("All"), ESearchCase::IgnoreCase)) {
      ClearAllCategories();
    } else if (GuidListStr.Equals(TEXT("Red"), ESearchCase::IgnoreCase)) {
      ClearCategory(ECesiumGuidColorCategory::Red);
    } else if (GuidListStr.Equals(TEXT("Yellow"), ESearchCase::IgnoreCase)) {
      ClearCategory(ECesiumGuidColorCategory::Yellow);
    } else if (GuidListStr.Equals(TEXT("Green"), ESearchCase::IgnoreCase)) {
      ClearCategory(ECesiumGuidColorCategory::Green);
    }
    return;
  }

  // 解析颜色分类
  ECesiumGuidColorCategory Category = ECesiumGuidColorCategory::None;
  if (CategoryStr.Equals(TEXT("Red"), ESearchCase::IgnoreCase)) {
    Category = ECesiumGuidColorCategory::Red;
  } else if (CategoryStr.Equals(TEXT("Yellow"), ESearchCase::IgnoreCase)) {
    Category = ECesiumGuidColorCategory::Yellow;
  } else if (CategoryStr.Equals(TEXT("Green"), ESearchCase::IgnoreCase)) {
    Category = ECesiumGuidColorCategory::Green;
  } else if (CategoryStr.Equals(TEXT("None"), ESearchCase::IgnoreCase)) {
    Category = ECesiumGuidColorCategory::None;
  } else {
    // 未识别的分类名称，忽略
    return;
  }

  // 以逗号分割 GUID 列表
  TArray<FString> Guids;
  GuidListStr.ParseIntoArray(Guids, TEXT(","), true);

  // 去除首尾空白
  for (FString& Guid : Guids) {
    Guid.TrimStartAndEndInline();
  }

  // 执行批量分类设置
  SetGuidCategory(Guids, Category);
}

// ============================================================
// JSCZ: CustomizeMaterial（ICesium3DTilesetLifecycleEventReceiver）
//
// 每个 UCesiumGltfPrimitiveComponent 加载完成并创建材质后调用。
// 流程：
//   1. 从当前图元的 FeatureIdSets 找到它关联的 PropertyTable
//   2. 通过 FeatureID 仅提取当前图元真正关联的 GUID
//   3. 通过 GuidColorMap 查找每个 GUID 的颜色分类（O(1)）
//   4. 取优先级最高的颜色（Red > Yellow > Green > None）
//   5. 将颜色应用到材质实例 Vector 参数 或 CustomDepth Stencil
//   6. 将本图元记录到 LoadedPrimitives，供后续分类变更时实时更新
// ============================================================
void UCesiumGuidColorManager::CustomizeMaterial(
    ICesiumLoadedTilePrimitive& TilePrimitive,
    UMaterialInstanceDynamic& Material,
    const UCesiumMaterialUserData* CesiumData,
    const CesiumGltf::Material& GltfMaterial) {
  ICesiumLoadedTile& LoadedTile = TilePrimitive.GetLoadedTile();
  const FCesiumModelMetadata& ModelMetadata = LoadedTile.GetModelMetadata();
  ACesium3DTileset& TilesetActor = LoadedTile.GetTilesetActor();
  const UCesiumFeaturesMetadataComponent* pFeaturesMetadataComponent =
      TilesetActor.FindComponentByClass<UCesiumFeaturesMetadataComponent>();
  const FCesiumPrimitiveFeatures& PrimitiveFeatures =
      TilePrimitive.GetPrimitiveFeatures();
  const TArray<FCesiumFeatureIdSet>& FeatureIdSets =
      UCesiumPrimitiveFeaturesBlueprintLibrary::GetFeatureIDSets(
          PrimitiveFeatures);

  const TArray<FCesiumPropertyTable>& PropertyTables =
      UCesiumModelMetadataBlueprintLibrary::GetPropertyTables(ModelMetadata);

  // 收集该图元真正关联到的 GUID，避免把整张属性表误绑定到单个图元
  TSet<FString> FoundGuidSet;
  int32 FeatureIdTextureCounter = 0;

  for (const FCesiumFeatureIdSet& FeatureIdSet : FeatureIdSets) {
    const int64 PropertyTableIndex =
        UCesiumFeatureIdSetBlueprintLibrary::GetPropertyTableIndex(
            FeatureIdSet);
    if (PropertyTableIndex < 0 || PropertyTableIndex >= PropertyTables.Num()) {
      EncodedFeaturesMetadata::getNameForFeatureIDSet(
          FeatureIdSet,
          FeatureIdTextureCounter);
      continue;
    }

    const FString FeatureIdSetName =
        EncodedFeaturesMetadata::getNameForFeatureIDSet(
            FeatureIdSet,
            FeatureIdTextureCounter);
    const FCesiumPropertyTable& Table = PropertyTables[PropertyTableIndex];
    const FString TableName =
        UCesiumPropertyTableBlueprintLibrary::GetPropertyTableName(Table);

    if (!isFeatureIdSetEnabled(
            pFeaturesMetadataComponent,
            FeatureIdSetName,
            TableName) ||
        !isPropertyTableEnabledForGuid(
            pFeaturesMetadataComponent,
            PropertyTableName,
            TableName,
            GuidPropertyName)) {
      continue;
    }

    const FCesiumPropertyTableProperty& GuidProperty =
        UCesiumPropertyTableBlueprintLibrary::FindProperty(
            Table,
            GuidPropertyName);

    // 检查属性是否有效
    const ECesiumPropertyTablePropertyStatus Status =
        UCesiumPropertyTablePropertyBlueprintLibrary::
            GetPropertyTablePropertyStatus(GuidProperty);
    if (Status != ECesiumPropertyTablePropertyStatus::Valid &&
        Status !=
            ECesiumPropertyTablePropertyStatus::EmptyPropertyWithDefault) {
      continue;
    }

    const int64 PropertyTableCount =
        UCesiumPropertyTableBlueprintLibrary::GetPropertyTableCount(Table);
    TSet<int64> FeatureIds;
    collectFeatureIdsForSet(
        PrimitiveFeatures,
        FeatureIdSet,
        TilePrimitive.GetMeshComponent(),
        FeatureIds);

    for (int64 FeatureID : FeatureIds) {
      if (FeatureID < 0 || FeatureID >= PropertyTableCount) {
        continue;
      }

      FString GuidValue =
          UCesiumPropertyTablePropertyBlueprintLibrary::GetString(
              GuidProperty,
              FeatureID,
              TEXT(""));
      if (!GuidValue.IsEmpty()) {
        FoundGuidSet.Add(MoveTemp(GuidValue));
      }
    }
  }

  TArray<FString> FoundGuids = FoundGuidSet.Array();

  // 根据找到的 GUID 计算最终颜色分类
  const ECesiumGuidColorCategory EffectiveCategory =
      ComputeEffectiveCategory(FoundGuids);

  // 获取图元组件引用（用于 CustomDepth Stencil 方式）
  UStaticMeshComponent& MeshComp = TilePrimitive.GetMeshComponent();

  // 应用颜色到材质和/或图元
  ApplyColorToComponents(&Material, &MeshComp, EffectiveCategory);

  // 将该图元记录到已加载列表，供后续分类变更时实时更新
  // 仅在发现 GUID 或使用 Stencil 模式（需要记录图元引用）时记录
  if (FoundGuids.Num() > 0 || bUseCustomDepthStencil) {
    FJsczLoadedPrimitive Record;
    Record.Material = &Material;
    Record.Primitive = &MeshComp;
    Record.Guids = MoveTemp(FoundGuids);
    LoadedPrimitives.Add(MoveTemp(Record));
  }
}

// ============================================================
// JSCZ: OnTileUnloading（ICesium3DTilesetLifecycleEventReceiver）
// Tile 卸载时触发。
// 由于使用弱指针，材质引用会自动失效，此处主动剪枝
// 防止 LoadedPrimitives 列表随时间无限增长
// ============================================================
void UCesiumGuidColorManager::OnTileUnloading(ICesiumLoadedTile& Tile) {
  // 阈值策略：当无效条目积累到一定比例时才执行清理，降低频繁剪枝的开销
  constexpr int32 PruneThreshold = 200;
  if (LoadedPrimitives.Num() > PruneThreshold) {
    PruneInvalidPrimitives();
  }
}

// ============================================================
// JSCZ: GetColorForCategory（内部方法）
// 根据颜色分类枚举返回对应的 FLinearColor
// ============================================================
FLinearColor UCesiumGuidColorManager::GetColorForCategory(
    ECesiumGuidColorCategory Category) const {
  switch (Category) {
  case ECesiumGuidColorCategory::Red:
    return RedColor;
  case ECesiumGuidColorCategory::Yellow:
    return YellowColor;
  case ECesiumGuidColorCategory::Green:
    return GreenColor;
  default:
    return NoneColor;
  }
}

// ============================================================
// JSCZ: ComputeEffectiveCategory（内部方法）
// 根据图元包含的一组 GUID，通过优先级规则确定最终颜色分类。
// 优先级：Red(最高) > Yellow > Green > None(最低)
// 这样能确保在多个 GUID 分属不同分类时，颜色不会相互覆盖丢失
// ============================================================
ECesiumGuidColorCategory UCesiumGuidColorManager::ComputeEffectiveCategory(
    const TArray<FString>& Guids) const {
  ECesiumGuidColorCategory BestCategory = ECesiumGuidColorCategory::None;

  for (const FString& Guid : Guids) {
    const ECesiumGuidColorCategory* pCategory = GuidColorMap.Find(Guid);
    if (!pCategory) {
      continue;
    }

    // 取优先级更高的分类（枚举值越大优先级越低，Red=1最高，None=0保留为"无分类"）
    // 注意：枚举值定义：None=0（不参与优先级比较）, Red=1, Yellow=2, Green=3
    // 优先级逻辑：Red(1) > Yellow(2) > Green(3)，
    // BestCategory 初始为 None（表示尚未匹配到分类），
    // 一旦找到非 None 分类，后续仅比较枚举值大小（值越小优先级越高）
    if (BestCategory == ECesiumGuidColorCategory::None ||
        static_cast<uint8>(*pCategory) < static_cast<uint8>(BestCategory)) {
      BestCategory = *pCategory;
    }

    // 已达最高优先级（Red），无需继续查找
    if (BestCategory == ECesiumGuidColorCategory::Red) {
      break;
    }
  }

  return BestCategory;
}

// ============================================================
// JSCZ: ApplyColorToComponents（内部方法）
// 将颜色分类实际应用到材质参数或 CustomDepth Stencil。
// 支持两种模式（由 bUseCustomDepthStencil 属性控制）：
//   1. 材质参数方式：设置 HighlightColorParamName 对应的 Vector 参数
//   2. CustomDepth Stencil 方式：设置渲染自定义深度 + Stencil 值
// ============================================================
void UCesiumGuidColorManager::ApplyColorToComponents(
    UMaterialInstanceDynamic* pMaterial,
    UPrimitiveComponent* pPrimitive,
    ECesiumGuidColorCategory Category) {

  if (bUseCustomDepthStencil) {
    // ---- CustomDepth Stencil 方式 ----
    // 无需修改材质，通过后处理材质读取 Stencil 值实现着色
    if (!IsValid(pPrimitive)) {
      return;
    }

    if (Category == ECesiumGuidColorCategory::None) {
      // 无高亮：关闭自定义深度渲染
      pPrimitive->SetRenderCustomDepth(false);
      pPrimitive->SetCustomDepthStencilValue(0);
    } else {
      // 开启自定义深度渲染，Stencil 值表示颜色分类
      // Red=RedStencilValue, Yellow=RedStencilValue+1, Green=RedStencilValue+2
      pPrimitive->SetRenderCustomDepth(true);
      const int32 StencilValue =
          RedStencilValue + (static_cast<int32>(Category) - 1);
      pPrimitive->SetCustomDepthStencilValue(StencilValue);
    }
  } else {
    // ---- 材质 Vector 参数方式 ----
    // 通过设置材质动态实例的 Vector 参数实现高亮
    // 需要在基础材质中添加名为 HighlightColorParamName 的 Vector 参数
    if (!IsValid(pMaterial)) {
      return;
    }

    pMaterial->SetVectorParameterValue(
        HighlightColorParamName,
        GetColorForCategory(Category));
  }
}

// ============================================================
// JSCZ: RefreshAllLoadedMaterials（内部方法）
// 遍历 LoadedPrimitives 列表，对每个有效图元重新计算颜色并应用。
// 在 SetGuidCategory / ClearAllCategories / ClearCategory 后调用，
// 实现分类变更的实时视觉反馈。
// 性能说明：通常已加载图元数量远小于总 GUID 数量（视距限制），
// 此遍历在实际场景中代价很小。
// ============================================================
void UCesiumGuidColorManager::RefreshAllLoadedMaterials() {
  bool bHasInvalid = false;

  for (FJsczLoadedPrimitive& Record : LoadedPrimitives) {
    // 获取有效的材质和图元引用
    UMaterialInstanceDynamic* pMaterial = Record.Material.Get();
    UPrimitiveComponent* pPrimitive = Record.Primitive.Get();

    // 检查引用是否仍然有效（Tile 未被卸载）
    if (!pMaterial && !pPrimitive) {
      bHasInvalid = true;
      continue;
    }

    // 重新计算该图元的颜色分类并应用
    const ECesiumGuidColorCategory EffectiveCategory =
        ComputeEffectiveCategory(Record.Guids);
    ApplyColorToComponents(pMaterial, pPrimitive, EffectiveCategory);
  }

  // 如果检测到无效引用，顺便清理
  if (bHasInvalid) {
    PruneInvalidPrimitives();
  }
}

// ============================================================
// JSCZ: PruneInvalidPrimitives（内部方法）
// 移除 LoadedPrimitives 中材质和图元引用均已失效的条目。
// 防止 LoadedPrimitives 随时间无限增长消耗内存。
// ============================================================
void UCesiumGuidColorManager::PruneInvalidPrimitives() {
  // 使用 RemoveAll 高效原地过滤无效条目
  LoadedPrimitives.RemoveAll([](const FJsczLoadedPrimitive& Record) {
    // 如果材质和图元引用都已失效，则移除该条目
    return !Record.Material.IsValid() && !Record.Primitive.IsValid();
  });
}

// ============================================================
// JSCZ: 使用示例（代码片段）
//
// // 1. 在 Actor 上添加 UCesiumGuidColorManager 组件
// UCesiumGuidColorManager* ColorMgr =
//     Cast<UCesiumGuidColorManager>(
//         Actor->AddComponentByClass(
//             UCesiumGuidColorManager::StaticClass(), false, ...));
// ColorMgr->GuidPropertyName = TEXT("guid");
//
// // 2. 批量设置红色高亮
// TArray<FString> RedGuids = { TEXT("abc-001"), TEXT("abc-002") };
// ColorMgr->SetGuidCategory(RedGuids, ECesiumGuidColorCategory::Red);
//
// // 3. 通过命令字符串分类（适合外部系统推送）
// ColorMgr->ApplyColorCommand(TEXT("Yellow:abc-003,abc-004,abc-005"));
//
// // 4. 清除所有高亮
// ColorMgr->ClearAllCategories();
// ============================================================

#pragma endregion
