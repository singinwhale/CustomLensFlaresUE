// Copyright Aesir Interactive, GmbH. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CustomLensFlareConfig.h"
#include "UObject/Object.h"

/**
 * Extension data that will be created on demand or by FCustomLensFlareSceneViewExtension.
 * Hold per view data that for post process blending.
 */
class CUSTOMLENSFLARE_API FCustomLensFlareSceneViewExtensionData : public ISceneViewFamilyExtentionData
{
public:
	FCustomLensFlareSceneViewExtensionData();

	// - ISceneViewFamilyExtentionData
	static const TCHAR* GSubclassIdentifier;
	virtual const TCHAR* GetSubclassIdentifier() const override;
	// --

	struct FPerViewExtensionData
	{
		float Intensity = 1.0f;

		FLinearColor Tint = FLinearColor(1.0f, 0.85f, 0.7f, 1.0f);

		TObjectPtr<UTexture2D> Gradient = nullptr;

		float ThresholdLevel = 1.0f;

		float ThresholdRange = 1.0f;

		float GhostIntensity = 1.0f;

		float GhostChromaShift = 0.015f;

		FLensFlareGhostSettings Ghost1 = {FLinearColor(1.0f, 0.8f, 0.4f, 1.0f), -1.5};

		FLensFlareGhostSettings Ghost2 = {FLinearColor(1.0f, 1.0f, 0.6f, 1.0f), 2.5};

		FLensFlareGhostSettings Ghost3 = {FLinearColor(0.8f, 0.8f, 1.0f, 1.0f), -5.0};

		FLensFlareGhostSettings Ghost4 = {FLinearColor(0.5f, 1.0f, 0.4f, 1.0f), 10.0};

		FLensFlareGhostSettings Ghost5 = {FLinearColor(0.5f, 0.8f, 1.0f, 1.0f), 0.7};

		FLensFlareGhostSettings Ghost6 = {FLinearColor(0.9f, 1.0f, 0.8f, 1.0f), -0.4};

		FLensFlareGhostSettings Ghost7 = {FLinearColor(1.0f, 0.8f, 0.4f, 1.0f), -0.2};

		FLensFlareGhostSettings Ghost8 = {FLinearColor(0.9f, 0.7f, 0.7f, 1.0f), -0.1};

		float HaloIntensity = 1.0f;

		float HaloWidth = 0.6f;

		float HaloMask = 0.5f;

		float HaloCompression = 0.65f;

		float HaloChromaShift = 0.015f;

		float GlareIntensity = 0.02f;

		float GlareDivider = 60.0f;

		FVector GlareScale = FVector(1.0f, 1.0f, 1.0f);

		FVector GlareAngles = FVector(1.047197f, 1.570796f, 2.617994f);

		FLinearColor GlareTint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

		TObjectPtr<UTexture2D> GlareLineMask = nullptr;

		FLinearColor FlareTint = FLinearColor(1.0f, 0.85f, 0.7f, 1.0f);

		float FlareIntensity = 1.0;
	};

	FPerViewExtensionData* GetOrCreateViewExtensionData(FSceneView& SceneView) const;;
	const FPerViewExtensionData* GetViewExtensionData(const FSceneView& SceneView) const;;

private:
	mutable TMap<void*, FPerViewExtensionData> PerViewData;

	static TStrongObjectPtr<UCustomLensFlareConfig> BaseConfig;
};
