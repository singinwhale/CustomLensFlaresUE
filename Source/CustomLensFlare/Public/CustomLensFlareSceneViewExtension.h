// Copyright Manuel Wagner (singinwhale.com). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScreenPass.h"
#include "Runtime/Engine/Public/SceneViewExtension.h"
#include "CustomLensFlareConfig.h"

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
	FScreenPassTexture HandleLensFlaresHook(FRDGBuilder& GraphBuilder,const FViewInfo& View, FScreenPassTexture Bloom, FScreenPassTextureSlice HalfResColor);
	FScreenPassTexture HandleBloomFlaresHook(FRDGBuilder& GraphBuilder,const FViewInfo& View, FScreenPassTextureSlice SceneColor, const class FSceneDownsampleChain& DownsampleChain);
	void InitStates();
	void RenderLensFlare(FRDGBuilder& GraphBuilder, const FViewInfo& View,
	                     FScreenPassTexture BloomTexture,
	                     FScreenPassTextureSlice HalfSceneColor,
	                     FScreenPassTexture& Outputs);
	FScreenPassTexture RenderThreshold(FRDGBuilder& GraphBuilder,
		FScreenPassTexture InputTexture,
		const FViewInfo& View);
	FScreenPassTexture RenderFlare(FRDGBuilder& GraphBuilder,
		FScreenPassTexture& BloomTexture,
		const FViewInfo& View);
	FScreenPassTexture RenderGlare(FRDGBuilder& GraphBuilder,
		FScreenPassTexture& BloomTexture,
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
		FScreenPassTexture RenderBloom(
			FRDGBuilder& GraphBuilder,
			const FViewInfo& View,
			const FScreenPassTextureSlice& SceneColor,
			int32 PassAmount
		);

		FRDGTextureRef RenderDownsample(
			FRDGBuilder& GraphBuilder,
			const FString& PassName,
			const FViewInfo& View,
			FRDGTextureRef InputTexture,
			const FIntRect& Viewport
		);

		FRDGTextureRef RenderUpsampleCombine(
			FRDGBuilder& GraphBuilder,
			const FString& PassName,
			const FViewInfo& View,
			const FScreenPassTexture& InputTexture,
			const FScreenPassTexture& PreviousTexture,
			float Radius
		);

		FCustomLensFlareSceneViewExtension& OwningExtension;
		TArray< FScreenPassTexture > MipMapsDownsample;
		TArray< FScreenPassTexture > MipMapsUpsample;
	};
};
