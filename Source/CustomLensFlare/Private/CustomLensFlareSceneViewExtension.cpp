// Copyright Manuel Wagner (singinwhale.com). All Rights Reserved.

#include "CustomLensFlareSceneViewExtension.h"

#include "CustomLensFlare.h"
#include "CustomLensFlareConfig.h"
#include "SceneRendering.h"
#include "ScreenPass.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"

TAutoConsoleVariable<int32> CVarLensFlareRenderBloom(
	TEXT("r.LensFlare.RenderBloom"),
	1,
	TEXT(" 0: Don't mix Bloom into lens-flare\n")
	TEXT(" 1: Mix the Bloom into the lens-flare"),
	ECVF_RenderThreadSafe
	);

TAutoConsoleVariable<int32> CVarLensFlareRenderFlarePass(
	TEXT("r.LensFlare.RenderFlare"),
	1,
	TEXT(" 0: Don't render flare pass\n")
	TEXT(" 1: Render flare pass (ghosts and halos)"),
	ECVF_RenderThreadSafe
	);

TAutoConsoleVariable<int32> CVarLensFlareRenderGlarePass(
	TEXT("r.LensFlare.RenderGlare"),
	1,
	TEXT(" 0: Don't render glare pass\n")
	TEXT(" 1: Render flare pass (star shape)"),
	ECVF_RenderThreadSafe
	);

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
		}
		),
	ECVF_RenderThreadSafe
	);


TAutoConsoleVariable<int32> CVarMaxBloomPassAmount(
	TEXT("r.LensFlare.MaxBloomPassAmount"),
	8,
	TEXT("Max Number of passes to render bloom"),
	ECVF_RenderThreadSafe
	);

TAutoConsoleVariable<float> CVarMinDownsampleSize(
	TEXT("r.LensFlare.MinDownsampleSize"),
	1,
	TEXT("Smallest resolution we downsample to. Pixels for the smaller screen edge. Set < 1 to disable and use MaxBloomPassAmount."),
	ECVF_RenderThreadSafe
	);

TAutoConsoleVariable<float> CVarBloomRadius(
	TEXT("r.LensFlare.BloomRadius"),
	0.85,
	TEXT(" Size/Scale of the Bloom. this variable defines the weight of the blending between the previous pass and the current one when doing the upsamples. This is the \"internal blend\" value I mentioned a few times."),
	ECVF_RenderThreadSafe
	);

DECLARE_GPU_STAT(CustomLensFlares)
DECLARE_GPU_STAT(CustomBloomFlares)

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
			RDG_EVENT_NAME("%s", *PassName),
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
			}
			);
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
		const FScreenPassTextureSlice& InputTexture,
		const FIntRect& OutputViewport
		)
	{
		const FScreenPassPipelineState PipelineState(VertexShader, PixelShader, BlendState);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("%s", *PassName),
			PassParameters,
			ERDGPassFlags::Raster,
			[PixelShader, PassParameters, InputTexture, OutputViewport, PipelineState](
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
				FIntVector InputTextureSize = InputTexture.TextureSRV->GetParent()->Desc.GetSize();
				FIntRect InputViewport = InputTexture.ViewRect;
				DrawRectangle(RHICmdList, // FRHICommandList
					OutputViewport.Min.X, OutputViewport.Min.Y, // float X, float Y
					OutputViewport.Width(), OutputViewport.Height(), // float SizeX, float SizeY
					InputViewport.Min.X, InputViewport.Min.Y, // float U, float V
					InputViewport.Width(), InputViewport.Height(), // float SizeU, float SizeV
					OutputViewport.Size(), // FIntPoint TargetSize
					FIntPoint(InputTextureSize.X, InputTextureSize.Y), // FIntPoint TextureSize
					PipelineState.VertexShader, // const TShaderRefBase VertexShader
					EDrawRectangleFlags::EDRF_UseTriangleOptimization // EDrawRectangleFlags Flags
					);
			}
			);
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
		RENDER_TARGET_BINDING_SLOTS()
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
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
		"CustomLensFlareScreenPassVS", SF_Vertex
		);

	// Rescale shader
	class FLensFlareRescalePS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FLensFlareRescalePS);
		SHADER_USE_PARAMETER_STRUCT(FLensFlareRescalePS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
			RENDER_TARGET_BINDING_SLOTS()
			SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputTexture)
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
	class FDownsampleThresholdPS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FDownsampleThresholdPS);
		SHADER_USE_PARAMETER_STRUCT(FDownsampleThresholdPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
			SHADER_PARAMETER_STRUCT_INCLUDE(FCustomLensFlarePassParameters, Pass)
			SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
			SHADER_PARAMETER(FVector4f, InputSizeAndInvInputSize)
			SHADER_PARAMETER(float, ThresholdLevel)
			SHADER_PARAMETER(float, ThresholdRange)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FDownsampleThresholdPS, "/Plugin/CustomLensFlare/DownsampleThreshold.usf", "DownsampleThresholdPS", SF_Pixel);

	// Bloom downsample
	class FDownsamplePS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FDownsamplePS);
		SHADER_USE_PARAMETER_STRUCT(FDownsamplePS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
			RENDER_TARGET_BINDING_SLOTS()
			SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputTexture)
			SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
			SHADER_PARAMETER(FVector4f, InputSizeAndInvInputSize)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FDownsamplePS, "/Plugin/CustomLensFlare/DownsampleThreshold.usf", "DownsamplePS", SF_Pixel);

	// Bloom upsample + combine
	class FUpsampleCombinePS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FUpsampleCombinePS);
		SHADER_USE_PARAMETER_STRUCT(FUpsampleCombinePS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
			RENDER_TARGET_BINDING_SLOTS()
			SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputTexture)
			SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
			SHADER_PARAMETER(FVector4f, InputSizeAndInvInputSize)
			SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, PreviousTexture)
			SHADER_PARAMETER(FVector4f, PreviousSizeAndInvInputSize)
			SHADER_PARAMETER(float, Radius)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FUpsampleCombinePS, "/Plugin/CustomLensFlare/DownsampleThreshold.usf", "UpsampleCombinePS", SF_Pixel);

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
		SF_Pixel
		);
	IMPLEMENT_GLOBAL_SHADER(FKawaseBlurUpPS, "/Plugin/CustomLensFlare/DualKawaseBlur.usf", "KawaseBlurUpsamplePS",
		SF_Pixel
		);

	// Chromatic shift shader
	class FLensFlareChromaPS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FLensFlareChromaPS);
		SHADER_USE_PARAMETER_STRUCT(FLensFlareChromaPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
			RENDER_TARGET_BINDING_SLOTS()
			SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputTexture)
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
			RENDER_TARGET_BINDING_SLOTS()
			SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputTexture)
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
			RENDER_TARGET_BINDING_SLOTS()
			SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputTexture)
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
			SHADER_PARAMETER(FIntVector, MixPass)
			SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
			SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, BloomTexture)
			SHADER_PARAMETER(float, BloomIntensity)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, GlareTexture)
			SHADER_PARAMETER(FVector2f, GlarePixelSize)
			SHADER_PARAMETER(float, FlareIntensity)
			SHADER_PARAMETER(FVector4f, FlareTint)
			SHADER_PARAMETER_TEXTURE(Texture2D, FlareGradientTexture)
			SHADER_PARAMETER_SAMPLER(SamplerState, FlareGradientSampler)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FLensFlareBloomMixPS, "/Plugin/CustomLensFlare/Mix.usf", "MixPS", SF_Pixel);
	
	FVector4f SizeToSizeAndInvSize(FIntPoint PixelSize)
	{
		check(PixelSize.X != 0);
		check(PixelSize.Y != 0);
		return FVector4f(PixelSize.X, PixelSize.Y, 1.0 / PixelSize.X, 1.0 / PixelSize.Y);
	}
	
	FVector4f SizeToSizeAndInvSize(FIntVector PixelSize)
	{
		return SizeToSizeAndInvSize({PixelSize.X, PixelSize.Y});
	}
	
	FVector4f ViewToUVScaleAndPixelSize(FIntRect Viewport, FIntPoint FullSize)
	{
		return FVector4f(Viewport.Width() / FullSize.X, Viewport.Height() / FullSize.Y, 1.0 / FullSize.X, 1.0/FullSize.Y);
	}
}


FCustomLensFlareSceneViewExtension::FCustomLensFlareSceneViewExtension(const FAutoRegister& AutoRegister):
	FSceneViewExtensionBase(AutoRegister)
{
}

FCustomLensFlareSceneViewExtension::~FCustomLensFlareSceneViewExtension()
{
	if (BloomFlaresHook.IsBoundToObject(this))
	{
		BloomFlaresHook.Unbind();
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

		ENQUEUE_RENDER_COMMAND(BindBloomFlaresHook)([this](FRHICommandListImmediate&)
			{
				BloomFlaresHook.BindSP(this, &FCustomLensFlareSceneViewExtension::HandleBloomFlaresHook);
			}
			);
	}
}

FScreenPassTexture FCustomLensFlareSceneViewExtension::HandleBloomFlaresHook(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTextureSlice SceneColor, const class FSceneDownsampleChain& DownsampleChain)
{
	if (!SceneColor.IsValid())
		return {};
	
	RDG_GPU_STAT_SCOPE(GraphBuilder, CustomBloomFlares)
	RDG_EVENT_SCOPE(GraphBuilder, "CustomBloomFlares");

	InitStates();

	int32 PassAmount = CVarMaxBloomPassAmount.GetValueOnRenderThread();

	float MinDownsampleSize = CVarMinDownsampleSize->GetFloat();
	if (MinDownsampleSize >= 1.0f)
	{
		int32 MinScreenDim = SceneColor.ViewRect.Size().GetMin();
		int32 DesiredPassAmount = FMath::CeilLogTwo(MinScreenDim / MinDownsampleSize);
		PassAmount = FMath::Min(PassAmount, DesiredPassAmount);
	} 

	// Buffers setup
	const FScreenPassTexture BlackDummy{
		GraphBuilder.RegisterExternalTexture(
			GSystemTextures.BlackDummy,
			TEXT("BlackDummy")
			)
	};

	FScreenPassTextureSlice BloomTexture;
	FScreenPassTexture FlareTexture;
	FScreenPassTexture GlareTexture;
	FScreenPassTextureSlice InputTexture(SceneColor);

	// Scene color setup
	// We need to pass a FScreenPassTexture into FScreenPassTextureViewport()
	// and not a FRDGTextureRef (aka SceneColor.Texture) to ensure we can compute
	// the right Rect vs Extent sub-region. Otherwise only the full buffer
	// resolution is gonna be reported leading to NaNs/garbage in the rendering.
	const FScreenPassTextureViewport SceneColorViewport(SceneColor);
	const FVector2f SceneColorViewportSize = GetInputViewportSize(SceneColorViewport.Rect, SceneColorViewport.Extent);

	////////////////////////////////////////////////////////////////////////
	// Editor buffer rescale
	////////////////////////////////////////////////////////////////////////

	// Rescale the Scene Color to fit the whole texture and not use a sub-region.
	// This is to simplify the render pass (shaders) that come after
	if (SceneColorViewport.Rect.Width() != SceneColorViewport.Extent.X
		|| SceneColorViewport.Rect.Height() != SceneColorViewport.Extent.Y)
	{
		const FString SceneColorRescalePassName(TEXT("SceneColorRescale"));

		// Build texture
		FRDGTextureDesc Desc = SceneColor.TextureSRV->GetParent()->Desc;
		Desc.Reset();
		Desc.Extent = SceneColorViewport.Rect.Size();
		FRDGTextureRef RescaleTexture = GraphBuilder.CreateTexture(Desc, *SceneColorRescalePassName);
		AddCopyTexturePass(GraphBuilder, SceneColor.TextureSRV->GetParent(), RescaleTexture,
			SceneColor.ViewRect.Min, FIntPoint::ZeroValue, SceneColor.ViewRect.Size());

		InputTexture.TextureSRV = GraphBuilder.CreateSRV(RescaleTexture);
		InputTexture.ViewRect = SceneColorViewport.Rect;
	}

	////////////////////////////////////////////////////////////////////////
	// Render passes
	////////////////////////////////////////////////////////////////////////
	FBloomFlareProcess Process{.OwningExtension = *this};
	// Bloom
	{
		BloomTexture = Process.RenderBloom(
			GraphBuilder,
			View,
			InputTexture,
			PassAmount
			);
	}

	FlareTexture = RenderFlare(GraphBuilder, BloomTexture, View);
	GlareTexture = RenderGlare(GraphBuilder, BloomTexture, View);

	////////////////////////////////////////////////////////////////////////
	// Composite Bloom, Flare and Glare together
	////////////////////////////////////////////////////////////////////////
	FRDGTextureRef MixTexture = nullptr;
	FIntRect MixViewport{
		0,
		0,
		View.ViewRect.Width() / 2,
		View.ViewRect.Height() / 2
	};

	{
		RDG_EVENT_SCOPE(GraphBuilder, "MixPass");

		const FString MixPassName(TEXT("Mix"));

		float BloomIntensity = Config->Intensity;

		// If the internal blending for the upsample pass is additive
		// (aka not using the lerp) then uncomment this line to
		// normalize the final bloom intensity.
		//  BloomIntensity = 1.0f / float( FMath::Max( PassAmount, 1 ) );

		FVector2f BufferSize{
			float(MixViewport.Width()),
			float(MixViewport.Height())
		};

		FIntVector BuffersValidity{
			(BloomTexture.IsValid()),
			(FlareTexture.IsValid()),
			(GlareTexture.IsValid())
		};

		// Create texture
		FRDGTextureDesc Description = SceneColor.TextureSRV->GetParent()->Desc;
		Description.Reset();
		Description.Extent = MixViewport.Size();
		Description.Format = PF_FloatRGB;
		Description.ClearValue = FClearValueBinding(FLinearColor::Black);
		MixTexture = GraphBuilder.CreateTexture(Description, *MixPassName);

		// Render shader
		TShaderMapRef<FCustomScreenPassVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FLensFlareBloomMixPS> PixelShader(View.ShaderMap);

		FLensFlareBloomMixPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLensFlareBloomMixPS::FParameters>();
		PassParameters->Pass.RenderTargets[0] = FRenderTargetBinding(MixTexture, ERenderTargetLoadAction::ENoAction);
		PassParameters->InputSampler = BilinearClampSampler;
		PassParameters->MixPass = BuffersValidity;
		// Bloom
		PassParameters->BloomTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc(BlackDummy.Texture));
		PassParameters->BloomIntensity = BloomIntensity;

		// Glare
		PassParameters->GlareTexture = BlackDummy.Texture;
		PassParameters->GlarePixelSize = FVector2f(1.0f, 1.0f) / BufferSize;

		// Flare
		PassParameters->Pass.InputTexture = BlackDummy.Texture;
		PassParameters->FlareIntensity = Config->FlareIntensity;
		PassParameters->FlareTint = FVector4f(Config->FlareTint);
		PassParameters->FlareGradientTexture = GWhiteTexture->TextureRHI;
		PassParameters->FlareGradientSampler = BilinearClampSampler;

		if (Config->Gradient != nullptr)
		{
			const FTextureRHIRef TextureRHI = Config->Gradient->GetResource()->TextureRHI;
			PassParameters->FlareGradientTexture = TextureRHI;
		}

		if (BloomTexture.IsValid())
		{
			PassParameters->BloomTexture = BloomTexture.TextureSRV;
		}

		if (FlareTexture.IsValid())
		{
			PassParameters->Pass.InputTexture = FlareTexture.Texture;
		}

		if (GlareTexture.IsValid())
		{
			PassParameters->GlareTexture = GlareTexture.Texture;
		}

		// Render
		DrawShaderPass(
			GraphBuilder,
			MixPassName,
			PassParameters,
			VertexShader,
			PixelShader,
			ClearBlendState,
			MixViewport
			);
	} // end of mixing scope

	// Output
	return FScreenPassTexture(MixTexture, MixViewport);
}

void FCustomLensFlareSceneViewExtension::InitStates()
{
	if (ClearBlendState != nullptr)
		return;

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


FScreenPassTexture FCustomLensFlareSceneViewExtension::RenderThreshold(FRDGBuilder& GraphBuilder, FScreenPassTexture InputTexture, const FViewInfo& View)
{
	RDG_EVENT_SCOPE(GraphBuilder, "ThresholdPass");

	FScreenPassTexture OutputTexture = FScreenPassTexture();

	FIntRect Viewport = View.ViewRect;
	FIntRect Viewport2 = InputTexture.ViewRect;
	FIntRect Viewport4 = Viewport2 / 2;

	{
		const FString PassName(TEXT("LensFlareDownsample"));

		// Build texture
		FRDGTextureDesc Description = InputTexture.Texture->Desc;
		Description.Reset();
		Description.Extent = Viewport4.Size();
		Description.Format = PF_FloatRGB;
		Description.ClearValue = FClearValueBinding(FLinearColor::Black);
		FRDGTextureRef Texture = GraphBuilder.CreateTexture(Description, *PassName);

		// Render shader
		TShaderMapRef<FCustomScreenPassVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FDownsampleThresholdPS> PixelShader(View.ShaderMap);

		FDownsampleThresholdPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDownsampleThresholdPS::FParameters>();
		PassParameters->Pass.InputTexture = InputTexture.Texture;
		PassParameters->Pass.RenderTargets[0] = FRenderTargetBinding(Texture, ERenderTargetLoadAction::ENoAction);
		PassParameters->InputSampler = BilinearClampSampler;
		PassParameters->InputSizeAndInvInputSize = SizeToSizeAndInvSize(InputTexture.ViewRect.Size());
		PassParameters->ThresholdLevel = Config->ThresholdLevel;
		PassParameters->ThresholdRange = Config->ThresholdRange;

		DrawSplitResolutionPass(
			GraphBuilder,
			PassName,
			PassParameters,
			VertexShader,
			PixelShader,
			ClearBlendState,
			FScreenPassTextureSlice(GraphBuilder.CreateSRV(InputTexture.Texture), Viewport2),
			Viewport4
			);

		OutputTexture = FScreenPassTexture(Texture);
	}

	return RenderBlur(
		GraphBuilder,
		OutputTexture,
		View,
		1
		);
}

FScreenPassTexture FCustomLensFlareSceneViewExtension::RenderFlare(FRDGBuilder& GraphBuilder, FScreenPassTextureSlice& BloomTexture, const FViewInfo& View)
{
	RDG_EVENT_SCOPE(GraphBuilder, "FlarePass");

	FScreenPassTexture OutputTexture = FScreenPassTexture();

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
		const FString PassName(TEXT("LensFlareChromaGhost"));

		// Build buffer
		FRDGTextureDesc Description = BloomTexture.TextureSRV->GetParent()->Desc;
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
		PassParameters->InputTexture = BloomTexture.TextureSRV;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(ChromaTexture, ERenderTargetLoadAction::ENoAction);
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
		const FString PassName(TEXT("LensFlareGhosts"));

		// Build buffer
		FRDGTextureDesc Description = BloomTexture.TextureSRV->GetParent()->Desc;
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

		OutputTexture = FScreenPassTexture(Texture);
	}

	{
		// Render shader
		const FString PassName(TEXT("LensFlareHalo"));

		TShaderMapRef<FCustomScreenPassVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FLensFlareHaloPS> PixelShader(View.ShaderMap);

		FLensFlareHaloPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLensFlareHaloPS::FParameters>();
		PassParameters->InputTexture = BloomTexture.TextureSRV;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture.Texture, ERenderTargetLoadAction::ELoad);
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
			1
			);
	}

	return OutputTexture;
}

FScreenPassTexture FCustomLensFlareSceneViewExtension::RenderGlare(FRDGBuilder& GraphBuilder, FScreenPassTextureSlice& BloomTexture, const FViewInfo& View)
{
	RDG_EVENT_SCOPE(GraphBuilder, "GlarePass");

	FScreenPassTexture OutputTexture = FScreenPassTexture();

	FIntRect Viewport4 = FIntRect(
		0,
		0,
		View.ViewRect.Width() / 4,
		View.ViewRect.Height() / 4
		);
	// Only render the Glare if its intensity is different from 0
	if (Config->GlareIntensity > SMALL_NUMBER)
	{
		const FString LensFlareGlarePassName(TEXT("LensFlareGlare"));

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
		FRDGTextureDesc Description = BloomTexture.TextureSRV->GetParent()->Desc;
		Description.Reset();
		Description.Extent = Viewport4.Size();
		Description.Format = PF_FloatRGB;
		Description.ClearValue = FClearValueBinding(FLinearColor::Transparent);
		FRDGTextureRef GlareTexture = GraphBuilder.CreateTexture(Description, *LensFlareGlarePassName);

		// Setup a few other variables that will 
		// be needed by the shaders.
		FVector4f PixelSize = FVector4f(0, 0, 0, 0);
		PixelSize.X = 1.0f / float(Viewport4.Width());
		PixelSize.Y = 1.0f / float(Viewport4.Height());
		PixelSize.Z = PixelSize.X;
		PixelSize.W = PixelSize.Y * -1.0f;

		FVector2f BufferSize = FVector2f(Description.Extent);

		// Setup shader

		// Vertex shader
		FLensFlareGlareVS::FParameters* VertexParameters = GraphBuilder.AllocParameters<FLensFlareGlareVS::FParameters>();
		VertexParameters->InputTexture = BloomTexture.TextureSRV;
		VertexParameters->RenderTargets[0] = FRenderTargetBinding(GlareTexture, ERenderTargetLoadAction::EClear);
		VertexParameters->InputSampler = BilinearBorderSampler;
		VertexParameters->TileCount = TileCount;
		VertexParameters->PixelSize = PixelSize;
		VertexParameters->BufferSize = BufferSize;

		// Geometry shader
		FLensFlareGlareGS::FParameters* GeometryParameters = GraphBuilder.AllocParameters<FLensFlareGlareGS::FParameters>();
		GeometryParameters->BufferSize = BufferSize;
		GeometryParameters->BufferRatio = BufferRatio;
		GeometryParameters->PixelSize = PixelSize;
		GeometryParameters->GlareIntensity = Config->GlareIntensity;
		GeometryParameters->GlareTint = FVector4f(Config->GlareTint);
		GET_SCALAR_ARRAY_ELEMENT(GeometryParameters->GlareScales, 0) = Config->GlareScale.X;
		GET_SCALAR_ARRAY_ELEMENT(GeometryParameters->GlareScales, 1) = Config->GlareScale.Y;
		GET_SCALAR_ARRAY_ELEMENT(GeometryParameters->GlareScales, 2) = Config->GlareScale.Z;
		GET_SCALAR_ARRAY_ELEMENT(GeometryParameters->GlareAngles, 0) = Config->GlareAngles.X;
		GET_SCALAR_ARRAY_ELEMENT(GeometryParameters->GlareAngles, 1) = Config->GlareAngles.Y;
		GET_SCALAR_ARRAY_ELEMENT(GeometryParameters->GlareAngles, 2) = Config->GlareAngles.Z;
		GeometryParameters->GlareDivider = FMath::Max(Config->GlareDivider, 0.01f);

		// Pixel shader
		FLensFlareGlarePS::FParameters* PixelParameters = GraphBuilder.AllocParameters<FLensFlareGlarePS::FParameters>();
		PixelParameters->GlareSampler = BilinearClampSampler;
		PixelParameters->GlareTexture = GWhiteTexture->TextureRHI;

		if (Config->GlareLineMask != nullptr)
		{
			const FTextureRHIRef TextureRHI = Config->GlareLineMask->GetResource()->TextureRHI;
			PixelParameters->GlareTexture = TextureRHI;
		}

		TShaderMapRef<FLensFlareGlareVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FLensFlareGlareGS> GeometryShader(View.ShaderMap);
		TShaderMapRef<FLensFlareGlarePS> PixelShader(View.ShaderMap);
		// Required for Lambda capture
		FRHIBlendState* BlendState = this->AdditiveBlendState;

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("%s", *LensFlareGlarePassName),
			VertexParameters,
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

				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), *VertexParameters);
				SetShaderParameters(RHICmdList, GeometryShader, GeometryShader.GetGeometryShader(), *GeometryParameters);
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PixelParameters);

				RHICmdList.SetStreamSource(0, nullptr, 0);
				RHICmdList.DrawPrimitive(0, 1, Amount);
			}
			);

		OutputTexture = FScreenPassTexture(GlareTexture);
	} // End of if()

	return OutputTexture;
}

FScreenPassTexture FCustomLensFlareSceneViewExtension::RenderBlur(FRDGBuilder& GraphBuilder, FScreenPassTexture InputTexture,
	const FViewInfo& View, int BlurSteps)
{
	// Shader setup
	TShaderMapRef<FCustomScreenPassVS> VertexShader(View.ShaderMap);
	TShaderMapRef<FKawaseBlurDownPS> PixelShaderDown(View.ShaderMap);
	TShaderMapRef<FKawaseBlurUpPS> PixelShaderUp(View.ShaderMap);

	// Data setup
	FRDGTextureRef PreviousBuffer = InputTexture.Texture;
	const FRDGTextureDesc& InputDescription = InputTexture.Texture->Desc;

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
			InputTexture.ViewRect.Width() / Divider,
			InputTexture.ViewRect.Height() / Divider
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
			FString(TEXT("KawaseBlur"))
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
				FScreenPassTextureSlice(GraphBuilder.CreateSRV(PreviousBuffer), i == 0 ? InputTexture.ViewRect : Viewports[i - 1]),
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
				FScreenPassTextureSlice(GraphBuilder.CreateSRV(PreviousBuffer), Viewports[i - 1]),
				Viewports[i]
				);
		}

		PreviousBuffer = Buffer;
	}

	return FScreenPassTexture(PreviousBuffer);
}

FScreenPassTextureSlice FCustomLensFlareSceneViewExtension::FBloomFlareProcess::RenderBloom(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FScreenPassTextureSlice& SceneColor, int32 PassAmount)
{
	check(SceneColor.IsValid());

	if (PassAmount <= 1)
	{
		return {};
	}

	RDG_EVENT_SCOPE(GraphBuilder, "BloomPass");

	//----------------------------------------------------------
	// Downsample
	//----------------------------------------------------------
	int32 Width = SceneColor.ViewRect.Width();
	int32 Height = SceneColor.ViewRect.Height();
	int32 Divider = 1;
	FScreenPassTextureSlice PreviousTexture = SceneColor;

	for (int32 i = 0; i < PassAmount; i++)
	{
		FIntRect Size{
			0,
			0,
			FMath::Max(Width / Divider, 1),
			FMath::Max(Height / Divider, 1)
		};

		const FString PassName = "Downsample_"
			+ FString::FromInt(i)
			+ "_(1/"
			+ FString::FromInt(Divider*2)
			+ ")_"
			+ FString::FromInt(Size.Width())
			+ "x"
			+ FString::FromInt(Size.Height());

		FScreenPassTextureSlice Texture;

		// The SceneColor input is already downscaled by the engine
		// so we just reference it and continue.
		if (i == 0)
		{
			Texture = PreviousTexture;
		}
		else
		{
			Texture = RenderDownsample(
				GraphBuilder,
				PassName,
				View,
				PreviousTexture,
				Size
				);
		}

		MipMapsDownsample.Add(Texture);
		PreviousTexture = Texture;
		Divider *= 2;
	}

	//----------------------------------------------------------
	// Upsample
	//----------------------------------------------------------
	float Radius = CVarBloomRadius.GetValueOnRenderThread();

	// Copy downsamples into upsample so that
	// we can easily access current and previous
	// inputs during the upsample process
	MipMapsUpsample.Append(MipMapsDownsample);

	// Stars at -2 since we need the last buffer
	// as the previous input (-2) and the one just
	// before as the current input (-1).
	// We also go from end to start of array to
	// go from small to big texture (going back up the mips)
	for (int32 i = PassAmount - 2; i >= 0; i--)
	{
		FIntRect CurrentSize = MipMapsUpsample[i].ViewRect;

		const FString PassName = "UpsampleCombine_"
			+ FString::FromInt(i)
			+ "_"
			+ FString::FromInt(CurrentSize.Width())
			+ "x"
			+ FString::FromInt(CurrentSize.Height());

		FScreenPassTextureSlice ResultTexture = RenderUpsampleCombine(
			GraphBuilder,
			PassName,
			View,
			MipMapsUpsample[i], // Current texture
			MipMapsUpsample[i + 1], // Previous texture,
			Radius
			);
		
		MipMapsUpsample[i] = ResultTexture;
	}

	return MipMapsUpsample[0];
}

FScreenPassTextureSlice FCustomLensFlareSceneViewExtension::FBloomFlareProcess::RenderDownsample(FRDGBuilder& GraphBuilder, const FString& PassName, const FViewInfo& View, FScreenPassTextureSlice InputTexture, const FIntRect& Viewport)
{
	// Build texture
	FRDGTextureDesc Description = InputTexture.TextureSRV->GetParent()->Desc;
	Description.Reset();
	Description.Extent = Viewport.Size();
	Description.Format = PF_FloatRGB;
	Description.ClearValue = FClearValueBinding(FLinearColor::Black);
	FRDGTextureRef TargetTexture = GraphBuilder.CreateTexture(Description, *PassName);

	// Render shader
	TShaderMapRef<FCustomScreenPassVS> VertexShader(View.ShaderMap);
	TShaderMapRef<FDownsamplePS> PixelShader(View.ShaderMap);

	FDownsamplePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDownsamplePS::FParameters>();

	PassParameters->InputTexture = InputTexture.TextureSRV;
	PassParameters->RenderTargets[0] = FRenderTargetBinding(TargetTexture, ERenderTargetLoadAction::ENoAction);
	PassParameters->InputSampler = OwningExtension.BilinearBorderSampler;
	FIntVector ParentPixelSize = InputTexture.TextureSRV->GetParent()->Desc.GetSize();
	PassParameters->InputSizeAndInvInputSize = SizeToSizeAndInvSize(ParentPixelSize);

	DrawSplitResolutionPass(
		GraphBuilder,
		PassName,
		PassParameters,
		VertexShader,
		PixelShader,
		OwningExtension.ClearBlendState,
		InputTexture,
		Viewport
		);

	FScreenPassTextureSlice TargetTextureSlice(GraphBuilder.CreateSRV(FRDGTextureSRVDesc(TargetTexture)), FIntRect(FIntPoint::ZeroValue, Viewport.Size()));
	return TargetTextureSlice;
}

FScreenPassTextureSlice FCustomLensFlareSceneViewExtension::FBloomFlareProcess::RenderUpsampleCombine(FRDGBuilder& GraphBuilder, const FString& PassName, const FViewInfo& View, const FScreenPassTextureSlice& InputTexture, const FScreenPassTextureSlice& PreviousTexture, float Radius)
{
	// Build texture
	FRDGTextureDesc Description = InputTexture.TextureSRV->GetParent()->Desc;
	Description.Reset();
	Description.Extent = InputTexture.ViewRect.Size();
	Description.Format = PF_FloatRGB;
	Description.ClearValue = FClearValueBinding(FLinearColor::Black);
	FRDGTextureRef TargetTexture = GraphBuilder.CreateTexture(Description, *PassName);

	TShaderMapRef<FCustomScreenPassVS> VertexShader(View.ShaderMap);
	TShaderMapRef<FUpsampleCombinePS> PixelShader(View.ShaderMap);

	FUpsampleCombinePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FUpsampleCombinePS::FParameters>();

	PassParameters->InputTexture = InputTexture.TextureSRV;
	PassParameters->RenderTargets[0] = FRenderTargetBinding(TargetTexture, ERenderTargetLoadAction::ENoAction);
	PassParameters->InputSampler = OwningExtension.BilinearClampSampler;
	FIntVector InputTextureSize = InputTexture.TextureSRV->GetParent()->Desc.GetSize();
	PassParameters->InputSizeAndInvInputSize = ViewToUVScaleAndPixelSize(InputTexture.ViewRect, {InputTextureSize.X, InputTextureSize.Y});
	PassParameters->PreviousTexture = PreviousTexture.TextureSRV;
	FIntVector PreviousTextureSize = PreviousTexture.TextureSRV->GetParent()->Desc.GetSize();
	PassParameters->PreviousSizeAndInvInputSize = ViewToUVScaleAndPixelSize(PreviousTexture.ViewRect, {PreviousTextureSize.X, PreviousTextureSize.Y});
	PassParameters->Radius = Radius;

	DrawSplitResolutionPass(
		GraphBuilder,
		PassName,
		PassParameters,
		VertexShader,
		PixelShader,
		OwningExtension.ClearBlendState,
		InputTexture,
		InputTexture.ViewRect
		);

	FScreenPassTextureSlice TargetTextureSlice(GraphBuilder.CreateSRV(FRDGTextureSRVDesc(TargetTexture)), FIntRect(FIntPoint::ZeroValue, InputTexture.ViewRect.Size()));
	return TargetTextureSlice;
}
