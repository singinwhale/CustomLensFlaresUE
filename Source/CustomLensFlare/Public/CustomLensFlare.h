// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"


class FCustomLensFlareSceneViewExtension;

class FCustomLensFlareModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void SetupCustomLensFlares();
	void DestroyCustomLensFlares();
	
	TSharedPtr<FCustomLensFlareSceneViewExtension> SceneViewExtensionInstance;
};

