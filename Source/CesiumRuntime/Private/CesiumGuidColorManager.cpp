// Copyright 2020-2025 CesiumGS, Inc. and Contributors
// JSCZ - GUID颜色管理器实现

#include "CesiumGuidColorManager.h"

#include "Cesium3DTileset.h"
#include "CesiumFeatureIdSet.h"
#include "CesiumModelMetadata.h"
#include "CesiumPrimitiveFeatures.h"
#include "CesiumPropertyTable.h"
#include "CesiumPropertyTableProperty.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

#pragma region JSCZ

UCesiumGuidColorManager::UCesiumGuidColorManager() {
  // 此组件不需要每帧 Tick
  PrimaryComponentTick.bCanEverTick = false;
}

void UCesiumGuidColorManager::BeginPlay() {
  Super::BeginPlay();

  // 如果启用自动注册，则在游戏开始时自动查找父级 Tileset 并注册
  if (bAutoRegister) {
    ACesium3DTileset* Tileset = Cast<ACesium3DTileset>(GetOwner());
    if (Tileset) {
      RegisterWithTileset(Tileset);
    }
  }
}

void UCesiumGuidColorManager::EndPlay(
    const EEndPlayReason::Type EndPlayReason) {
  // 清理所有缓存数据
  CachedPrimitives.Empty();
  ManagedTextures.Empty();
  GuidColorMap.Empty();

  Super::EndPlay(EndPlayReason);
}

void UCesiumGuidColorManager::RegisterWithTileset(ACesium3DTileset* Tileset) {
  if (Tileset) {
    // 将自身注册为 Tileset 的生命周期事件接收器
    // 此后所有新加载的瓦片都会触发 CustomizeMaterial 等回调
    Tileset->SetLifecycleEventReceiver(this);
  }
}

void UCesiumGuidColorManager::SetGuidColors(
    const TArray<FString>& Guids,
    FLinearColor Color) {
  // 将每个 GUID 添加到颜色映射表中
  for (const FString& Guid : Guids) {
    GuidColorMap.Add(Guid, Color);
  }
}

void UCesiumGuidColorManager::RemoveGuidColors(
    const TArray<FString>& Guids) {
  // 从颜色映射表中移除指定的 GUID
  for (const FString& Guid : Guids) {
    GuidColorMap.Remove(Guid);
  }
}

void UCesiumGuidColorManager::ClearAllGuidColors() {
  // 清空整个颜色映射表
  GuidColorMap.Empty();
}

void UCesiumGuidColorManager::RefreshAllColors() {
  // 先清理已失效的缓存条目
  CleanupStaleCacheEntries();

  // 遍历所有缓存的图元，根据最新的颜色映射重新生成颜色数据
  for (FPrimitiveColorInfo& Info : CachedPrimitives) {
    if (!Info.Material.IsValid() || !Info.ColorTexture) {
      continue;
    }

    // 使用最新的 GuidColorMap 重新构建颜色数组
    TArray<FLinearColor> Colors = BuildColorArray(Info.FeatureGuids);

    // 原地更新纹理数据（不重新创建纹理对象）
    UpdateColorTexture(Info.ColorTexture, Colors);

    // 重新设置材质参数确保更新生效
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
  // 流程：
  // 1. 从图元的要素数据中读取每个要素的 GUID
  // 2. 根据 GuidColorMap 查找每个 GUID 对应的颜色
  // 3. 生成颜色查找纹理（每个像素 = 一个要素的颜色）
  // 4. 将纹理设置到材质参数中

  // 步骤1：读取此图元所有要素的 GUID
  TArray<FString> FeatureGuids = ReadFeatureGuids(TilePrimitive);

  // 如果没有要素或没有 GUID 属性，跳过此图元
  if (FeatureGuids.Num() == 0) {
    return;
  }

  // 步骤2：根据当前颜色映射构建颜色数组
  TArray<FLinearColor> Colors = BuildColorArray(FeatureGuids);

  // 步骤3：创建颜色查找纹理
  UTexture2D* ColorTexture = CreateColorTexture(FeatureGuids.Num(), Colors);
  if (!ColorTexture) {
    return;
  }

  // 步骤4：设置材质纹理参数
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

  // 缓存信息，用于后续调用 RefreshAllColors() 时更新
  FPrimitiveColorInfo CacheInfo;
  CacheInfo.Material = &Material;
  CacheInfo.ColorTexture = ColorTexture;
  CacheInfo.FeatureGuids = MoveTemp(FeatureGuids);
  CachedPrimitives.Add(MoveTemp(CacheInfo));
}

void UCesiumGuidColorManager::OnTileUnloading(ICesiumLoadedTile& Tile) {
  // 瓦片卸载时，清理无效的缓存条目
  // 被卸载瓦片的材质将变为无效，延迟清理策略会在下次访问时移除
  CleanupStaleCacheEntries();
}

TArray<FString> UCesiumGuidColorManager::ReadFeatureGuids(
    ICesiumLoadedTilePrimitive& TilePrimitive) const {
  TArray<FString> Result;

  // 获取图元的要素ID集合（FeatureIdSet）
  const FCesiumPrimitiveFeatures& Features =
      TilePrimitive.GetPrimitiveFeatures();
  const TArray<FCesiumFeatureIdSet>& FeatureIdSets =
      UCesiumPrimitiveFeaturesBlueprintLibrary::GetFeatureIDSets(Features);

  if (FeatureIdSets.Num() == 0) {
    return Result;
  }

  // 获取模型元数据中的属性表（PropertyTable）
  const FCesiumModelMetadata& ModelMetadata =
      TilePrimitive.GetLoadedTile().GetModelMetadata();
  TArray<FCesiumPropertyTable> PropertyTables =
      UCesiumModelMetadataBlueprintLibrary::GetPropertyTables(ModelMetadata);

  if (PropertyTables.Num() == 0) {
    return Result;
  }

  // 遍历要素ID集合，查找包含 GUID 属性的属性表
  for (const FCesiumFeatureIdSet& FeatureIdSet : FeatureIdSets) {
    // 获取此要素ID集合关联的属性表索引
    int64 TableIndex =
        UCesiumFeatureIdSetBlueprintLibrary::GetPropertyTableIndex(
            FeatureIdSet);

    if (TableIndex < 0 || TableIndex >= PropertyTables.Num()) {
      continue;
    }

    const FCesiumPropertyTable& Table = PropertyTables[TableIndex];

    // 获取属性表中所有属性，检查是否包含 GUID 属性
    TMap<FString, FCesiumPropertyTableProperty> Properties =
        UCesiumPropertyTableBlueprintLibrary::GetProperties(Table);

    FCesiumPropertyTableProperty* GuidProp = Properties.Find(GuidPropertyName);
    if (!GuidProp) {
      continue;
    }

    // 找到了 GUID 属性，读取所有要素的 GUID 值
    int64 FeatureCount =
        UCesiumPropertyTablePropertyBlueprintLibrary::GetPropertySize(
            *GuidProp);

    Result.SetNum(FeatureCount);
    for (int64 i = 0; i < FeatureCount; i++) {
      Result[i] = UCesiumPropertyTablePropertyBlueprintLibrary::GetString(
          *GuidProp,
          i,
          TEXT(""));
    }

    // 使用第一个匹配的属性表
    return Result;
  }

  return Result;
}

TArray<FLinearColor>
UCesiumGuidColorManager::BuildColorArray(
    const TArray<FString>& FeatureGuids) const {
  TArray<FLinearColor> Colors;
  Colors.SetNum(FeatureGuids.Num());

  // 根据 GUID 查找颜色映射，未匹配的使用默认颜色
  for (int32 i = 0; i < FeatureGuids.Num(); i++) {
    const FLinearColor* FoundColor = GuidColorMap.Find(FeatureGuids[i]);
    Colors[i] = FoundColor ? *FoundColor : DefaultColor;
  }

  return Colors;
}

UTexture2D* UCesiumGuidColorManager::CreateColorTexture(
    int64 FeatureCount,
    const TArray<FLinearColor>& Colors) {
  if (FeatureCount <= 0) {
    return nullptr;
  }

  // 计算纹理尺寸：宽度最大4096像素，超出部分自动换行
  int32 Width = FMath::Min(static_cast<int32>(FeatureCount), 4096);
  int32 Height =
      FMath::CeilToInt(static_cast<float>(FeatureCount) / 4096.0f);

  // 创建临时纹理（非持久化，仅存在于内存中）
  UTexture2D* Texture =
      UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
  if (!Texture) {
    return nullptr;
  }

  // 配置纹理属性
  Texture->Filter = TF_Nearest;  // 最近邻过滤：精确的要素ID到颜色映射
  Texture->SRGB = false;         // 线性颜色空间
  Texture->AddressX = TA_Clamp;  // 边缘钳制
  Texture->AddressY = TA_Clamp;
  Texture->NeverStream = true;  // 不使用纹理流送

  // 填充纹理像素数据
  FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
  void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
  uint8* Pixels = static_cast<uint8*>(Data);

  // 清零所有像素
  int64 TotalPixels = static_cast<int64>(Width) * Height;
  FMemory::Memzero(Pixels, TotalPixels * 4);

  // 逐像素写入颜色数据（BGRA格式）
  for (int64 i = 0; i < FeatureCount && i < Colors.Num(); i++) {
    FColor C = Colors[i].ToFColor(false);
    int64 Offset = i * 4;
    Pixels[Offset + 0] = C.B;
    Pixels[Offset + 1] = C.G;
    Pixels[Offset + 2] = C.R;
    Pixels[Offset + 3] = C.A;
  }

  Mip.BulkData.Unlock();

  // 上传纹理数据到 GPU
  Texture->UpdateResource();

  // 添加到管理列表，防止被GC回收
  ManagedTextures.Add(Texture);

  return Texture;
}

void UCesiumGuidColorManager::UpdateColorTexture(
    UTexture2D* Texture,
    const TArray<FLinearColor>& Colors) {
  if (!Texture) {
    return;
  }

  int32 Width = Texture->GetSizeX();
  int32 Height = Texture->GetSizeY();

  FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
  void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
  uint8* Pixels = static_cast<uint8*>(Data);

  // 清零所有像素
  int64 TotalPixels = static_cast<int64>(Width) * Height;
  FMemory::Memzero(Pixels, TotalPixels * 4);

  // 重新写入颜色数据
  for (int64 i = 0; i < Colors.Num() && i < TotalPixels; i++) {
    FColor C = Colors[i].ToFColor(false);
    int64 Offset = i * 4;
    Pixels[Offset + 0] = C.B;
    Pixels[Offset + 1] = C.G;
    Pixels[Offset + 2] = C.R;
    Pixels[Offset + 3] = C.A;
  }

  Mip.BulkData.Unlock();

  // 重新上传纹理数据到 GPU
  Texture->UpdateResource();
}

void UCesiumGuidColorManager::CleanupStaleCacheEntries() {
  // 从后向前遍历，移除材质已失效的缓存条目
  for (int32 i = CachedPrimitives.Num() - 1; i >= 0; --i) {
    if (!CachedPrimitives[i].Material.IsValid()) {
      // 材质已被销毁，同时释放对应的纹理
      UTexture2D* Tex = CachedPrimitives[i].ColorTexture;
      if (Tex) {
        ManagedTextures.Remove(Tex);
      }
      CachedPrimitives.RemoveAtSwap(i);
    }
  }
}

#pragma endregion
