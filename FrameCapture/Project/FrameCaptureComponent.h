
#pragma once

#include "CoreMinimal.h"
#include "FrameCaptureComponent.generated.h"

class UTextureRenderTarget2D;
class FRHICommandListImmediate;

// ※ CUSTOM4_23_API は書き換えてください
/**
 * 
 */
UCLASS(hidecategories=(Collision, Object, Physics, SceneComponent), ClassGroup=Rendering, editinlinenew, meta=(BlueprintSpawnableComponent))
class CUSTOM4_23_API UFrameCaptureComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()
	
	/** 
	 * OneShotでキャプチャーする
	 */
	UFUNCTION(BlueprintCallable,Category = "Rendering|FrameCapture")
	void CaptureScene();
	
	/** 毎フレームキャプチャーするか */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=FrameCapture)
	bool bCaptureEveryFrame;
	
	/** 描画先ターゲット */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=FrameCapture)
	UTextureRenderTarget2D* TextureTarget;

	
	
	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	//~ End UActorComponent Interface


	/**
	 * 3D描画終了タイミングでのコールバック
	 * @note RenderThread動作
	 */
	void OnRender3DFinish(FRHICommandListImmediate& RHICmdList, const FTexture2DRHIRef& SceneTexture, const FIntRect& ViewRect);
	
private:
	/*
	 * DelegateへのBinding処理
	 */
	void SetBinding(bool bEnable);
	
private:
	/// 1回のみキャプチャー処理
	bool bOneFrameCapture;
	/// 描画処理先
	UTextureRenderTarget2D* RenderTextureTarget;
	/// Delegateのハンドル
	FDelegateHandle OnRender3DFinishHandle;
	
	mutable FCriticalSection CriticalSectionFlag;
	mutable FCriticalSection CriticalSectionTarget;
};
