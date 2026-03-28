// Copyright 2020-2024 CesiumGS, Inc. and Contributors

// ============================================================
// JSCZ: ACesiumGuidColorExample 实现
// 演示 UCesiumGuidColorManager 的4种典型使用场景
// ============================================================

#pragma region JSCZ

#include "CesiumGuidColorExample.h"
#include "CesiumGuidColorManager.h"

// ============================================================
// JSCZ: 构造函数
// 创建并绑定 UCesiumGuidColorManager 子组件
// ============================================================
ACesiumGuidColorExample::ACesiumGuidColorExample() {
  PrimaryActorTick.bCanEverTick = false;

  // 创建 GUID 颜色管理器组件作为根组件的子组件
  GuidColorManager = CreateDefaultSubobject<UCesiumGuidColorManager>(
      TEXT("GuidColorManager"));
}

// ============================================================
// JSCZ: BeginPlay
// 游戏开始时自动输出使用提示
// ============================================================
void ACesiumGuidColorExample::BeginPlay() {
  Super::BeginPlay();

  UE_LOG(
      LogTemp,
      Log,
      TEXT("[JSCZ] CesiumGuidColorExample 已启动。"
           "已自动注册到场景中所有 ACesium3DTileset。"
           "请调用 HighlightBuildings() 开始高亮示例。"));
}

// ============================================================
// JSCZ: 场景1 - 批量高亮建筑
//
// 将编辑器中配置的 RedGuids / YellowGuids / GreenGuids
// 分别批量设置为对应的颜色分类。
// 当 Tileset 动态加载新 Tile 时，匹配的 GUID 将自动高亮，
// 无需在加载后再手动调用。
// ============================================================
void ACesiumGuidColorExample::HighlightBuildings() {
  if (!IsValid(GuidColorManager)) {
    return;
  }

  // 批量设置红色高亮
  if (RedGuids.Num() > 0) {
    GuidColorManager->SetGuidCategory(
        RedGuids,
        ECesiumGuidColorCategory::Red);
    UE_LOG(
        LogTemp,
        Log,
        TEXT("[JSCZ] 已将 %d 个 GUID 设为红色高亮"),
        RedGuids.Num());
  }

  // 批量设置黄色高亮
  if (YellowGuids.Num() > 0) {
    GuidColorManager->SetGuidCategory(
        YellowGuids,
        ECesiumGuidColorCategory::Yellow);
    UE_LOG(
        LogTemp,
        Log,
        TEXT("[JSCZ] 已将 %d 个 GUID 设为黄色高亮"),
        YellowGuids.Num());
  }

  // 批量设置绿色高亮
  if (GreenGuids.Num() > 0) {
    GuidColorManager->SetGuidCategory(
        GreenGuids,
        ECesiumGuidColorCategory::Green);
    UE_LOG(
        LogTemp,
        Log,
        TEXT("[JSCZ] 已将 %d 个 GUID 设为绿色高亮"),
        GreenGuids.Num());
  }

  UE_LOG(
      LogTemp,
      Log,
      TEXT("[JSCZ] 当前共注册 %d 个 GUID"),
      GuidColorManager->GetTotalGuidCount());
}

// ============================================================
// JSCZ: 场景2 - 清除所有高亮
//
// 清除全部 GUID 分类，恢复所有已加载建筑为无高亮状态。
// 同时实时更新当前已加载的所有 Tile 材质。
// ============================================================
void ACesiumGuidColorExample::ClearHighlights() {
  if (!IsValid(GuidColorManager)) {
    return;
  }

  GuidColorManager->ClearAllCategories();
  UE_LOG(LogTemp, Log, TEXT("[JSCZ] 已清除所有建筑高亮"));
}

// ============================================================
// JSCZ: 场景3 - 按类型分三色分类
//
// 演示典型的城市要素分类场景（如：
//   红色 = 危险建筑 / 警报状态
//   黄色 = 待检修建筑 / 预警状态
//   绿色 = 正常建筑 / 安全状态）
// ============================================================
void ACesiumGuidColorExample::ClassifyBuildingsByType() {
  if (!IsValid(GuidColorManager)) {
    return;
  }

  // 先清除旧分类
  GuidColorManager->ClearAllCategories();

  // 示例数据：危险建筑（红色）
  TArray<FString> DangerousBuildings = {
      TEXT("danger-001"),
      TEXT("danger-002"),
      TEXT("danger-003"),
  };
  GuidColorManager->SetGuidCategory(
      DangerousBuildings,
      ECesiumGuidColorCategory::Red);

  // 示例数据：待检修建筑（黄色）
  TArray<FString> MaintenanceBuildings = {
      TEXT("maint-001"),
      TEXT("maint-002"),
      TEXT("maint-003"),
      TEXT("maint-004"),
  };
  GuidColorManager->SetGuidCategory(
      MaintenanceBuildings,
      ECesiumGuidColorCategory::Yellow);

  // 示例数据：正常建筑（绿色）
  TArray<FString> SafeBuildings = {
      TEXT("safe-001"),
      TEXT("safe-002"),
  };
  GuidColorManager->SetGuidCategory(
      SafeBuildings,
      ECesiumGuidColorCategory::Green);

  UE_LOG(
      LogTemp,
      Log,
      TEXT("[JSCZ] 三色分类完成：红色=%d，黄色=%d，绿色=%d"),
      DangerousBuildings.Num(),
      MaintenanceBuildings.Num(),
      SafeBuildings.Num());
}

// ============================================================
// JSCZ: 场景4 - 通过命令字符串分类
//
// 演示 ApplyColorCommand() 的用法。
// 此方式适合与外部数据系统（如 WebSocket / REST API）对接，
// 外部系统直接推送格式化命令字符串，无需调用 C++ API。
//
// 命令格式：
//   "Red:guid1,guid2,guid3"    将 GUID 设为红色
//   "Yellow:guid1,guid2"       将 GUID 设为黄色
//   "Green:guid1"              将 GUID 设为绿色
//   "None:guid1,guid2"         清除 GUID 高亮
//   "Clear:Red"                清除所有红色分类
//   "Clear:All"                清除全部分类
// ============================================================
void ACesiumGuidColorExample::SendClassifyCommand() {
  if (!IsValid(GuidColorManager)) {
    return;
  }

  // 示例命令1：将3个建筑设为红色高亮（模拟服务器推送火警报警）
  GuidColorManager->ApplyColorCommand(
      TEXT("Red:building-fire-001,building-fire-002,building-fire-003"));

  // 示例命令2：将2个建筑设为黄色（模拟推送预警通知）
  GuidColorManager->ApplyColorCommand(
      TEXT("Yellow:building-warn-001,building-warn-002"));

  // 示例命令3：将已安全建筑设为绿色（模拟恢复正常）
  GuidColorManager->ApplyColorCommand(TEXT("Green:building-ok-001"));

  // 示例命令4：清除某个不再高亮的建筑（通过 None 分类清除）
  GuidColorManager->ApplyColorCommand(TEXT("None:building-old-001"));

  UE_LOG(
      LogTemp,
      Log,
      TEXT("[JSCZ] 命令字符串分类演示完成，当前注册 %d 个 GUID"),
      GuidColorManager->GetTotalGuidCount());
}

// ============================================================
// JSCZ: 最简使用代码片段（复制即用）
//
// --- C++ 方式 ---
//
// // 1. 添加组件
// UCesiumGuidColorManager* Mgr =
//     NewObject<UCesiumGuidColorManager>(MyActor);
// Mgr->GuidPropertyName = TEXT("guid"); // 与glTF属性名一致
// MyActor->AddInstanceComponent(Mgr);
// Mgr->RegisterComponent();
//
// // 2. 批量高亮（可在任意时刻调用，已加载模型立即刷新）
// TArray<FString> Ids = { TEXT("id-001"), TEXT("id-002") };
// Mgr->SetGuidCategory(Ids, ECesiumGuidColorCategory::Red);
//
// // 3. 通过命令字符串（适合网络/外部推送）
// Mgr->ApplyColorCommand(TEXT("Yellow:id-003,id-004"));
//
// // 4. 清除
// Mgr->ClearAllCategories();
//
// --- Blueprint 方式 ---
// 在 Actor Blueprint 中添加 "Cesium GUID Color Manager [JSCZ]" 组件，
// 在事件图中调用 "Set Guid Category" / "Apply Color Command" 节点即可。
// ============================================================

#pragma endregion
