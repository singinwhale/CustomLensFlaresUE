// Copyright Aesir Interactive, GmbH. All Rights Reserved.


#include "CustomLensFlareSceneViewExtensionData.h"

TStrongObjectPtr<UCustomLensFlareConfig> FCustomLensFlareSceneViewExtensionData::BaseConfig;
FCustomLensFlareSceneViewExtensionData::FCustomLensFlareSceneViewExtensionData()
{
	if (!BaseConfig.IsValid())
	{
		FString ConfigPath;
		if (GConfig->GetString(TEXT("CustomLensFlareSceneViewExtension"), TEXT("ConfigPath"), ConfigPath, GEngineIni))
		{
			UCustomLensFlareConfig* LoadedConfig = LoadObject<UCustomLensFlareConfig>(nullptr, *ConfigPath);
			check(LoadedConfig);
			BaseConfig = TStrongObjectPtr(LoadedConfig);
		}
	}
}

const TCHAR* FCustomLensFlareSceneViewExtensionData::GSubclassIdentifier = TEXT("CustomLensFlareSceneViewExtensionData");

const TCHAR* FCustomLensFlareSceneViewExtensionData::GetSubclassIdentifier() const
{
	return GSubclassIdentifier;
}

FCustomLensFlareSceneViewExtensionData::FPerViewExtensionData* FCustomLensFlareSceneViewExtensionData::GetOrCreateViewExtensionData(FSceneView& SceneView) const
{
	FPerViewExtensionData* PerViewExtensionData = PerViewData.Find(SceneView.State);
	if (!PerViewExtensionData)
	{
		PerViewExtensionData = &PerViewData.Add(SceneView.State);
		BaseConfig->OverrideBlendableSettings(SceneView, 1.0f);
	}
	return PerViewExtensionData;
}

const FCustomLensFlareSceneViewExtensionData::FPerViewExtensionData* FCustomLensFlareSceneViewExtensionData::GetViewExtensionData(const FSceneView& SceneView) const
{
	return PerViewData.Find(SceneView.State);
}
