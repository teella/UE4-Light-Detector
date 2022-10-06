// Fill out your copyright notice in the Description page of Project Settings.


#include "LightDetector.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values for this component's properties
ULightDetector::ULightDetector()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 1.0;

	NextLightDectorUpdate = 0;

	LightUpdateInterval = 1.0f;
}

// Called every frame
void ULightDetector::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		CalculateBrightness();
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
		NextLightDectorUpdate = GetWorld()->GetTimeSeconds() + LightUpdateInterval;

		//CaptureSceneDeferred will capture the scene the next time it's render. So first time the scene will be black
		detectorTop->CaptureSceneDeferred();
		detectorBottom->CaptureSceneDeferred();

		// Reset our values for the next brightness test
		currentBrightness = 0;

		// Read the pixels from our RenderTexture and store the data into our color array
		// Note: ReadPixels is allegedly a very slow operation
		fRenderTarget = detectorTextureTop->GameThread_GetRenderTargetResource();
		fRenderTarget->ReadPixels(pixelStorage1);
		fRenderTarget = detectorTextureBottom->GameThread_GetRenderTargetResource();
		fRenderTarget->ReadPixels(pixelStorage2);

		ParallelFor(2, [this](int32 index)
			{
				if (index == 0)
					ProcessRenderTexture(pixelStorage1);
				else if (index == 1)
					ProcessRenderTexture(pixelStorage2);
			}, EParallelForFlags::BackgroundPriority);

		// this is to help smooth things out. so you don't flip flop between being bright or dark
		float currentDelta = UKismetMathLibrary::Abs(brightnessOutput - currentBrightness);

		if (UKismetMathLibrary::Abs(currentDelta - lastDelta) < 50)
		{
			brightnessOutput = currentBrightness;
		}
		lastDelta = currentDelta;

	}

	return brightnessOutput;
}
