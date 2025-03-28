
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
diff --git a/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessLensFlares.cpp b/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessLensFlares.cpp
index f51614e4c892..6bcbd7602080 100644
--- a/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessLensFlares.cpp
+++ b/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessLensFlares.cpp
@@ -361,6 +361,8 @@ bool IsLensFlaresEnabled(const FViewInfo& View)
 		Settings.LensFlareIntensity > SMALL_NUMBER);
 }
 
+FLensFlaresHook LensFlaresHook;
+
 FScreenPassTexture AddLensFlaresPass(
 	FRDGBuilder& GraphBuilder,
 	const FViewInfo& View,
@@ -415,5 +417,10 @@ FScreenPassTexture AddLensFlaresPass(
 		LensFlareInputs.bCompositeWithBloom = false;
 	}
 
+	if (LensFlaresHook.IsBound())
+	{
+		return LensFlaresHook.Execute(GraphBuilder, View, Bloom, DefaultSceneDownsample);
+	}
+
 	return AddLensFlaresPass(GraphBuilder, View, LensFlareInputs);
 }
\ No newline at end of file
```

```diff
diff --git a/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessLensFlares.h b/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessLensFlares.h
index 378b131960cf..4dfad61ac66c 100644
--- a/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessLensFlares.h
+++ b/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessLensFlares.h
@@ -60,6 +60,9 @@ using FLensFlareOutputs = FScreenPassTexture;
 
 bool IsLensFlaresEnabled(const FViewInfo& View);
 
+DECLARE_DELEGATE_RetVal_FourParams( FScreenPassTexture, FLensFlaresHook, FRDGBuilder&, const FViewInfo&, FScreenPassTexture, FScreenPassTextureSlice);
+extern RENDERER_API FLensFlaresHook LensFlaresHook;
+
 // Helper function which pulls inputs from the post process settings of the view.
 FScreenPassTexture AddLensFlaresPass(
 	FRDGBuilder& GraphBuilder,
```

## Ini Changes

Reference a settings data asset in your `DefaultGame.ini`. There is one shipped with the project that you can put into your ini file. 

```ini
[CustomLensFlareSceneViewExtension]
ConfigPath=/CustomLensFlare/DA_LensFlaresConfig.DA_LensFlaresConfig
```