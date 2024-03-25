// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/CircularQueue.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "LightDetector.generated.h"

//***********************************************************
//						Thread Worker
// Reference: https://unrealcommunity.wiki/multi-threading:-how-to-create-threads-in-ue4-0bsy2g96
//***********************************************************
struct FPixelCircularQueueData
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RamaCode)
	TArray<FColor> topPixelStorage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RamaCode)
	TArray<FColor> bottomPixelStorage;

	bool IgnoreBlueColor;
	float MinimumLightValue;
};

class ULightDetector;

class FLightDetectorWorker : public FRunnable
{
public:
	//Constructor / Destructor
	FLightDetectorWorker(ULightDetector *detector);
	virtual ~FLightDetectorWorker();

	/** Singleton instance, can access the thread any time via static accessor, if it is active! */
	static  FLightDetectorWorker* Runnable;

	FRunnableThread* Thread;

	/** Stop this thread? Uses Thread Safe Counter */
	FThreadSafeCounter StopTaskCounter;

	bool IsFinished() const
	{
		return _finished;
	}
	
	float ProcessRenderTexture(TArray<FColor> pixelStorage, bool IgnoreBlueColor, float MinimumLightValue);

	// Begin FRunnable interface.
	virtual bool Init();
	virtual uint32 Run();
	virtual void Stop();
	// End FRunnable interface

	/** Makes sure this thread has stopped properly */
	void EnsureCompletion();

	//~~~ Starting and Stopping Thread ~~~

	/*
		Start the thread and the worker from static (easy access)!
		This code ensures only 1 Prime Number thread will be able to run at a time.
		This function returns a handle to the newly started instance.
	*/
	static FLightDetectorWorker* ThreadedWorkerInit(ULightDetector* detector);

	/** Shuts down the thread. Static so it can easily be called from outside the thread context */
	static void Shutdown();

	static bool IsThreadFinished();

	TCircularQueue<FPixelCircularQueueData> Request;

private:
	bool _finished{ false };

	TArray<FPixelCircularQueueData> RequestData;

	TObjectPtr<ULightDetector> LightDetectorInstance = nullptr;
};
//***********************************************************
//						Thread Worker
//***********************************************************


UCLASS(ClassGroup = (GothGirl), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class STEALTHMETEREXAMPLE_API ULightDetector : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	ULightDetector();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightDetector|Components|LightDetector")
	bool IgnoreBlueColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightDetector|Components|LightDetector")
	float LightUpdateInterval;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightDetector|Components|LightDetector")
	float MinimumLightValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightDetector|Components|LightDetector")
	int MaxLightHistory;

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

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "LightDetection")
	void StartThreadWorker();

	UFUNCTION(BlueprintPure, Category = "LightDetection")
	float GetBrightness() { return brightnessOutput; }

	void AddToLightHistory(float TopTotal, float BottomTotal);

private:
	float NextLightDectorUpdate{ 0 };

	TArray<FColor> pixelStorageTop;
	TArray<FColor> pixelStorageBottom;

	float brightnessOutput{ 0 };

	float ProcessRenderTexture(TArray<FColor> pixelStorage);
	float CalculateBrightness();

	enum CAPTURESIDE
	{
		TOP = 0,
		BOTTOM
	};

	void ProcessBrightness();
	void ReadPixelsNonBlocking(UTextureRenderTarget2D* renderTarget, TArray<FColor>& OutImageData);
	TArray<bool>bReadPixelsStarted;
	TArray<bool>bCaptureStarted;
	TArray<FRenderCommandFence> ReadPixelFence;
	TArray<FRenderCommandFence> CaptureFence;
	float NextReadFenceBottomUpdate{ 0 };

	TArray<float> lightHistory;
	int currentHistoryIndex;

	FLightDetectorWorker* workerThread{ NULL };

	//this is added to stop an an occasional crash in 5.3 and 5.4 getting Lumen Scene Data
	bool FirstTimeRun{ true };
};
