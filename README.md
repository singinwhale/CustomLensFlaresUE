
## Required Engine Changes

Patches for Unreal 5.5.2

```diff
diff --git a/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessing.cpp b/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessing.cpp
index 3764634fb674..c66108ef3b05 100644
--- a/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessing.cpp
+++ b/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessing.cpp
@@ -1275,7 +1275,7 @@ void AddPostProcessingPasses(
 			{
 				const ELensFlareQuality LensFlareQuality = GetLensFlareQuality();
 				const uint32 LensFlareDownsampleStageIndex = static_cast<uint32>(ELensFlareQuality::MAX) - static_cast<uint32>(LensFlareQuality) - 1;
-				Bloom = AddLensFlaresPass(GraphBuilder, View, Bloom,
+				Bloom = AddLensFlaresPass(GraphBuilder, View, Bloom, HalfResSceneColor,
 					LensFlareSceneDownsampleChain->GetTexture(LensFlareDownsampleStageIndex),
 					LensFlareSceneDownsampleChain->GetFirstTexture());
 			}
@@ -2704,7 +2704,7 @@ void AddMobilePostProcessingPasses(FRDGBuilder& GraphBuilder, FScene* Scene, con
 			{
 				const ELensFlareQuality LensFlareQuality = GetLensFlareQuality();
 				const uint32 LensFlareDownsampleStageIndex = static_cast<uint32>(ELensFlareQuality::MAX) - static_cast<uint32>(LensFlareQuality) - 1;
-				BloomUpOutputs = AddLensFlaresPass(GraphBuilder, View, BloomUpOutputs,
+				BloomUpOutputs = AddLensFlaresPass(GraphBuilder, View, BloomUpOutputs, FScreenPassTextureSlice::CreateFromScreenPassTexture(GraphBuilder, PostProcessDownsample_Bloom[1]),
 					FScreenPassTextureSlice::CreateFromScreenPassTexture(GraphBuilder, PostProcessDownsample_Bloom[LensFlareDownsampleStageIndex]),
 					FScreenPassTextureSlice::CreateFromScreenPassTexture(GraphBuilder, PostProcessDownsample_Bloom[0]));
 			}

```

```diff
diff --git a/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessLensFlares.cpp b/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessLensFlares.cpp
index d9cb0875fb50..cd0c19ce1b3e 100644
--- a/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessLensFlares.cpp
+++ b/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessLensFlares.cpp
@@ -361,10 +361,12 @@ bool IsLensFlaresEnabled(const FViewInfo& View)
 		Settings.LensFlareIntensity > SMALL_NUMBER);
 }
 
+FLensFlaresHook LensFlaresHook;
 FScreenPassTexture AddLensFlaresPass(
 	FRDGBuilder& GraphBuilder,
 	const FViewInfo& View,
 	FScreenPassTexture Bloom,
+	FScreenPassTextureSlice HalfSceneColor,
 	FScreenPassTextureSlice QualitySceneDownsample,
 	FScreenPassTextureSlice DefaultSceneDownsample)
 {
@@ -416,5 +418,10 @@ FScreenPassTexture AddLensFlaresPass(
 		LensFlareInputs.bCompositeWithBloom = false;
 	}
 
+	if (LensFlaresHook.IsBound())
+	{
+		return LensFlaresHook.Execute(GraphBuilder, View, LensFlareInputs);
+	}
+
 	return AddLensFlaresPass(GraphBuilder, View, LensFlareInputs);
 }
\ No newline at end of file

```

```diff
diff --git a/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessLensFlares.h b/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessLensFlares.h
index 378b131960cf..d3049388024d 100644
--- a/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessLensFlares.h
+++ b/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessLensFlares.h
@@ -60,10 +60,14 @@ using FLensFlareOutputs = FScreenPassTexture;
 
 bool IsLensFlaresEnabled(const FViewInfo& View);
 
+DECLARE_DELEGATE_RetVal_ThreeParams( FScreenPassTexture, FLensFlaresHook, FRDGBuilder&, const FViewInfo&, const FLensFlareInputs&);
+extern RENDERER_API FLensFlaresHook LensFlaresHook;
+
 // Helper function which pulls inputs from the post process settings of the view.
 FScreenPassTexture AddLensFlaresPass(
 	FRDGBuilder& GraphBuilder,
 	const FViewInfo& View,
 	FScreenPassTexture Bloom,
+	FScreenPassTextureSlice HalfSceneColor,
 	FScreenPassTextureSlice QualitySceneDownsample,
 	FScreenPassTextureSlice DefaultSceneDownsample);
\ No newline at end of file

```