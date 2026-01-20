
![Lens Flares Screenshot in Editor](screenshot.png)

Based on [Custom Lens-Flare Post-Process in Unreal Engine](https://www.froyok.fr/blog/2021-09-ue4-custom-lens-flare/)
by Froyok.

I took the liberty to change some things compared to the original implementation:
- the hook into the engine is not a multicast delegate to make it more clear who is handling the lens flares.
- there is no engine subsystem, instead there is a global scene view extension which is the standard way of extending the 
  renderer even though none of the overloads are used.
- I added the possibility to scale the individual leaves of the glare effect which allows for somthing similar to anamorphic lens flares.

# Installation

## Required Engine Changes

Patches are for Unreal 5.5.4

```diff
--- a/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessing.cpp
+++ b/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessing.cpp
@@ -173,8 +173,19 @@
        TEXT(" 3: enable only for view with texture visualized through Vis / VisualizeTexture command, to avoid debug clutter in other views.\n"),
        ECVF_RenderThreadSafe);
 #endif
+
+TAutoConsoleVariable<int32> CVarCustomBloomFlareMode(
+       TEXT("r.PostProcessing.CustomBloomFlareMode"),
+       1,
+       TEXT("If set to 1, use external Bloom/Lens-flares rendering if available"),
+       ECVF_Scalability | ECVF_RenderThreadSafe);
 }

+FLensFlaresHook BloomFlaresHook;
+// --
+
+
 #if WITH_EDITOR
 static void AddGBufferPicking(FRDGBuilder& GraphBuilder, const FViewInfo& View, const TRDGUniformBufferRef<FSceneTextureUniformParameters>& SceneTextures);
 #endif
@@ -1187,6 +1198,13 @@
                FRDGBufferRef SceneColorApplyParameters = nullptr;
                if (bBloomEnabled)
                {
+                       if ((CVarCustomBloomFlareMode.GetValueOnAnyThread() > 0) && BloomFlaresHook.IsBound())
+                       {
+                               Bloom = BloomFlaresHook.Execute(GraphBuilder, View, SceneColorSlice, SceneDownsampleChain);
+                       }
+                       else
+                       {
                        const FTextureDownsampleChain* LensFlareSceneDownsampleChain;

                        FTextureDownsampleChain BloomDownsampleChain;

```

```diff
--- a/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessing.h
+++ b/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessing.h
@@ -20,6 +20,13 @@
        struct FRasterResults;
 }

+
+DECLARE_DELEGATE_RetVal_FourParams( FScreenPassTexture, FLensFlaresHook, FRDGBuilder&, const FViewInfo&, FScreenPassTextureSlice, const class FTextureDownsampleChain&);
+// Delegate that you can bind to override the bloom rendering process in the engine and provide your own implementation.
+extern RENDERER_API FLensFlaresHook BloomFlaresHook;
+// --
+
 // Returns whether the full post process pipeline is enabled. Otherwise, the minimal set of operations are performed.
 bool IsPostProcessingEnabled(const FViewInfo& View);

```

## Ini Changes

Reference a settings data asset in your `DefaultEngine.ini`. There is one shipped with the project that you can put into your ini file. 

```ini
[CustomLensFlareSceneViewExtension]
ConfigPath=/CustomLensFlare/DA_LensFlaresConfig.DA_LensFlaresConfig
```
