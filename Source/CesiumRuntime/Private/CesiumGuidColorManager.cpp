// Copyright 2020-2025 CesiumGS, Inc. and Contributors
// JSCZ - GUID颜色管理器实现
// 核心逻辑：通过 CesiumFeaturesMetadataComponent 获取 FeatureIdSets
// 和 PropertyTables，以属性值作为 GUID，通过材质赋予面颜色

#include "CesiumGuidColorManager.h"

#include "Cesium3DTileset.h"
#include "CesiumFeatureIdSet.h"
#include "CesiumFeaturesMetadataComponent.h"
#include "CesiumModelMetadata.h"
#include "CesiumPrimitiveFeatures.h"
#include "CesiumPropertyTable.h"
#include "CesiumPropertyTableProperty.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

#pragma region JSCZ

UCesiumGuidColorManager::UCesiumGuidColorManager() {
  PrimaryComponentTick.bCanEverTick = false;
}

void UCesiumGuidColorManager::BeginPlay() {
  Super::BeginPlay();

  if (bAutoRegister) {
    ACesium3DTileset* Tileset = Cast<ACesium3DTileset>(GetOwner());
    if (Tileset) {
      RegisterWithTileset(Tileset);
    }
  }
}

void UCesiumGuidColorManager::EndPlay(
    const EEndPlayReason::Type EndPlayReason) {
  CachedPrimitives.Empty();
  ManagedTextures.Empty();
  GuidColorMap.Empty();

  Super::EndPlay(EndPlayReason);
}

void UCesiumGuidColorManager::RegisterWithTileset(ACesium3DTileset* Tileset) {
  if (!Tileset) {
    return;
  }

  // 检查 Tileset 上是否存在 CesiumFeaturesMetadataComponent
  // 这是使用 GUID 着色的前提条件：必须先配置好 FeatureIdSets 和 PropertyTables
  UCesiumFeaturesMetadataComponent* FeaturesMetadata =
      Tileset->FindComponentByClass<UCesiumFeaturesMetadataComponent>();
  if (!FeaturesMetadata) {
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[JSCZ] UCesiumGuidColorManager::RegisterWithTileset: "
             "Tileset 上未找到 CesiumFeaturesMetadataComponent！"
             "必须先添加并配置该组件（Description 中的 FeatureIdSets "
             "和 PropertyTables），否则 FeatureID 不会被编码到材质中。"));
  }

  // 注册为 Tileset 的生命周期事件接收器
  Tileset->SetLifecycleEventReceiver(this);

  UE_LOG(
      LogTemp,
      Log,
      TEXT("[JSCZ] UCesiumGuidColorManager: 已注册到 Tileset [%s], "
           "FeatureIdSetIndex=%d, GuidPropertyName=%s"),
      *Tileset->GetName(),
      FeatureIdSetIndex,
      *GuidPropertyName);
}

void UCesiumGuidColorManager::SetGuidColors(
    const TArray<FString>& Guids,
    FLinearColor Color) {
  for (const FString& Guid : Guids) {
    GuidColorMap.Add(Guid, Color);
  }
}

void UCesiumGuidColorManager::RemoveGuidColors(
    const TArray<FString>& Guids) {
  for (const FString& Guid : Guids) {
    GuidColorMap.Remove(Guid);
  }
}

void UCesiumGuidColorManager::ClearAllGuidColors() {
  GuidColorMap.Empty();
}

void UCesiumGuidColorManager::RefreshAllColors() {
  CleanupStaleCacheEntries();

  for (FPrimitiveColorInfo& Info : CachedPrimitives) {
    if (!Info.Material.IsValid() || !Info.ColorTexture) {
      continue;
    }

    TArray<FLinearColor> Colors = BuildColorArray(Info.FeatureGuids);
    UpdateColorTexture(Info.ColorTexture, Colors);

    Info.Material->SetTextureParameterValue(
        ColorTexParameterName, Info.ColorTexture);
  }
}

void UCesiumGuidColorManager::CustomizeMaterial(
    ICesiumLoadedTilePrimitive& TilePrimitive,
    UMaterialInstanceDynamic& Material,
    const UCesiumMaterialUserData* CesiumData,
    const CesiumGltf::Material& GltfMaterial) {
  // ===== 核心逻辑：瓦片动态加载时自动着色 =====
  //
  // 正确流程（通过 CesiumFeaturesMetadataComponent 配置的数据）：
  // 1. 从图元获取 FeatureIdSets（CesiumFeaturesMetadataComponent 已编码到材质）
  // 2. 使用 FeatureIdSetIndex 选择指定的 FeatureIdSet
  // 3. 通过 FeatureIdSet 的 PropertyTableIndex 找到关联的 PropertyTable
  // 4. 从 PropertyTable 中读取 GuidPropertyName 属性的值 → 作为 GUID
  // 5. 查 GuidColorMap 得到颜色 → 生成颜色纹理 → 设到材质参数

  // 步骤1-4：读取此图元所有要素的 GUID
  TArray<FString> FeatureGuids = ReadFeatureGuids(TilePrimitive);

  if (FeatureGuids.Num() == 0) {
    return;
  }

  // 步骤5a：根据当前颜色映射构建颜色数组
  TArray<FLinearColor> Colors = BuildColorArray(FeatureGuids);

  // 步骤5b：创建颜色查找纹理
  UTexture2D* ColorTexture = CreateColorTexture(FeatureGuids.Num(), Colors);
  if (!ColorTexture) {
    return;
  }

  // 步骤5c：设置材质纹理参数（材质 Shader 使用此纹理按 FeatureID 采样颜色）
  Material.SetTextureParameterValue(ColorTexParameterName, ColorTexture);

  // 设置纹理尺寸参数（供材质 Shader 计算 UV 坐标）
  int32 TexWidth = ColorTexture->GetSizeX();
  int32 TexHeight = ColorTexture->GetSizeY();
  Material.SetVectorParameterValue(
      ColorTexSizeParameterName,
      FLinearColor(
          static_cast<float>(TexWidth),
          static_cast<float>(TexHeight),
          0.0f,
          0.0f));

  // 缓存信息，用于后续 RefreshAllColors() 更新
  FPrimitiveColorInfo CacheInfo;
  CacheInfo.Material = &Material;
  CacheInfo.ColorTexture = ColorTexture;
  CacheInfo.FeatureGuids = MoveTemp(FeatureGuids);
  CachedPrimitives.Add(MoveTemp(CacheInfo));
}

void UCesiumGuidColorManager::OnTileUnloading(ICesiumLoadedTile& Tile) {
  CleanupStaleCacheEntries();
}

TArray<FString> UCesiumGuidColorManager::ReadFeatureGuids(
    ICesiumLoadedTilePrimitive& TilePrimitive) const {
  TArray<FString> Result;

  // ===== 从 FeatureIdSets 获取要素ID集合 =====
  //
  // FeatureIdSets 来源于 CesiumFeaturesMetadataComponent 的 Description 配置。
  // 只有在 Description.PrimitiveFeatures.FeatureIdSets 中配置的集合
  // 才会被 Cesium 编码到材质参数中（_FEATURE_ID_N）。

  const FCesiumPrimitiveFeatures& Features =
      TilePrimitive.GetPrimitiveFeatures();
  const TArray<FCesiumFeatureIdSet>& FeatureIdSets =
      UCesiumPrimitiveFeaturesBlueprintLibrary::GetFeatureIDSets(Features);

  if (FeatureIdSets.Num() == 0) {
    return Result;
  }

  // 使用指定索引的 FeatureIdSet
  if (FeatureIdSetIndex < 0 || FeatureIdSetIndex >= FeatureIdSets.Num()) {
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[JSCZ] ReadFeatureGuids: FeatureIdSetIndex=%d 超出范围 "
             "[0, %d)，请检查 CesiumFeaturesMetadataComponent 的 "
             "Description.PrimitiveFeatures.FeatureIdSets 配置"),
        FeatureIdSetIndex,
        FeatureIdSets.Num());
    return Result;
  }

  const FCesiumFeatureIdSet& SelectedFeatureIdSet =
      FeatureIdSets[FeatureIdSetIndex];

  // 获取此 FeatureIdSet 的要素数量
  int64 FeatureCount =
      UCesiumFeatureIdSetBlueprintLibrary::GetFeatureCount(
          SelectedFeatureIdSet);
  if (FeatureCount <= 0) {
    return Result;
  }

  // ===== 通过 PropertyTableIndex 找到关联的 PropertyTable =====
  //
  // FeatureIdSet 通过 PropertyTableIndex 关联到 PropertyTable。
  // PropertyTable 存储在 ModelMetadata 中（由 CesiumFeaturesMetadataComponent
  // 的 Description.ModelMetadata.PropertyTables 配置）。

  int64 TableIndex =
      UCesiumFeatureIdSetBlueprintLibrary::GetPropertyTableIndex(
          SelectedFeatureIdSet);

  const FCesiumModelMetadata& ModelMetadata =
      TilePrimitive.GetLoadedTile().GetModelMetadata();
  const TArray<FCesiumPropertyTable>& PropertyTables =
      UCesiumModelMetadataBlueprintLibrary::GetPropertyTables(ModelMetadata);

  if (TableIndex < 0 || TableIndex >= PropertyTables.Num()) {
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[JSCZ] ReadFeatureGuids: FeatureIdSet[%d] 的 "
             "PropertyTableIndex=%lld 无效（PropertyTable数量=%d）。"
             "请检查 CesiumFeaturesMetadataComponent 配置。"),
        FeatureIdSetIndex,
        TableIndex,
        PropertyTables.Num());
    return Result;
  }

  const FCesiumPropertyTable& Table = PropertyTables[TableIndex];

  // ===== 从 PropertyTable 中读取 GUID 属性值 =====
  //
  // PropertyTable 内部存储了多个属性（Property），
  // 其中 GuidPropertyName 对应的属性值作为 GUID 标识符。
  // 每个要素ID（0, 1, 2...）对应一个属性值。

  const TMap<FString, FCesiumPropertyTableProperty>& Properties =
      UCesiumPropertyTableBlueprintLibrary::GetProperties(Table);

  const FCesiumPropertyTableProperty* GuidProp =
      Properties.Find(GuidPropertyName);
  if (!GuidProp) {
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[JSCZ] ReadFeatureGuids: PropertyTable[%lld] 中未找到属性 "
             "'%s'。可用属性："),
        TableIndex,
        *GuidPropertyName);
    for (const auto& Pair : Properties) {
      UE_LOG(LogTemp, Warning, TEXT("  - %s"), *Pair.Key);
    }
    return Result;
  }

  // 读取每个要素的 GUID 值（按要素ID索引）
  Result.SetNum(FeatureCount);
  for (int64 i = 0; i < FeatureCount; i++) {
    Result[i] = UCesiumPropertyTablePropertyBlueprintLibrary::GetString(
        *GuidProp,
        i,
        TEXT(""));
  }

  return Result;
}

TArray<FLinearColor>
UCesiumGuidColorManager::BuildColorArray(
    const TArray<FString>& FeatureGuids) const {
  TArray<FLinearColor> Colors;
  Colors.SetNum(FeatureGuids.Num());

  for (int32 i = 0; i < FeatureGuids.Num(); i++) {
    const FLinearColor* FoundColor = GuidColorMap.Find(FeatureGuids[i]);
    Colors[i] = FoundColor ? *FoundColor : DefaultColor;
  }

  return Colors;
}

UTexture2D* UCesiumGuidColorManager::CreateColorTexture(
    int64 FeatureCount,
    const TArray<FLinearColor>& Colors) {
  // 纹理最大尺寸为 4096x4096（GPU 通用限制），最多支持 16,777,216 个要素
  constexpr int64 MaxFeatureCount = 4096LL * 4096LL;
  if (FeatureCount <= 0 || FeatureCount > MaxFeatureCount) {
    return nullptr;
  }

  int32 Width = FMath::Min(static_cast<int32>(FeatureCount), 4096);
  int32 Height =
      FMath::CeilToInt(static_cast<float>(FeatureCount) / 4096.0f);

  UTexture2D* Texture =
      UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
  if (!Texture) {
    return nullptr;
  }

  Texture->Filter = TF_Nearest;
  Texture->SRGB = false;
  Texture->AddressX = TA_Clamp;
  Texture->AddressY = TA_Clamp;
  Texture->NeverStream = true;

  FTexturePlatformData* PlatformData = Texture->GetPlatformData();
  if (!PlatformData || PlatformData->Mips.Num() == 0) {
    return nullptr;
  }

  FTexture2DMipMap& Mip = PlatformData->Mips[0];
  void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
  if (!Data) {
    Mip.BulkData.Unlock();
    return nullptr;
  }

  uint8* Pixels = static_cast<uint8*>(Data);

  int64 TotalPixels = static_cast<int64>(Width) * Height;
  FMemory::Memzero(Pixels, TotalPixels * 4);

  for (int64 i = 0; i < FeatureCount && i < Colors.Num(); i++) {
    FColor C = Colors[i].ToFColor(false);
    int64 Offset = i * 4;
    Pixels[Offset + 0] = C.B;
    Pixels[Offset + 1] = C.G;
    Pixels[Offset + 2] = C.R;
    Pixels[Offset + 3] = C.A;
  }

  Mip.BulkData.Unlock();

  Texture->UpdateResource();

  ManagedTextures.Add(Texture);

  return Texture;
}

void UCesiumGuidColorManager::UpdateColorTexture(
    UTexture2D* Texture,
    const TArray<FLinearColor>& Colors) {
  if (!Texture) {
    return;
  }

  FTexturePlatformData* PlatformData = Texture->GetPlatformData();
  if (!PlatformData || PlatformData->Mips.Num() == 0) {
    return;
  }

  int32 Width = Texture->GetSizeX();
  int32 Height = Texture->GetSizeY();

  FTexture2DMipMap& Mip = PlatformData->Mips[0];
  void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
  if (!Data) {
    Mip.BulkData.Unlock();
    return;
  }

  uint8* Pixels = static_cast<uint8*>(Data);

  int64 TotalPixels = static_cast<int64>(Width) * Height;
  FMemory::Memzero(Pixels, TotalPixels * 4);

  for (int64 i = 0; i < Colors.Num() && i < TotalPixels; i++) {
    FColor C = Colors[i].ToFColor(false);
    int64 Offset = i * 4;
    Pixels[Offset + 0] = C.B;
    Pixels[Offset + 1] = C.G;
    Pixels[Offset + 2] = C.R;
    Pixels[Offset + 3] = C.A;
  }

  Mip.BulkData.Unlock();

  Texture->UpdateResource();
}

void UCesiumGuidColorManager::CleanupStaleCacheEntries() {
  for (int32 i = CachedPrimitives.Num() - 1; i >= 0; --i) {
    if (!CachedPrimitives[i].Material.IsValid()) {
      UTexture2D* Tex = CachedPrimitives[i].ColorTexture;
      if (Tex) {
        ManagedTextures.Remove(Tex);
      }
      CachedPrimitives.RemoveAtSwap(i);
    }
  }
}

#pragma endregion
