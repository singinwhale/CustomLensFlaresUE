// Copyright Manuel Wagner (singinwhale.com). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CustomLensFlareComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CUSTOMLENSFLARE_API UCustomLensFlareComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCustomLensFlareComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Lens Flare")
	TObjectPtr<class UCustomLensFlareConfig> CustomLensFlareConfig;
};
