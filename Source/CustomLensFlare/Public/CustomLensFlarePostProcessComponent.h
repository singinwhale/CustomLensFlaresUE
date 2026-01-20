// Copyright Aesir Interactive, GmbH. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CustomLensFlarePostProcessComponent.generated.h"


class UCustomLensFlareConfig;

/**
 * While this component is active it will blend in its lens flare config.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CUSTOMLENSFLARE_API UCustomLensFlarePostProcessComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCustomLensFlarePostProcessComponent();

	// - UActorComponent
	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// --

	UFUNCTION(BlueprintCallable)
	void SetWeight(float NewWeight);
protected:
	void RemoveBlendable(UCustomLensFlareConfig* ConfigToRemove) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Custom Lens Flare")
	TObjectPtr<UCustomLensFlareConfig> CustomLensFlareConfig;

	/**
	 * Additional weight to be applied to the influence.
	 * Will be multiplied by the post process volumes overall weight before being used to blend in the config.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter=SetWeight, Category = "Custom Lens Flare")
	float Weight = 1.0f;

#if WITH_EDITORONLY_DATA
	TWeakObjectPtr<UCustomLensFlareConfig> LastUsedConfig;
#endif
};
