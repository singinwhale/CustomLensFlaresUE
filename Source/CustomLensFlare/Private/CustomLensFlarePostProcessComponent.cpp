// Copyright Aesir Interactive, GmbH. All Rights Reserved.


#include "CustomLensFlarePostProcessComponent.h"

// ReSharper disable once CppUnusedIncludeDirective
#include "CustomLensFlareConfig.h"
#include "Components/PostProcessComponent.h"

UCustomLensFlarePostProcessComponent::UCustomLensFlarePostProcessComponent()
{
	bAutoActivate = true;
	PrimaryComponentTick.bCanEverTick = false;
}

void UCustomLensFlarePostProcessComponent::Activate(bool bReset)
{
	Super::Activate(bReset);

	SetWeight(Weight);
}


void UCustomLensFlarePostProcessComponent::Deactivate()
{
	RemoveBlendable(CustomLensFlareConfig);
	Super::Deactivate();
}

#if WITH_EDITOR
void UCustomLensFlarePostProcessComponent::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property == nullptr)
		return;

	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UCustomLensFlarePostProcessComponent, CustomLensFlareConfig))
	{
		if (CustomLensFlareConfig)
		{
			SetWeight(Weight);
		}
		else
		{
			RemoveBlendable(LastUsedConfig.Get());
		}
	}

	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UCustomLensFlarePostProcessComponent, Weight))
	{
		SetWeight(Weight);
	}
}
#endif

void UCustomLensFlarePostProcessComponent::RemoveBlendable(UCustomLensFlareConfig* ConfigToRemove) const
{
	if (APostProcessVolume* PostProcessVolumeOwner = GetOwner<APostProcessVolume>())
	{
		PostProcessVolumeOwner->Settings.RemoveBlendable(ConfigToRemove);
	}
	else if (UPostProcessComponent* PostProcessComponent = GetOwner()->GetComponentByClass<UPostProcessComponent>())
	{
		PostProcessComponent->Settings.RemoveBlendable(ConfigToRemove);
	}
}

void UCustomLensFlarePostProcessComponent::SetWeight(float NewWeight)
{
	Weight = NewWeight;
	if (!CustomLensFlareConfig || NewWeight <= 0.0f)
	{
		RemoveBlendable(CustomLensFlareConfig);
		return;
	}

#if WITH_EDITOR
	LastUsedConfig = CustomLensFlareConfig;
#endif

	if (APostProcessVolume* PostProcessVolumeOwner = GetOwner<APostProcessVolume>())
	{
		PostProcessVolumeOwner->AddOrUpdateBlendable(CustomLensFlareConfig, NewWeight);
	}
	else if (UPostProcessComponent* PostProcessComponent = GetOwner()->GetComponentByClass<UPostProcessComponent>())
	{
		PostProcessComponent->AddOrUpdateBlendable(CustomLensFlareConfig, NewWeight);
	}
}
