// Copyright Manuel Wagner (singinwhale.com). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CustomLensFlareConfig.generated.h"

// This custom struct is used to more easily
// setup and organize the settings for the Ghosts
USTRUCT(BlueprintType)
struct FLensFlareGhostSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Exedre")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Exedre")
	float Scale = 1.0f;
};

/**
 * 
 */
UCLASS()
class CUSTOMLENSFLARE_API UCustomLensFlareConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="General", meta=(UIMin = "0.0", UIMax = "10.0"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, Category="General")
	FLinearColor Tint = FLinearColor(1.0f, 0.85f, 0.7f, 1.0f);

	UPROPERTY(EditAnywhere, Category="General")
	TObjectPtr<UTexture2D> Gradient = nullptr;


	UPROPERTY(EditAnywhere, Category="Threshold", meta=(UIMin = "0.0", UIMax = "10.0"))
	float ThresholdLevel = 1.0f;

	UPROPERTY(EditAnywhere, Category="Threshold", meta=(UIMin = "0.01", UIMax = "10.0"))
	float ThresholdRange = 1.0f;


	UPROPERTY(EditAnywhere, Category="Ghosts", meta=(UIMin = "0.0", UIMax = "1.0"))
	float GhostIntensity = 1.0f;

	UPROPERTY(EditAnywhere, Category="Ghosts", meta=(UIMin = "0.0", UIMax = "1.0"))
	float GhostChromaShift = 0.015f;

	UPROPERTY(EditAnywhere, Category="Ghosts")
	FLensFlareGhostSettings Ghost1 = {FLinearColor(1.0f, 0.8f, 0.4f, 1.0f), -1.5};

	UPROPERTY(EditAnywhere, Category="Ghosts")
	FLensFlareGhostSettings Ghost2 = {FLinearColor(1.0f, 1.0f, 0.6f, 1.0f), 2.5};

	UPROPERTY(EditAnywhere, Category="Ghosts")
	FLensFlareGhostSettings Ghost3 = {FLinearColor(0.8f, 0.8f, 1.0f, 1.0f), -5.0};

	UPROPERTY(EditAnywhere, Category="Ghosts")
	FLensFlareGhostSettings Ghost4 = {FLinearColor(0.5f, 1.0f, 0.4f, 1.0f), 10.0};

	UPROPERTY(EditAnywhere, Category="Ghosts")
	FLensFlareGhostSettings Ghost5 = {FLinearColor(0.5f, 0.8f, 1.0f, 1.0f), 0.7};

	UPROPERTY(EditAnywhere, Category="Ghosts")
	FLensFlareGhostSettings Ghost6 = {FLinearColor(0.9f, 1.0f, 0.8f, 1.0f), -0.4};

	UPROPERTY(EditAnywhere, Category="Ghosts")
	FLensFlareGhostSettings Ghost7 = {FLinearColor(1.0f, 0.8f, 0.4f, 1.0f), -0.2};

	UPROPERTY(EditAnywhere, Category="Ghosts")
	FLensFlareGhostSettings Ghost8 = {FLinearColor(0.9f, 0.7f, 0.7f, 1.0f), -0.1};


	UPROPERTY(EditAnywhere, Category="Halo", meta=(UIMin = "0.0", UIMax = "1.0"))
	float HaloIntensity = 1.0f;

	UPROPERTY(EditAnywhere, Category="Halo", meta=(UIMin = "0.0", UIMax = "1.0"))
	float HaloWidth = 0.6f;

	UPROPERTY(EditAnywhere, Category="Halo", meta=(UIMin = "0.0", UIMax = "1.0"))
	float HaloMask = 0.5f;

	UPROPERTY(EditAnywhere, Category="Halo", meta=(UIMin = "0.0", UIMax = "1.0"))
	float HaloCompression = 0.65f;

	UPROPERTY(EditAnywhere, Category="Halo", meta=(UIMin = "0.0", UIMax = "1.0"))
	float HaloChromaShift = 0.015f;


	UPROPERTY(EditAnywhere, Category="Glare", meta=(UIMin = "0", UIMax = "10"))
	float GlareIntensity = 0.02f;

	UPROPERTY(EditAnywhere, Category="Glare", meta=(UIMin = "0.01", UIMax = "200"))
	float GlareDivider = 60.0f;

	UPROPERTY(EditAnywhere, Category="Glare", meta=(UIMin = "0.0", UIMax = "10.0"))
	FVector GlareScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, Category="Glare", meta=(UIMin = "0.0", UIMax = "10.0"))
	FVector GlareAngles = FVector(1.047197f, 1.570796f, 2.617994f);

	UPROPERTY(EditAnywhere, Category="Glare")
	FLinearColor GlareTint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, Category="Glare")
	TObjectPtr<UTexture2D> GlareLineMask = nullptr;

	UPROPERTY(EditAnywhere, Category="Flare")
	FLinearColor FlareTint = FLinearColor(1.0f, 0.85f, 0.7f, 1.0f);

	UPROPERTY(EditAnywhere, Category="Flare", meta=(UIMin = "0", UIMax = "1"))
	float FlareIntensity = 1.0;
};
