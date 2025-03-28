// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomLensFlare.h"

#include "CustomLensFlareSceneViewExtension.h"
#include "SceneViewExtension.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FCustomLensFlareModule"

void FCustomLensFlareModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FCustomLensFlareModule::SetupCustomLensFlares);
	if (GEngine && GEngine->IsInitialized())
	{
		SetupCustomLensFlares();
	}
	
	FString BaseDir = IPluginManager::Get().FindPlugin(TEXT("CustomLensFlare"))->GetBaseDir();
	FString PluginShaderDir = FPaths::Combine( BaseDir, TEXT("Shaders") );
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/CustomLensFlare"), PluginShaderDir);
}

void FCustomLensFlareModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
	DestroyCustomLensFlares();
}

void FCustomLensFlareModule::SetupCustomLensFlares()
{
	if (!SceneViewExtensionInstance)
	{
		SceneViewExtensionInstance = FSceneViewExtensions::NewExtension<FCustomLensFlareSceneViewExtension>();
		SceneViewExtensionInstance->Initialize();
	}
}

void FCustomLensFlareModule::DestroyCustomLensFlares()
{
	SceneViewExtensionInstance.Reset();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCustomLensFlareModule, CustomLensFlare)