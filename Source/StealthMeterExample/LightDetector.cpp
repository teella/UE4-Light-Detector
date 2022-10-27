// Fill out your copyright notice in the Description page of Project Settings.


#include "LightDetector.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values for this component's properties
ULightDetector::ULightDetector()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1;

	NextLightDectorUpdate = 0;

	LightUpdateInterval = 0.1f;
}

// Called every frame
void ULightDetector::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		if (!bReadPixelsStartedTop && !bReadPixelsStartedBottom)
		{
			CalculateBrightness();
		}
		else if (bReadPixelsStartedTop && ReadPixelFenceTop.IsFenceComplete())
		{
			//we always reset NextReadFenceBottomUpdate to 0 once done
			//this allows a 0.1 second between render requests and reading pixels
			//play nice with GPU
			if (NextReadFenceBottomUpdate <= 0)
			{
				NextReadFenceBottomUpdate = GetWorld()->GetTimeSeconds() + 0.1;
			}

			//render the bottom of the light detector mesh and queue a read pixel on render thread
			if (!bReadPixelsStartedBottom && NextReadFenceBottomUpdate < GetWorld()->GetTimeSeconds())
			{
				detectorBottom->CaptureSceneDeferred();
				ReadPixelsNonBlocking(detectorTextureBottom, pixelStorageBottom);
				ReadPixelFenceBottom.BeginFence();
				bReadPixelsStartedBottom = true;
			}
			else if (bReadPixelsStartedBottom && ReadPixelFenceBottom.IsFenceComplete())
			{
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
		// Reset our values for the next brightness test
		currentBrightness = 0;

		//CaptureSceneDeferred will capture the scene the next time it's render. So first time the scene will be black
		detectorTop->CaptureSceneDeferred();
		//ReadPixelsNonBlocking will queue a request on the render thread once the scene is done rendering
		ReadPixelsNonBlocking(detectorTextureTop, pixelStorageTop);
		//this fence will return IsFenceComplete as true once completed
		ReadPixelFenceTop.BeginFence();
		//fence will always return true before BeginFence is called. so need bool to stop this
		bReadPixelsStartedTop = true;
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
	bReadPixelsStartedTop = false;
	bReadPixelsStartedBottom = false;
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