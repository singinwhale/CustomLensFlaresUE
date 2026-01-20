// Copyright Manuel Wagner (singinwhale.com). All Rights Reserved.


#include "CustomLensFlareConfig.h"

#include "CustomLensFlareSceneViewExtensionData.h"

void UCustomLensFlareConfig::OverrideBlendableSettings(class FSceneView& View, float Weight) const
{
	const FCustomLensFlareSceneViewExtensionData* CustomLensFlareSceneViewExtensionData = const_cast<FSceneViewFamily*>(View.Family)->GetOrCreateExtentionData<FCustomLensFlareSceneViewExtensionData>();
	if (!CustomLensFlareSceneViewExtensionData)
		return;

	FCustomLensFlareSceneViewExtensionData::FPerViewExtensionData* PerViewData = CustomLensFlareSceneViewExtensionData->GetOrCreateViewExtensionData(View);
	if (!PerViewData)
		return;

	PerViewData->Intensity = FMath::Lerp(PerViewData->Intensity, Intensity, Weight);

	PerViewData->Tint = FMath::Lerp(PerViewData->Tint, Tint, Weight);

	PerViewData->Gradient = Gradient; // todo: respect weight

	PerViewData->ThresholdLevel = FMath::Lerp(PerViewData->ThresholdLevel, ThresholdLevel, Weight);

	PerViewData->ThresholdRange = FMath::Lerp(PerViewData->ThresholdRange, ThresholdRange, Weight);

	PerViewData->GhostIntensity = FMath::Lerp(PerViewData->GhostIntensity, GhostIntensity, Weight);

	PerViewData->GhostChromaShift = FMath::Lerp(PerViewData->GhostChromaShift, GhostChromaShift, Weight);

	PerViewData->Ghost1.Color = FMath::Lerp(PerViewData->Ghost1.Color, Ghost1.Color, Weight);
	PerViewData->Ghost1.Scale = FMath::Lerp(PerViewData->Ghost1.Scale, Ghost1.Scale, Weight);

	PerViewData->Ghost2.Color = FMath::Lerp(PerViewData->Ghost2.Color, Ghost2.Color, Weight);
	PerViewData->Ghost2.Scale = FMath::Lerp(PerViewData->Ghost2.Scale, Ghost2.Scale, Weight);

	PerViewData->Ghost3.Color = FMath::Lerp(PerViewData->Ghost3.Color, Ghost3.Color, Weight);
	PerViewData->Ghost3.Scale = FMath::Lerp(PerViewData->Ghost3.Scale, Ghost3.Scale, Weight);

	PerViewData->Ghost4.Color = FMath::Lerp(PerViewData->Ghost4.Color, Ghost4.Color, Weight);
	PerViewData->Ghost4.Scale = FMath::Lerp(PerViewData->Ghost4.Scale, Ghost4.Scale, Weight);

	PerViewData->Ghost5.Color = FMath::Lerp(PerViewData->Ghost5.Color, Ghost5.Color, Weight);
	PerViewData->Ghost5.Scale = FMath::Lerp(PerViewData->Ghost5.Scale, Ghost5.Scale, Weight);

	PerViewData->Ghost6.Color = FMath::Lerp(PerViewData->Ghost6.Color, Ghost6.Color, Weight);
	PerViewData->Ghost6.Scale = FMath::Lerp(PerViewData->Ghost6.Scale, Ghost6.Scale, Weight);

	PerViewData->Ghost7.Color = FMath::Lerp(PerViewData->Ghost7.Color, Ghost7.Color, Weight);
	PerViewData->Ghost7.Scale = FMath::Lerp(PerViewData->Ghost7.Scale, Ghost7.Scale, Weight);

	PerViewData->Ghost8.Color = FMath::Lerp(PerViewData->Ghost8.Color, Ghost8.Color, Weight);
	PerViewData->Ghost8.Scale = FMath::Lerp(PerViewData->Ghost8.Scale, Ghost8.Scale, Weight);

	PerViewData->HaloIntensity = FMath::Lerp(PerViewData->HaloIntensity, HaloIntensity, Weight);

	PerViewData->HaloWidth = FMath::Lerp(PerViewData->HaloWidth, HaloWidth, Weight);

	PerViewData->HaloMask = FMath::Lerp(PerViewData->HaloMask, HaloMask, Weight);

	PerViewData->HaloCompression = FMath::Lerp(PerViewData->HaloCompression, HaloCompression, Weight);

	PerViewData->HaloChromaShift = FMath::Lerp(PerViewData->HaloChromaShift, HaloChromaShift, Weight);

	PerViewData->GlareIntensity = FMath::Lerp(PerViewData->GlareIntensity, GlareIntensity, Weight);

	PerViewData->GlareDivider = FMath::Lerp(PerViewData->GlareDivider, GlareDivider, Weight);

	PerViewData->GlareScale = FMath::Lerp(PerViewData->GlareScale, GlareScale, Weight);

	PerViewData->GlareAngles = FMath::Lerp(PerViewData->GlareAngles, GlareAngles, Weight);

	PerViewData->GlareTint = FMath::Lerp(PerViewData->GlareTint, GlareTint, Weight);

	PerViewData->GlareLineMask = GlareLineMask; // todo: respect weight

	PerViewData->FlareTint = FMath::Lerp(PerViewData->FlareTint, FlareTint, Weight);

	PerViewData->FlareIntensity = FMath::Lerp(PerViewData->FlareIntensity, FlareIntensity, Weight);
}
