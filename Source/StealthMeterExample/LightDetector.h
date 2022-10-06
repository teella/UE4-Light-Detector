// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "LightDetector.generated.h"


UCLASS(ClassGroup = (GothGirl), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class STEALTHMETEREXAMPLE_API ULightDetector : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	ULightDetector();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightDetector|Components|LightDetector")
	float LightUpdateInterval;

	// The Render Textures we will be passing into the CalculateBrightness() method
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightDetector|Components|LightDetector")
	TObjectPtr<UTextureRenderTarget2D> detectorTextureTop = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightDetector|Components|LightDetector")
	TObjectPtr <UTextureRenderTarget2D> detectorTextureBottom = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightDetector|Components|LightDetector")
	TObjectPtr <USceneCaptureComponent2D> detectorBottom = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightDetector|Components|LightDetector")
	TObjectPtr <USceneCaptureComponent2D> detectorTop = nullptr;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "LightDetection")
	float GetBrightness() { return brightnessOutput; }

private:
	FCriticalSection Mutex;
	float NextLightDectorUpdate;

	TObjectPtr<FRenderTarget> fRenderTarget = nullptr;
	TArray<FColor> pixelStorage1;
	TArray<FColor> pixelStorage2;

	float brightnessOutput{ 0 };
	float currentBrightness{ 0 };
	float lastDelta{ 0 };

	void ProcessRenderTexture(TArray<FColor> pixelStorage);
	float CalculateBrightness();
};
