// Tencent is pleased to support the open source community by making UnLua available.
// 
// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the MIT License (the "License"); 
// you may not use this file except in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, 
// software distributed under the License is distributed on an "AS IS" BASIS, 
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
// See the License for the specific language governing permissions and limitations under the License.

#pragma once

#include "InputCoreTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "UnLuaCompatibility.h"
#include "UnLuaManager.generated.h"

UCLASS()
class UUnLuaManager : public UObject
{
    GENERATED_BODY()

public:
    UUnLuaManager();

    bool Bind(UObjectBaseUtility *Object, UClass *Class, const TCHAR *InModuleName, int32 InitializerTableRef = INDEX_NONE);

    bool OnModuleHotfixed(const TCHAR *InModuleName);

    void RemoveAttachedObject(UObjectBaseUtility *Object);

    void Cleanup(class UWorld *World, bool bFullCleanup);

    void PostCleanup();

    void OnMapLoaded(UWorld *World);

    void OnActorSpawned(class AActor *Actor);

    UFUNCTION()
    void OnActorDestroyed(class AActor *Actor);

    UFUNCTION()
    void OnLatentActionCompleted(int32 LinkID);

private:
    bool BindInternal(UObjectBaseUtility *Object, UClass *Class, const FString &InModuleName, bool bNewCreated);
    bool BindSurvivalObject(struct lua_State *L, UObjectBaseUtility *Object, UClass *Class, const char *ModuleName);
    bool ConditionalUpdateClass(UClass *Class, const TSet<FName> &LuaFunctions, TMap<FName, UFunction*> &UEFunctions);

    ENetMode CheckObjectNetMode(UObjectBaseUtility *Object, UClass *Class, bool bNewCreated);

    void OverrideFunctions(const TSet<FName> &LuaFunctions, TMap<FName, UFunction*> &UEFunctions, UClass *OuterClass, bool bCheckFuncNetMode = false, ENetMode NetMode = NM_Standalone);
    void OverrideFunction(UFunction *TemplateFunction, UClass *OuterClass, FName NewFuncName);
    void AddFunction(UFunction *TemplateFunction, UClass *OuterClass, FName NewFuncName);
    void ReplaceFunction(UFunction *TemplateFunction, UClass *OuterClass);

    void AddAttachedObject(UObjectBaseUtility *Object, int32 ObjectRef);

    void CleanupDuplicatedFunctions();
    void CleanupCachedNatives();
    void CleanupCachedScripts();

    TMap<UClass*, FString> ModuleNames;
    TMap<FString, UClass*> Classes;
    TMap<UClass*, TMap<FName, UFunction*>> OverridableFunctions;
    TMap<UClass*, TArray<UFunction*>> DuplicatedFunctions;
    TMap<FString, TSet<FName>> ModuleFunctions;
    TMap<UFunction*, FNativeFuncPtr> CachedNatives;
    TMap<UFunction*, TArray<uint8>> CachedScripts;

    TMap<UClass*, TArray<UClass*>> Base2DerivedClasses;

    TMap<UObjectBaseUtility*, int32> AttachedObjects;
    TSet<AActor*> AttachedActors;
    TSet<AActor*> ActorsWithoutWorld;
};
