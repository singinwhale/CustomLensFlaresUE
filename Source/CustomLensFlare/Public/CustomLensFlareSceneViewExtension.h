// Copyright Manuel Wagner (singinwhale.com). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScreenPass.h"
#include "Runtime/Engine/Public/SceneViewExtension.h"
#include "CustomLensFlareConfig.h"
#include "CustomLensFlareSceneViewExtensionData.h"

struct FLensFlareInputs;

/**
 * 
 */
class CUSTOMLENSFLARE_API FCustomLensFlareSceneViewExtension final : public FSceneViewExtensionBase
{
public:
	FCustomLensFlareSceneViewExtension(const FAutoRegister& AutoRegister);
	~FCustomLensFlareSceneViewExtension();

	// - FSceneViewExtensionBase
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	// --

	void Initialize();

private:
	FScreenPassTexture HandleBloomFlaresHook(FRDGBuilder& GraphBuilder,const FViewInfo& View, FScreenPassTextureSlice SceneColor, const class FTextureDownsampleChain& DownsampleChain);
	void InitStates();
	static const FCustomLensFlareSceneViewExtensionData::FPerViewExtensionData* GetPerViewExtensionData(const FSceneView& View);

	FScreenPassTexture RenderThreshold(FRDGBuilder& GraphBuilder,
		FScreenPassTexture InputTexture,
		const FViewInfo& View);
	FScreenPassTexture RenderFlare(FRDGBuilder& GraphBuilder,
		FScreenPassTextureSlice& BloomTexture,
		const FViewInfo& View);
	FScreenPassTexture RenderGlare(FRDGBuilder& GraphBuilder,
		FScreenPassTextureSlice& BloomTexture,
		const FViewInfo& View);
	FScreenPassTexture RenderBlur(FRDGBuilder& GraphBuilder,
		FScreenPassTexture InputTexture,
		const FViewInfo& View,
		int BlurSteps);

	TStrongObjectPtr<UCustomLensFlareConfig> Config;


	// Cached blending and sampling states
	// which are re-used across render passes
	FRHIBlendState* ClearBlendState = nullptr;
	FRHIBlendState* AdditiveBlendState = nullptr;

	FRHISamplerState* BilinearClampSampler = nullptr;
	FRHISamplerState* BilinearBorderSampler = nullptr;
	FRHISamplerState* BilinearRepeatSampler = nullptr;
	FRHISamplerState* NearestRepeatSampler = nullptr;

	struct FBloomFlareProcess
	{
		FScreenPassTextureSlice RenderBloom(
			FRDGBuilder& GraphBuilder,
			const FViewInfo& View,
			const FScreenPassTextureSlice& SceneColor,
			int32 PassAmount
		);

		FScreenPassTextureSlice RenderDownsample(
			FRDGBuilder& GraphBuilder,
			const FString& PassName,
			const FViewInfo& View,
			FScreenPassTextureSlice InputTexture,
			const FIntRect& Viewport
		);

		FScreenPassTextureSlice RenderUpsampleCombine(
			FRDGBuilder& GraphBuilder,
			const FString& PassName,
			const FViewInfo& View,
			const FScreenPassTextureSlice& InputTexture,
			const FScreenPassTextureSlice& PreviousTexture,
			float Radius
		);

		FCustomLensFlareSceneViewExtension& OwningExtension;
		TArray< FScreenPassTextureSlice > MipMapsDownsample;
		TArray< FScreenPassTextureSlice > MipMapsUpsample;
	};
};
