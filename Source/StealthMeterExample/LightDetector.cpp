// Fill out your copyright notice in the Description page of Project Settings.


#include "LightDetector.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values for this component's properties
ULightDetector::ULightDetector()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.0;

	NextLightDectorUpdate = 0;

	LightUpdateInterval = 0.05f;

	bReadPixelsStarted.AddDefaulted(2);
	bCaptureStarted.AddDefaulted(2);
	ReadPixelFence.AddDefaulted(2);
	CaptureFence.AddDefaulted(2);
}

// Called every frame
void ULightDetector::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		if (!bReadPixelsStarted[CAPTURESIDE::TOP] && !bReadPixelsStarted[CAPTURESIDE::BOTTOM] && 
			!bCaptureStarted[CAPTURESIDE::TOP] && !bCaptureStarted[CAPTURESIDE::BOTTOM])
		{
			CalculateBrightness();
		}
		else if (bCaptureStarted[CAPTURESIDE::TOP] && CaptureFence[CAPTURESIDE::TOP].IsFenceComplete())
		{
			//we always reset NextReadFenceBottomUpdate to 0 once done
			//this allows a 0.1 second between render requests and reading pixels
			//play nice with GPU
			if (NextReadFenceBottomUpdate <= 0)
			{
				NextReadFenceBottomUpdate = GetWorld()->GetTimeSeconds() + 0.1;
			}

			if (NextReadFenceBottomUpdate < GetWorld()->GetTimeSeconds())
			{
				//reset TOP capture bool
				bCaptureStarted[CAPTURESIDE::TOP] = false;

				detectorBottom->CaptureSceneDeferred();
				CaptureFence[CAPTURESIDE::BOTTOM].BeginFence(); //lets us know when the capture is done
				bCaptureStarted[CAPTURESIDE::BOTTOM] = true;

				NextReadFenceBottomUpdate = 0;
			}
		}
		else if (bCaptureStarted[CAPTURESIDE::BOTTOM] && CaptureFence[CAPTURESIDE::BOTTOM].IsFenceComplete())
		{
			//we always reset NextReadFenceBottomUpdate to 0 once done
			//this allows a 0.1 second between render requests and reading pixels
			//play nice with GPU
			if (NextReadFenceBottomUpdate <= 0)
			{
				NextReadFenceBottomUpdate = GetWorld()->GetTimeSeconds() + 0.1;
			}

			if (NextReadFenceBottomUpdate < GetWorld()->GetTimeSeconds())
			{
				//reset BOTTOM capture bool
				bCaptureStarted[CAPTURESIDE::BOTTOM] = false;

				//ReadPixelsNonBlocking will queue a request on the render thread once the scene is done rendering
				ReadPixelsNonBlocking(detectorTextureTop, pixelStorageTop);
				//this fence will return IsFenceComplete as true once completed
				ReadPixelFence[CAPTURESIDE::TOP].BeginFence();
				//fence will always return true before BeginFence is called. so need bool to stop this
				bReadPixelsStarted[CAPTURESIDE::TOP] = true;

				NextReadFenceBottomUpdate = 0;
			}
		}
		else if (bReadPixelsStarted[CAPTURESIDE::TOP] && ReadPixelFence[CAPTURESIDE::TOP].IsFenceComplete())
		{
			//we always reset NextReadFenceBottomUpdate to 0 once done
			//this allows a 0.1 second between render requests and reading pixels
			//play nice with GPU
			if (NextReadFenceBottomUpdate <= 0)
			{
				NextReadFenceBottomUpdate = GetWorld()->GetTimeSeconds() + 0.1;
			}

			//render the bottom of the light detector mesh and queue a read pixel on render thread
			if (!bReadPixelsStarted[CAPTURESIDE::BOTTOM] && NextReadFenceBottomUpdate < GetWorld()->GetTimeSeconds())
			{
				ReadPixelsNonBlocking(detectorTextureBottom, pixelStorageBottom);
				ReadPixelFence[CAPTURESIDE::BOTTOM].BeginFence();
				bReadPixelsStarted[CAPTURESIDE::BOTTOM] = true;
			}
			else if (bReadPixelsStarted[CAPTURESIDE::BOTTOM] && ReadPixelFence[CAPTURESIDE::BOTTOM].IsFenceComplete())
			{
				// Reset our values for the next brightness test
				currentBrightness = 0;

				//now both top and bottom have been captured. Lets check the values and reset variables
				ProcessBrightness();
			}
		}
	}
}

void ULightDetector::ProcessRenderTexture(TArray<FColor> pixelStorage)
{
	ParallelFor(pixelStorage.Num(), [this, &pixelStorage](int32 pixelNum)
		{
			float brightness = ((pixelStorage[pixelNum].R + pixelStorage[pixelNum].G + pixelStorage[pixelNum].B) * 0.333);

			if (brightness > currentBrightness)
			{
				Mutex.Lock();
				if (brightness > currentBrightness) currentBrightness = brightness;
				Mutex.Unlock();
			}
		}, EParallelForFlags::BackgroundPriority);
}

float ULightDetector::CalculateBrightness() {

	if (detectorTextureTop == nullptr ||
		detectorTextureBottom == nullptr ||
		detectorBottom == nullptr ||
		detectorTop == nullptr) {
		return 0.0f;
	}

	if (NextLightDectorUpdate < GetWorld()->GetTimeSeconds())
	{
		//CaptureSceneDeferred will capture the scene the next time it's render. So first time the scene will be black
		detectorTop->CaptureSceneDeferred();
		CaptureFence[CAPTURESIDE::TOP].BeginFence(); //lets us know when the capture is done
		bCaptureStarted[CAPTURESIDE::TOP] = true;
	}

	return brightnessOutput;
}

void ULightDetector::ProcessBrightness()
{
	ParallelFor(2, [this](int32 index)
		{
			if (index == 0)
				ProcessRenderTexture(pixelStorageTop);
			else if (index == 1)
				ProcessRenderTexture(pixelStorageBottom);
		}, EParallelForFlags::BackgroundPriority);

	// this is to help smooth things out. so you don't flip flop between being bright or dark
	float currentDelta = UKismetMathLibrary::Abs(brightnessOutput - currentBrightness);

	if (UKismetMathLibrary::Abs(currentDelta - lastDelta) < 50)
	{
		brightnessOutput = currentBrightness;
	}
	lastDelta = currentDelta;

	//reset variables
	bReadPixelsStarted[CAPTURESIDE::TOP] = bReadPixelsStarted[CAPTURESIDE::BOTTOM] = false;
	bCaptureStarted[CAPTURESIDE::TOP] = bCaptureStarted[CAPTURESIDE::BOTTOM] = false;
	NextLightDectorUpdate = GetWorld()->GetTimeSeconds() + LightUpdateInterval;
	NextReadFenceBottomUpdate = 0;
}

void ULightDetector::ReadPixelsNonBlocking(UTextureRenderTarget2D* renderTarget, TArray<FColor>& OutImageData)
{
	if (renderTarget)
	{
		FTextureRenderTargetResource* RenderTargetRes = renderTarget->GameThread_GetRenderTargetResource();
		if (RenderTargetRes)
		{
			// Read the render target surface data back.	
			struct FReadSurfaceContext
			{
				FRenderTarget* SrcRenderTarget;
				TArray<FColor>* OutData;
				FIntRect Rect;
				FReadSurfaceDataFlags Flags;
			};

			OutImageData.Reset();
			FReadSurfaceContext Context =
			{
				RenderTargetRes,
				&OutImageData,
				FIntRect(0, 0, RenderTargetRes->GetSizeXY().X, RenderTargetRes->GetSizeXY().Y),
				FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX)
			};

			ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
				[Context](FRHICommandListImmediate& RHICmdList)
				{
					RHICmdList.ReadSurfaceData(
						Context.SrcRenderTarget->GetRenderTargetTexture(),
						Context.Rect,
						*Context.OutData,
						Context.Flags
					);
				});
		}
	}
}