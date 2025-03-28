// Copyright Manuel Wagner (singinwhale.com). All Rights Reserved.

#include "CustomLensFlareSceneViewExtension.h"

#include "CustomLensFlare.h"
#include "CustomLensFlareConfig.h"
#include "SceneRendering.h"
#include "ScreenPass.h"
#include "PostProcess/PostProcessLensFlares.h"
#include "PostProcess/SceneFilterRendering.h"

TAutoConsoleVariable<int32> CVarLensFlareRenderBloom(
	TEXT("r.LensFlare.RenderBloom"),
	1,
	TEXT(" 0: Don't mix Bloom into lens-flare\n")
	TEXT(" 1: Mix the Bloom into the lens-flare"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarLensFlareRenderFlarePass(
	TEXT("r.LensFlare.RenderFlare"),
	1,
	TEXT(" 0: Don't render flare pass\n")
	TEXT(" 1: Render flare pass (ghosts and halos)"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarLensFlareRenderGlarePass(
	TEXT("r.LensFlare.RenderGlare"),
	1,
	TEXT(" 0: Don't render glare pass\n")
	TEXT(" 1: Render flare pass (star shape)"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarLensFlareEnabled(
	TEXT("r.LensFlare.Enabled"),
	1,
	TEXT(" 0: Don't render lens flares\n")
	TEXT(" 1: Render flares"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Value)
	{
		FCustomLensFlareModule& CustomLensFlareModule = FModuleManager::Get().GetModuleChecked<FCustomLensFlareModule>("CustomLensFlare");
		if (Value->GetBool())
		{
			CustomLensFlareModule.SetupCustomLensFlares();
		}
		else
		{
			CustomLensFlareModule.DestroyCustomLensFlares();
		}
		
	}),
	ECVF_RenderThreadSafe);

DECLARE_GPU_STAT(CustomLensFlares)

namespace
{
	// The function that draw a shader into a given RenderGraph texture
	template <typename TShaderParameters, typename TShaderClassVertex, typename TShaderClassPixel>
	void DrawShaderPass(
		FRDGBuilder& GraphBuilder,
		const FString& PassName,
		TShaderParameters* PassParameters,
		TShaderMapRef<TShaderClassVertex> VertexShader,
		TShaderMapRef<TShaderClassPixel> PixelShader,
		FRHIBlendState* BlendState,
		const FIntRect& Viewport
	)
	{
		const FScreenPassPipelineState PipelineState(VertexShader, PixelShader, BlendState);

		GraphBuilder.AddPass(
			FRDGEventName(TEXT("%s"), *PassName),
			PassParameters,
			ERDGPassFlags::Raster,
			[PixelShader, PassParameters, Viewport, PipelineState](FRHICommandListImmediate& RHICmdList)
			{
				RHICmdList.SetViewport(
					Viewport.Min.X, Viewport.Min.Y, 0.0f,
					Viewport.Max.X, Viewport.Max.Y, 1.0f
				);

				SetScreenPassPipelineState(RHICmdList, PipelineState);

				SetShaderParameters(
					RHICmdList,
					PixelShader,
					PixelShader.GetPixelShader(),
					*PassParameters
				);
				DrawRectangle(RHICmdList, // FRHICommandList
				              0.0f, 0.0f, // float X, float Y
				              Viewport.Width(), Viewport.Height(), // float SizeX, float SizeY
				              Viewport.Min.X, Viewport.Min.Y, // float U, float V
				              Viewport.Width(), // float SizeU
				              Viewport.Height(), // float SizeV
				              Viewport.Size(), // FIntPoint TargetSize
				              Viewport.Size(), // FIntPoint TextureSize
				              PipelineState.VertexShader, // const TShaderRefBase VertexShader
				              EDrawRectangleFlags::EDRF_UseTriangleOptimization // EDrawRectangleFlags Flags
				);
			});
	}

	// The function that draw a shader into a given RenderGraph texture
	// with the input texture having a different viewport than the target
	template <typename TShaderParameters, typename TShaderClassVertex, typename TShaderClassPixel>
	void DrawSplitResolutionPass(
		FRDGBuilder& GraphBuilder,
		const FString& PassName,
		TShaderParameters* PassParameters,
		TShaderMapRef<TShaderClassVertex> VertexShader,
		TShaderMapRef<TShaderClassPixel> PixelShader,
		FRHIBlendState* BlendState,
		const FIntRect& InputViewport,
		const FIntRect& OutputViewport
	)
	{
		const FScreenPassPipelineState PipelineState(VertexShader, PixelShader, BlendState);

		GraphBuilder.AddPass(
			FRDGEventName(TEXT("%s"), *PassName),
			PassParameters,
			ERDGPassFlags::Raster,
			[PixelShader, PassParameters, InputViewport, OutputViewport, PipelineState](
			FRHICommandListImmediate& RHICmdList)
			{
				RHICmdList.SetViewport(
					OutputViewport.Min.X, OutputViewport.Min.Y, 0.0f,
					OutputViewport.Max.X, OutputViewport.Max.Y, 1.0f
				);

				SetScreenPassPipelineState(RHICmdList, PipelineState);

				SetShaderParameters(
					RHICmdList,
					PixelShader,
					PixelShader.GetPixelShader(),
					*PassParameters
				);
				FRenderTargetBinding RenderTarget = PassParameters->Pass.RenderTargets[0];
				DrawRectangle(RHICmdList, // FRHICommandList
				              OutputViewport.Min.X, OutputViewport.Min.Y, // float X, float Y
				              OutputViewport.Width(), OutputViewport.Height(), // float SizeX, float SizeY
				              InputViewport.Min.X, InputViewport.Min.Y, // float U, float V
				              InputViewport.Width(), InputViewport.Height(), // float SizeU, float SizeV
				              OutputViewport.Size(), // FIntPoint TargetSize
				              InputViewport.Size(), // FIntPoint TextureSize
				              PipelineState.VertexShader, // const TShaderRefBase VertexShader
				              EDrawRectangleFlags::EDRF_UseTriangleOptimization // EDrawRectangleFlags Flags
				);
			});
	}

	FVector2f GetInputViewportSize(const FIntRect& Input, const FIntPoint& Extent)
	{
		// Based on
		// GetScreenPassTextureViewportParameters()
		// Engine/Source/Runtime/Renderer/Private/ScreenPass.cpp

		FVector2f ExtentInverse = FVector2f(1.0f / Extent.X, 1.0f / Extent.Y);

		FVector2f RectMin = FVector2f(Input.Min);
		FVector2f RectMax = FVector2f(Input.Max);

		FVector2f Min = RectMin * ExtentInverse;
		FVector2f Max = RectMax * ExtentInverse;

		return (Max - Min);
	}
}

namespace
{
	// RDG buffer input shared by all passes
	BEGIN_SHADER_PARAMETER_STRUCT(FCustomLensFlarePassParameters,)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	// The vertex shader to draw a rectangle.
	class FCustomScreenPassVS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FCustomScreenPassVS);

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters&)
		{
			return true;
		}

		FCustomScreenPassVS() = default;

		FCustomScreenPassVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FGlobalShader(Initializer)
		{
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FCustomScreenPassVS, "/Plugin/CustomLensFlare/ScreenPass.usf",
	                        "CustomLensFlareScreenPassVS", SF_Vertex);

	// Rescale shader
	class FLensFlareRescalePS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FLensFlareRescalePS);
		SHADER_USE_PARAMETER_STRUCT(FLensFlareRescalePS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
			SHADER_PARAMETER_STRUCT_INCLUDE(FCustomLensFlarePassParameters, Pass)
			SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
			SHADER_PARAMETER(FVector2f, InputViewportSize)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FLensFlareRescalePS, "/Plugin/CustomLensFlare/Rescale.usf", "RescalePS", SF_Pixel);

	// Downsample shader
	class FDownsamplePS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FDownsamplePS);
		SHADER_USE_PARAMETER_STRUCT(FDownsamplePS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
			SHADER_PARAMETER_STRUCT_INCLUDE(FCustomLensFlarePassParameters, Pass)
			SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
			SHADER_PARAMETER(FVector2f, InputSize)
			SHADER_PARAMETER(float, ThresholdLevel)
			SHADER_PARAMETER(float, ThresholdRange)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FDownsamplePS, "/Plugin/CustomLensFlare/DownsampleThreshold.usf", "DownsampleThresholdPS",
	                        SF_Pixel);

	// Blur shader (use Dual Kawase method)
	class FKawaseBlurDownPS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FKawaseBlurDownPS);
		SHADER_USE_PARAMETER_STRUCT(FKawaseBlurDownPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
			SHADER_PARAMETER_STRUCT_INCLUDE(FCustomLensFlarePassParameters, Pass)
			SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
			SHADER_PARAMETER(FVector2f, BufferSize)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};


	class FKawaseBlurUpPS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FKawaseBlurUpPS);
		SHADER_USE_PARAMETER_STRUCT(FKawaseBlurUpPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
			SHADER_PARAMETER_STRUCT_INCLUDE(FCustomLensFlarePassParameters, Pass)
			SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
			SHADER_PARAMETER(FVector2f, BufferSize)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FKawaseBlurDownPS, "/Plugin/CustomLensFlare/DualKawaseBlur.usf", "KawaseBlurDownsamplePS",
	                        SF_Pixel);
	IMPLEMENT_GLOBAL_SHADER(FKawaseBlurUpPS, "/Plugin/CustomLensFlare/DualKawaseBlur.usf", "KawaseBlurUpsamplePS",
	                        SF_Pixel);

	// Chromatic shift shader
	class FLensFlareChromaPS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FLensFlareChromaPS);
		SHADER_USE_PARAMETER_STRUCT(FLensFlareChromaPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
			SHADER_PARAMETER_STRUCT_INCLUDE(FCustomLensFlarePassParameters, Pass)
			SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
			SHADER_PARAMETER(float, ChromaShift)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FLensFlareChromaPS, "/Plugin/CustomLensFlare/Chroma.usf", "ChromaPS", SF_Pixel);

	// Ghost shader
	class FLensFlareGhostsPS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FLensFlareGhostsPS);
		SHADER_USE_PARAMETER_STRUCT(FLensFlareGhostsPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
			SHADER_PARAMETER_STRUCT_INCLUDE(FCustomLensFlarePassParameters, Pass)
			SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
			SHADER_PARAMETER_ARRAY(FVector4f, GhostColors, [8])
			SHADER_PARAMETER_SCALAR_ARRAY(float, GhostScales, [8])
			SHADER_PARAMETER(float, Intensity)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FLensFlareGhostsPS, "/Plugin/CustomLensFlare/Ghosts.usf", "GhostsPS", SF_Pixel);

	class FLensFlareHaloPS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FLensFlareHaloPS);
		SHADER_USE_PARAMETER_STRUCT(FLensFlareHaloPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
			SHADER_PARAMETER_STRUCT_INCLUDE(FCustomLensFlarePassParameters, Pass)
			SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
			SHADER_PARAMETER(float, Width)
			SHADER_PARAMETER(float, Mask)
			SHADER_PARAMETER(float, Compression)
			SHADER_PARAMETER(float, Intensity)
			SHADER_PARAMETER(float, ChromaShift)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FLensFlareHaloPS, "/Plugin/CustomLensFlare/Halo.usf", "HaloPS", SF_Pixel);

	// Glare shader pass
	class FLensFlareGlareVS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FLensFlareGlareVS);
		SHADER_USE_PARAMETER_STRUCT(FLensFlareGlareVS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
			SHADER_PARAMETER_STRUCT_INCLUDE(FCustomLensFlarePassParameters, Pass)
			SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
			SHADER_PARAMETER(FIntPoint, TileCount)
			SHADER_PARAMETER(FVector4f, PixelSize)
			SHADER_PARAMETER(FVector2f, BufferSize)
		END_SHADER_PARAMETER_STRUCT()
	};

	class FLensFlareGlareGS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FLensFlareGlareGS);
		SHADER_USE_PARAMETER_STRUCT(FLensFlareGlareGS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
			SHADER_PARAMETER(FVector4f, PixelSize)
			SHADER_PARAMETER(FVector2f, BufferSize)
			SHADER_PARAMETER(FVector2f, BufferRatio)
			SHADER_PARAMETER(float, GlareIntensity)
			SHADER_PARAMETER(float, GlareDivider)
			SHADER_PARAMETER(FVector4f, GlareTint)
			SHADER_PARAMETER_SCALAR_ARRAY(float, GlareScales, [3])
			SHADER_PARAMETER_SCALAR_ARRAY(float, GlareAngles, [3])
		END_SHADER_PARAMETER_STRUCT()
	};

	class FLensFlareGlarePS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FLensFlareGlarePS);
		SHADER_USE_PARAMETER_STRUCT(FLensFlareGlarePS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
			SHADER_PARAMETER_SAMPLER(SamplerState, GlareSampler)
			SHADER_PARAMETER_TEXTURE(Texture2D, GlareTexture)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FLensFlareGlareVS, "/Plugin/CustomLensFlare/Glare.usf", "GlareVS", SF_Vertex);
	IMPLEMENT_GLOBAL_SHADER(FLensFlareGlareGS, "/Plugin/CustomLensFlare/Glare.usf", "GlareGS", SF_Geometry);
	IMPLEMENT_GLOBAL_SHADER(FLensFlareGlarePS, "/Plugin/CustomLensFlare/Glare.usf", "GlarePS", SF_Pixel);

	// Final bloom mix shader
	class FLensFlareBloomMixPS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FLensFlareBloomMixPS);
		SHADER_USE_PARAMETER_STRUCT(FLensFlareBloomMixPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
			SHADER_PARAMETER_STRUCT_INCLUDE(FCustomLensFlarePassParameters, Pass)
			SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BloomTexture)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, GlareTexture)
			SHADER_PARAMETER_TEXTURE(Texture2D, GradientTexture)
			SHADER_PARAMETER_SAMPLER(SamplerState, GradientSampler)
			SHADER_PARAMETER(FVector4f, Tint)
			SHADER_PARAMETER(FVector2f, InputViewportSize)
			SHADER_PARAMETER(FVector2f, BufferSize)
			SHADER_PARAMETER(FVector2f, PixelSize)
			SHADER_PARAMETER(FIntVector, MixPass)
			SHADER_PARAMETER(float, Intensity)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FLensFlareBloomMixPS, "/Plugin/CustomLensFlare/Mix.usf", "MixPS", SF_Pixel);
}


FCustomLensFlareSceneViewExtension::FCustomLensFlareSceneViewExtension(const FAutoRegister& AutoRegister):
	FSceneViewExtensionBase(AutoRegister)
{
}

FCustomLensFlareSceneViewExtension::~FCustomLensFlareSceneViewExtension()
{
	if (LensFlaresHook.IsBoundToObject(this))
	{
		LensFlaresHook.Unbind();
	}
}

bool FCustomLensFlareSceneViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	return Config.IsValid();
}

void FCustomLensFlareSceneViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FCustomLensFlareSceneViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
}

void FCustomLensFlareSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FCustomLensFlareSceneViewExtension::Initialize()
{
	FString ConfigPath;
	if (GConfig->GetString(TEXT("CustomLensFlareSceneViewExtension"), TEXT("ConfigPath"), ConfigPath, GGameIni))
	{
		UCustomLensFlareConfig* LoadedConfig = LoadObject<UCustomLensFlareConfig>(nullptr, *ConfigPath);
		check(LoadedConfig);
		Config = TStrongObjectPtr(LoadedConfig);

		LensFlaresHook.BindRaw(this, &FCustomLensFlareSceneViewExtension::HandleLensFlaresHook);
	}
}

FScreenPassTexture FCustomLensFlareSceneViewExtension::HandleLensFlaresHook(
	FRDGBuilder& GraphBuilder, const FViewInfo& View, const FLensFlareInputs& Inputs)
{
	FScreenPassTexture Outputs;
	check(View.bIsViewInfo);
	RenderLensFlare(GraphBuilder,
	                View,
	                Inputs.Bloom,
	                Inputs.HalfSceneColor,
	                Outputs);
	return Outputs;
}

void FCustomLensFlareSceneViewExtension::RenderLensFlare(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FScreenPassTextureSlice BloomTexture,
	FScreenPassTextureSlice HalfSceneColor,
	FScreenPassTexture& Outputs
)
{
	if (!Config.IsValid())
	{
		return;
	}

	RDG_GPU_STAT_SCOPE(GraphBuilder, CustomLensFlares)
	RDG_EVENT_SCOPE(GraphBuilder, "CustomLensFlares");
	const FScreenPassTextureViewport BloomViewport(BloomTexture);
	const FVector2f BloomInputViewportSize = GetInputViewportSize(BloomViewport.Rect, BloomViewport.Extent);

	const FScreenPassTextureViewport SceneColorViewport(HalfSceneColor);
	const FVector2f SceneColorViewportSize = GetInputViewportSize(SceneColorViewport.Rect, SceneColorViewport.Extent);

	// Input
	FRDGTextureRef InputTexture = BloomTexture.TextureSRV->GetParent();
	FIntRect InputRect = SceneColorViewport.Rect;

	// Outputs
	FRDGTextureRef OutputTexture = HalfSceneColor.TextureSRV->GetParent();
	FIntRect OutputRect = SceneColorViewport.Rect;

	// States
	if (ClearBlendState == nullptr)
	{
		// Blend modes from:
		// '/Engine/Source/Runtime/RenderCore/Private/ClearQuad.cpp'
		// '/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessMaterial.cpp'
		ClearBlendState = TStaticBlendState<>::GetRHI();
		AdditiveBlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();

		BilinearClampSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		BilinearBorderSampler = TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI();
		BilinearRepeatSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		NearestRepeatSampler = TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	}

	if (SceneColorViewport.Rect.Width() != SceneColorViewport.Extent.X
		|| SceneColorViewport.Rect.Height() != SceneColorViewport.Extent.Y)
	{
		const FString PassName("LensFlareRescale");

		// Build target buffer
		FRDGTextureDesc Desc = HalfSceneColor.TextureSRV->GetParent()->Desc;
		Desc.Reset();
		Desc.Extent = SceneColorViewport.Rect.Size();
		Desc.Format = PF_FloatRGB;
		Desc.ClearValue = FClearValueBinding(FLinearColor::Transparent);
		FRDGTextureRef RescaleTexture = GraphBuilder.CreateTexture(Desc, *PassName);

		// Setup shaders
		TShaderMapRef<FCustomScreenPassVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FLensFlareRescalePS> PixelShader(View.ShaderMap);

		// Setup shader parameters
		FLensFlareRescalePS::FParameters* PassParameters = GraphBuilder.AllocParameters<
			FLensFlareRescalePS::FParameters>();
		PassParameters->Pass.InputTexture = HalfSceneColor.TextureSRV->GetParent();
		PassParameters->Pass.RenderTargets[0] =
			FRenderTargetBinding(RescaleTexture, ERenderTargetLoadAction::ENoAction);
		PassParameters->InputSampler = BilinearClampSampler;
		PassParameters->InputViewportSize = SceneColorViewportSize;

		// Render shader into buffer
		DrawShaderPass(
			GraphBuilder,
			PassName,
			PassParameters,
			VertexShader,
			PixelShader,
			ClearBlendState,
			SceneColorViewport.Rect
		);

		// Assign result before end of scope
		InputTexture = RescaleTexture;
	}


	////////////////////////////////////////////////////////////////////////
	// Render passes
	////////////////////////////////////////////////////////////////////////
	FRDGTextureRef ThresholdTexture = nullptr;
	FRDGTextureRef FlareTexture = nullptr;
	FRDGTextureRef GlareTexture = nullptr;

	ThresholdTexture = RenderThreshold(
		GraphBuilder,
		InputTexture,
		InputRect,
		View
	);

	if (CVarLensFlareRenderFlarePass.GetValueOnRenderThread())
	{
		FlareTexture = RenderFlare(
			GraphBuilder,
			ThresholdTexture,
			InputRect,
			View
		);
	}

	if (CVarLensFlareRenderGlarePass.GetValueOnRenderThread())
	{
		GlareTexture = RenderGlare(
			GraphBuilder,
			ThresholdTexture,
			InputRect,
			View
		);
	}


	{
		const FString PassName("LensFlareMix");

		FIntRect MixViewport = FIntRect(
			0,
			0,
			View.ViewRect.Width(),
			View.ViewRect.Height()
		);

		FVector2f BufferSize = FVector2f(MixViewport.Width(), MixViewport.Height());

		// Create buffer
		FRDGTextureDesc Description = BloomTexture.TextureSRV->GetParent()->Desc;
		Description.Reset();
		Description.Extent = MixViewport.Size();
		Description.Format = PF_FloatRGBA;
		Description.ClearValue = FClearValueBinding(FLinearColor::Transparent);
		FRDGTextureRef MixTexture = GraphBuilder.CreateTexture(Description, *PassName);

		// Shader parameters
		TShaderMapRef<FCustomScreenPassVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FLensFlareBloomMixPS> PixelShader(View.ShaderMap);

		FLensFlareBloomMixPS::FParameters* PassParameters = GraphBuilder.AllocParameters<
			FLensFlareBloomMixPS::FParameters>();
		PassParameters->Pass.RenderTargets[0] = FRenderTargetBinding(MixTexture, ERenderTargetLoadAction::ENoAction);
		PassParameters->InputSampler = BilinearClampSampler;
		PassParameters->GradientTexture = GWhiteTexture->TextureRHI;
		PassParameters->GradientSampler = BilinearClampSampler;
		PassParameters->BufferSize = BufferSize;
		PassParameters->PixelSize = FVector2f(1.0f, 1.0f) / BufferSize;
		PassParameters->InputViewportSize = BloomInputViewportSize;
		PassParameters->Tint = FVector4f(Config->Tint);
		PassParameters->Intensity = Config->Intensity;

		if (Config->Gradient != nullptr)
		{
			const FTextureRHIRef TextureRHI = Config->Gradient->GetResource()->TextureRHI;
			PassParameters->GradientTexture = TextureRHI;
		}

		// Plug in buffers
		const int32 MixBloomPass = CVarLensFlareRenderBloom.GetValueOnRenderThread();

		PassParameters->MixPass = FIntVector(
			(MixBloomPass),
			(FlareTexture != nullptr),
			(GlareTexture != nullptr)
		);

		PassParameters->BloomTexture = BloomTexture.TextureSRV->GetParent();

		if (FlareTexture != nullptr)
		{
			PassParameters->Pass.InputTexture = FlareTexture;
		}
		else
		{
			PassParameters->Pass.InputTexture = InputTexture;
		}

		if (GlareTexture != nullptr)
		{
			PassParameters->GlareTexture = GlareTexture;
		}
		else
		{
			PassParameters->GlareTexture = InputTexture;
		}

		// Render
		DrawShaderPass(
			GraphBuilder,
			PassName,
			PassParameters,
			VertexShader,
			PixelShader,
			ClearBlendState,
			MixViewport
		);

		OutputTexture = MixTexture;
		OutputRect = MixViewport;
	}

	////////////////////////////////////////////////////////////////////////
	// Final Output
	////////////////////////////////////////////////////////////////////////
	Outputs.Texture = OutputTexture;
	Outputs.ViewRect = OutputRect;
}

FRDGTextureRef FCustomLensFlareSceneViewExtension::RenderThreshold(FRDGBuilder& GraphBuilder,
                                                                   FRDGTextureRef InputTexture,
                                                                   FIntRect& InputRect,
                                                                   const FViewInfo& View)
{
	RDG_EVENT_SCOPE(GraphBuilder, "ThresholdPass");

	FRDGTextureRef OutputTexture = nullptr;

	FIntRect Viewport = View.ViewRect;
	FIntRect Viewport2 = InputRect;
	FIntRect Viewport4 = Viewport2 / 2;

	{
		const FString PassName("LensFlareDownsample");

		// Build texture
		FRDGTextureDesc Description = InputTexture->Desc;
		Description.Reset();
		Description.Extent = Viewport4.Size();
		Description.Format = PF_FloatRGB;
		Description.ClearValue = FClearValueBinding(FLinearColor::Black);
		FRDGTextureRef Texture = GraphBuilder.CreateTexture(Description, *PassName);

		// Render shader
		TShaderMapRef<FCustomScreenPassVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FDownsamplePS> PixelShader(View.ShaderMap);

		FDownsamplePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDownsamplePS::FParameters>();
		PassParameters->Pass.InputTexture = InputTexture;
		PassParameters->Pass.RenderTargets[0] = FRenderTargetBinding(Texture, ERenderTargetLoadAction::ENoAction);
		PassParameters->InputSampler = BilinearClampSampler;
		PassParameters->InputSize = FVector2f(Viewport2.Size());
		PassParameters->ThresholdLevel = Config->ThresholdLevel;
		PassParameters->ThresholdRange = Config->ThresholdRange;

		DrawSplitResolutionPass(
			GraphBuilder,
			PassName,
			PassParameters,
			VertexShader,
			PixelShader,
			ClearBlendState,
			Viewport2,
			Viewport4
		);

		OutputTexture = Texture;
	}

	return RenderBlur(
		GraphBuilder,
		OutputTexture,
		View,
		Viewport4,
		1
	);
}

FRDGTextureRef FCustomLensFlareSceneViewExtension::RenderFlare(FRDGBuilder& GraphBuilder, FRDGTextureRef InputTexture,
                                                               FIntRect& InputRect, const FViewInfo& View)
{
	RDG_EVENT_SCOPE(GraphBuilder, "FlarePass");

	FRDGTextureRef OutputTexture = nullptr;

	FIntRect Viewport = View.ViewRect;
	FIntRect Viewport2 = FIntRect(0, 0,
	                              View.ViewRect.Width() / 2,
	                              View.ViewRect.Height() / 2
	);
	FIntRect Viewport4 = FIntRect(0, 0,
	                              View.ViewRect.Width() / 4,
	                              View.ViewRect.Height() / 4
	);

	FRDGTextureRef ChromaTexture = nullptr;

	{
		const FString PassName("LensFlareChromaGhost");

		// Build buffer
		FRDGTextureDesc Description = InputTexture->Desc;
		Description.Reset();
		Description.Extent = Viewport2.Size();
		Description.Format = PF_FloatRGB;
		Description.ClearValue = FClearValueBinding(FLinearColor::Black);
		ChromaTexture = GraphBuilder.CreateTexture(Description, *PassName);

		// Shader parameters
		TShaderMapRef<FCustomScreenPassVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FLensFlareChromaPS> PixelShader(View.ShaderMap);

		FLensFlareChromaPS::FParameters* PassParameters = GraphBuilder.AllocParameters<
			FLensFlareChromaPS::FParameters>();
		PassParameters->Pass.InputTexture = InputTexture;
		PassParameters->Pass.RenderTargets[0] = FRenderTargetBinding(ChromaTexture, ERenderTargetLoadAction::ENoAction);
		PassParameters->InputSampler = BilinearBorderSampler;
		PassParameters->ChromaShift = Config->GhostChromaShift;

		// Render
		DrawShaderPass(
			GraphBuilder,
			PassName,
			PassParameters,
			VertexShader,
			PixelShader,
			ClearBlendState,
			Viewport2
		);
	}

	{
		const FString PassName("LensFlareGhosts");

		// Build buffer
		FRDGTextureDesc Description = InputTexture->Desc;
		Description.Reset();
		Description.Extent = Viewport2.Size();
		Description.Format = PF_FloatRGB;
		Description.ClearValue = FClearValueBinding(FLinearColor::Transparent);
		FRDGTextureRef Texture = GraphBuilder.CreateTexture(Description, *PassName);

		// Shader parameters
		TShaderMapRef<FCustomScreenPassVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FLensFlareGhostsPS> PixelShader(View.ShaderMap);

		FLensFlareGhostsPS::FParameters* PassParameters = GraphBuilder.AllocParameters<
			FLensFlareGhostsPS::FParameters>();
		PassParameters->Pass.InputTexture = ChromaTexture;
		PassParameters->Pass.RenderTargets[0] = FRenderTargetBinding(Texture, ERenderTargetLoadAction::ENoAction);
		PassParameters->InputSampler = BilinearBorderSampler;
		PassParameters->Intensity = Config->GhostIntensity;

		PassParameters->GhostColors[0] = Config->Ghost1.Color;
		PassParameters->GhostColors[1] = Config->Ghost2.Color;
		PassParameters->GhostColors[2] = Config->Ghost3.Color;
		PassParameters->GhostColors[3] = Config->Ghost4.Color;
		PassParameters->GhostColors[4] = Config->Ghost5.Color;
		PassParameters->GhostColors[5] = Config->Ghost6.Color;
		PassParameters->GhostColors[6] = Config->Ghost7.Color;
		PassParameters->GhostColors[7] = Config->Ghost8.Color;

		GET_SCALAR_ARRAY_ELEMENT(PassParameters->GhostScales, 0) = Config->Ghost1.Scale;
		GET_SCALAR_ARRAY_ELEMENT(PassParameters->GhostScales, 1) = Config->Ghost2.Scale;
		GET_SCALAR_ARRAY_ELEMENT(PassParameters->GhostScales, 2) = Config->Ghost3.Scale;
		GET_SCALAR_ARRAY_ELEMENT(PassParameters->GhostScales, 3) = Config->Ghost4.Scale;
		GET_SCALAR_ARRAY_ELEMENT(PassParameters->GhostScales, 4) = Config->Ghost5.Scale;
		GET_SCALAR_ARRAY_ELEMENT(PassParameters->GhostScales, 5) = Config->Ghost6.Scale;
		GET_SCALAR_ARRAY_ELEMENT(PassParameters->GhostScales, 6) = Config->Ghost7.Scale;
		GET_SCALAR_ARRAY_ELEMENT(PassParameters->GhostScales, 7) = Config->Ghost8.Scale;

		// Render
		DrawShaderPass(
			GraphBuilder,
			PassName,
			PassParameters,
			VertexShader,
			PixelShader,
			ClearBlendState,
			Viewport2
		);

		OutputTexture = Texture;
	}

	{
		// Render shader
		const FString PassName("LensFlareHalo");

		TShaderMapRef<FCustomScreenPassVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FLensFlareHaloPS> PixelShader(View.ShaderMap);

		FLensFlareHaloPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLensFlareHaloPS::FParameters>();
		PassParameters->Pass.InputTexture = InputTexture;
		PassParameters->Pass.RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ELoad);
		PassParameters->InputSampler = BilinearBorderSampler;
		PassParameters->Intensity = Config->HaloIntensity;
		PassParameters->Width = Config->HaloWidth;
		PassParameters->Mask = Config->HaloMask;
		PassParameters->Compression = Config->HaloCompression;
		PassParameters->ChromaShift = Config->HaloChromaShift;

		DrawShaderPass(
			GraphBuilder,
			PassName,
			PassParameters,
			VertexShader,
			PixelShader,
			AdditiveBlendState,
			Viewport2
		);
	}

	{
		OutputTexture = RenderBlur(
			GraphBuilder,
			OutputTexture,
			View,
			Viewport2,
			1
		);
	}

	return OutputTexture;
}

FRDGTextureRef FCustomLensFlareSceneViewExtension::RenderGlare(FRDGBuilder& GraphBuilder, FRDGTextureRef InputTexture,
                                                               FIntRect& InputRect, const FViewInfo& View)
{
	RDG_EVENT_SCOPE(GraphBuilder, "GlarePass");

	FRDGTextureRef OutputTexture = nullptr;

	FIntRect Viewport4 = FIntRect(
		0,
		0,
		View.ViewRect.Width() / 4,
		View.ViewRect.Height() / 4
	);
	// Only render the Glare if its intensity is different from 0
	if (Config->GlareIntensity > SMALL_NUMBER)
	{
		const FString PassName("LensFlareGlare");

		// This compute the number of point that will be drawn
		// Since we want one point for 2 by 2 pixel block we just 
		// need to divide the resolution by two to get this value.
		FIntPoint TileCount = Viewport4.Size();
		TileCount.X = TileCount.X / 2;
		TileCount.Y = TileCount.Y / 2;
		int32 Amount = TileCount.X * TileCount.Y;

		// Compute the ratio between the width and height
		// to know how to adjust the scaling of the quads.
		// (This assume width is bigger than height.)
		FVector2f BufferRatio = FVector2f(
			float(Viewport4.Height()) / float(Viewport4.Width()),
			1.0f
		);

		// Build the buffer
		FRDGTextureDesc Description = InputTexture->Desc;
		Description.Reset();
		Description.Extent = Viewport4.Size();
		Description.Format = PF_FloatRGB;
		Description.ClearValue = FClearValueBinding(FLinearColor::Transparent);
		FRDGTextureRef GlareTexture = GraphBuilder.CreateTexture(Description, *PassName);

		// Setup a few other variables that will 
		// be needed by the shaders.
		FVector4f PixelSize = FVector4f(0, 0, 0, 0);
		PixelSize.X = 1.0f / float(Viewport4.Width());
		PixelSize.Y = 1.0f / float(Viewport4.Height());
		PixelSize.Z = PixelSize.X;
		PixelSize.W = PixelSize.Y * -1.0f;

		FVector2f BufferSize = FVector2f(Description.Extent);

		// Setup shader
		FCustomLensFlarePassParameters* PassParameters = GraphBuilder.AllocParameters<FCustomLensFlarePassParameters>();
		PassParameters->InputTexture = InputTexture;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(GlareTexture, ERenderTargetLoadAction::EClear);

		// Vertex shader
		FLensFlareGlareVS::FParameters VertexParameters;
		VertexParameters.Pass = *PassParameters;
		VertexParameters.InputSampler = BilinearBorderSampler;
		VertexParameters.TileCount = TileCount;
		VertexParameters.PixelSize = PixelSize;
		VertexParameters.BufferSize = BufferSize;

		// Geometry shader
		FLensFlareGlareGS::FParameters GeometryParameters;
		GeometryParameters.BufferSize = BufferSize;
		GeometryParameters.BufferRatio = BufferRatio;
		GeometryParameters.PixelSize = PixelSize;
		GeometryParameters.GlareIntensity = Config->GlareIntensity;
		GeometryParameters.GlareTint = FVector4f(Config->GlareTint);
		GET_SCALAR_ARRAY_ELEMENT(GeometryParameters.GlareScales, 0) = Config->GlareScale.X;
		GET_SCALAR_ARRAY_ELEMENT(GeometryParameters.GlareScales, 1) = Config->GlareScale.Y;
		GET_SCALAR_ARRAY_ELEMENT(GeometryParameters.GlareScales, 2) = Config->GlareScale.Z;
		GET_SCALAR_ARRAY_ELEMENT(GeometryParameters.GlareAngles, 0) = Config->GlareAngles.X;
		GET_SCALAR_ARRAY_ELEMENT(GeometryParameters.GlareAngles, 1) = Config->GlareAngles.Y;
		GET_SCALAR_ARRAY_ELEMENT(GeometryParameters.GlareAngles, 2) = Config->GlareAngles.Z;
		GeometryParameters.GlareDivider = FMath::Max(Config->GlareDivider, 0.01f);

		// Pixel shader
		FLensFlareGlarePS::FParameters PixelParameters;
		PixelParameters.GlareSampler = BilinearClampSampler;
		PixelParameters.GlareTexture = GWhiteTexture->TextureRHI;

		if (Config->GlareLineMask != nullptr)
		{
			const FTextureRHIRef TextureRHI = Config->GlareLineMask->GetResource()->TextureRHI;
			PixelParameters.GlareTexture = TextureRHI;
		}

		TShaderMapRef<FLensFlareGlareVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FLensFlareGlareGS> GeometryShader(View.ShaderMap);
		TShaderMapRef<FLensFlareGlarePS> PixelShader(View.ShaderMap);
		// Required for Lambda capture
		FRHIBlendState* BlendState = this->AdditiveBlendState;

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("%s", *PassName),
			PassParameters,
			ERDGPassFlags::Raster,
			[
				VertexShader, VertexParameters,
				GeometryShader, GeometryParameters,
				PixelShader, PixelParameters,
				BlendState, Viewport4, Amount
			](FRHICommandListImmediate& RHICmdList)
			{
				RHICmdList.SetViewport(
					Viewport4.Min.X, Viewport4.Min.Y, 0.0f,
					Viewport4.Max.X, Viewport4.Max.Y, 1.0f
				);

				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.BlendState = BlendState;
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.SetGeometryShader(GeometryShader.GetGeometryShader());
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				GraphicsPSOInit.PrimitiveType = PT_PointList;
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), VertexParameters);
				SetShaderParameters(RHICmdList, GeometryShader, GeometryShader.GetGeometryShader(), GeometryParameters);
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PixelParameters);

				RHICmdList.SetStreamSource(0, nullptr, 0);
				RHICmdList.DrawPrimitive(0, 1, Amount);
			});

		OutputTexture = GlareTexture;
	} // End of if()

	return OutputTexture;
}

FRDGTextureRef FCustomLensFlareSceneViewExtension::RenderBlur(FRDGBuilder& GraphBuilder, FRDGTextureRef InputTexture,
                                                              const FViewInfo& View, const FIntRect& Viewport,
                                                              int BlurSteps)
{
	// Shader setup
	TShaderMapRef<FCustomScreenPassVS> VertexShader(View.ShaderMap);
	TShaderMapRef<FKawaseBlurDownPS> PixelShaderDown(View.ShaderMap);
	TShaderMapRef<FKawaseBlurUpPS> PixelShaderUp(View.ShaderMap);

	// Data setup
	FRDGTextureRef PreviousBuffer = InputTexture;
	const FRDGTextureDesc& InputDescription = InputTexture->Desc;

	const FString PassDownName = TEXT("Down");
	const FString PassUpName = TEXT("Up");
	const int32 ArraySize = BlurSteps * 2;

	// Viewport resolutions
	// Could have been a bit more clever and avoid duplicate
	// sizes for upscale passes but heh... it works.
	int32 Divider = 2;
	TArray<FIntRect> Viewports;
	for (int32 i = 0; i < ArraySize; i++)
	{
		FIntRect NewRect = FIntRect(
			0,
			0,
			Viewport.Width() / Divider,
			Viewport.Height() / Divider
		);

		Viewports.Add(NewRect);

		if (i < (BlurSteps - 1))
		{
			Divider *= 2;
		}
		else
		{
			Divider /= 2;
		}
	}

	// Render
	for (int32 i = 0; i < ArraySize; i++)
	{
		// Build texture
		FRDGTextureDesc BlurDesc = InputDescription;
		BlurDesc.Reset();
		BlurDesc.Extent = Viewports[i].Size();
		BlurDesc.Format = PF_FloatRGB;
		BlurDesc.NumMips = 1;
		BlurDesc.ClearValue = FClearValueBinding(FLinearColor::Transparent);

		FVector2f ViewportResolution = FVector2f(
			Viewports[i].Width(),
			Viewports[i].Height()
		);

		const FString PassName =
			FString("KawaseBlur")
			+ FString::Printf(TEXT("_%i_"), i)
			+ ((i < BlurSteps) ? PassDownName : PassUpName)
			+ FString::Printf(TEXT("_%ix%i"), Viewports[i].Width(), Viewports[i].Height());

		FRDGTextureRef Buffer = GraphBuilder.CreateTexture(BlurDesc, *PassName);

		// Render shader
		if (i < BlurSteps)
		{
			FKawaseBlurDownPS::FParameters* PassDownParameters = GraphBuilder.AllocParameters<
				FKawaseBlurDownPS::FParameters>();
			PassDownParameters->Pass.InputTexture = PreviousBuffer;
			PassDownParameters->Pass.RenderTargets[0] =
				FRenderTargetBinding(Buffer, ERenderTargetLoadAction::ENoAction);
			PassDownParameters->InputSampler = BilinearClampSampler;
			PassDownParameters->BufferSize = ViewportResolution;

			DrawSplitResolutionPass(
				GraphBuilder,
				PassName,
				PassDownParameters,
				VertexShader,
				PixelShaderDown,
				ClearBlendState,
				i == 0 ? Viewport : Viewports[i - 1],
				Viewports[i]
			);
		}
		else
		{
			FKawaseBlurUpPS::FParameters* PassUpParameters = GraphBuilder.AllocParameters<
				FKawaseBlurUpPS::FParameters>();
			PassUpParameters->Pass.InputTexture = PreviousBuffer;
			PassUpParameters->Pass.RenderTargets[0] = FRenderTargetBinding(Buffer, ERenderTargetLoadAction::ENoAction);
			PassUpParameters->InputSampler = BilinearClampSampler;
			PassUpParameters->BufferSize = ViewportResolution;

			DrawSplitResolutionPass(
				GraphBuilder,
				PassName,
				PassUpParameters,
				VertexShader,
				PixelShaderUp,
				ClearBlendState,
				Viewports[i - 1],
				Viewports[i]
			);
		}

		PreviousBuffer = Buffer;
	}

	return PreviousBuffer;
}
