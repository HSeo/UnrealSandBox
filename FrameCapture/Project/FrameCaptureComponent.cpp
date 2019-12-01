
#include "FrameCaptureComponent.h"
#include "RendererInterface.h"
#include "Modules/ModuleManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderTargetPool.h"
#include "ScreenRendering.h"
#include "CommonRenderResources.h"
#include "RHICommandList.h"

DECLARE_GPU_STAT(FrameCapture);

UFrameCaptureComponent::UFrameCaptureComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer),
  bCaptureEveryFrame(true),
  TextureTarget(nullptr),
  bOneFrameCapture(false),
  RenderTextureTarget(nullptr),
  OnRender3DFinishHandle()
{
	bAutoActivate = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
}

/** 
 * Render the scene to the texture target immediately.  
 * This should not be used if bCaptureEveryFrame is enabled, or the scene capture will render redundantly. 
 */
void UFrameCaptureComponent::CaptureScene()
{
	{
		FScopeLock Lock(&CriticalSectionFlag);
		bOneFrameCapture = true;
	}
	SetBinding(true);
}


/**
 *
 */
void UFrameCaptureComponent::OnRegister()
{
	Super::OnRegister();
	SetBinding(bCaptureEveryFrame);
}

/**
 *
 */
void UFrameCaptureComponent::OnUnregister()
{
	Super::OnUnregister();
	SetBinding(false);
}

/**
 *
 */
void UFrameCaptureComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (RenderTextureTarget != TextureTarget)
	{
		FScopeLock Lock(&CriticalSectionTarget);
		RenderTextureTarget = TextureTarget;
	}
	// bCaptureEveryFrame変数監視の為（Setter使ったら良い）
	SetBinding(bCaptureEveryFrame || bOneFrameCapture);
}


/**
 * 3D描画終了タイミングでのコールバック
 * @note RenderThread動作
 */
void UFrameCaptureComponent::OnRender3DFinish(FRHICommandListImmediate& RHICmdList, const FTexture2DRHIRef& SceneTexture, const FIntRect& ViewRect)
{
	// see Source/Runtime/MovieSceneCapture/Private/FrameGrabber.cpp

	SCOPED_GPU_STAT(RHICmdList, FrameCapture);
	{
		FScopeLock Lock(&CriticalSectionTarget);
		
		if (IsValid(RenderTextureTarget))
		{
			// see FViewportSurfaceReader::ResolveRenderTarget
			
			IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>(TEXT("Renderer"));
		
			const FIntPoint TargetSize(RenderTextureTarget->SizeX, RenderTextureTarget->SizeY);

			FPooledRenderTargetDesc OutputDesc = FPooledRenderTargetDesc::Create2DDesc(
				TargetSize,
				RenderTextureTarget->GetFormat(),
				FClearValueBinding::None,
				RenderTextureTarget->SRGB ? TexCreate_SRGB : TexCreate_None,
				TexCreate_RenderTargetable,
				false);

			TRefCountPtr<IPooledRenderTarget> ResampleTexturePooledRenderTarget;
			GRenderTargetPool.FindFreeElement(RHICmdList, OutputDesc, ResampleTexturePooledRenderTarget, TEXT("ResampleTexture"));
			check(ResampleTexturePooledRenderTarget);

			const FSceneRenderTargetItem& DestRenderTarget = ResampleTexturePooledRenderTarget->GetRenderTargetItem();

			FTextureRenderTargetResource* TextureTargetResource = RenderTextureTarget->GetRenderTargetResource();
			FTextureRenderTarget2DResource* TextureTargetResource2D = TextureTargetResource ? static_cast<FTextureRenderTarget2DResource*>(TextureTargetResource) : nullptr;
			
			if (TextureTargetResource2D)
			{
				
				FRHIRenderPassInfo RPInfo(DestRenderTarget.TargetableTexture, ERenderTargetActions::Load_Store, TextureTargetResource2D->GetTextureRHI());
				RHICmdList.BeginRenderPass(RPInfo, TEXT("FrameCaptureResolveRenderTarget"));
				{
					RHICmdList.SetViewport(0, 0, 0.0f, TargetSize.X, TargetSize.Y, 1.0f);

					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
					GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

					const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;

					TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(FeatureLevel);
					TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
					TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	#if 0
					const bool bIsSourceBackBufferSameAsWindowSize = SceneTexture->GetSizeX() == WindowSize.X && SceneTexture->GetSizeY() == WindowSize.Y;
					const bool bIsSourceBackBufferSameAsTargetSize = TargetSize.X == SceneTexture->GetSizeX() && TargetSize.Y == SceneTexture->GetSizeY();

					if (bIsSourceBackBufferSameAsWindowSize || bIsSourceBackBufferSameAsTargetSize)
					{
						PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), SceneTexture);
					}
					else
					{
						PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SceneTexture);
					}
	#else
					// Widnow Size取ってないのでBilinear前提で
					PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SceneTexture);
	#endif

					FIntPoint ViewRectSize = ViewRect.Max - ViewRect.Min;
					RendererModule->DrawRectangle(
						RHICmdList,
						0, 0,										// Dest X, Y
						TargetSize.X, TargetSize.Y,					// Dest W, H
						ViewRect.Min.X, ViewRect.Min.Y,				// Source X, Y
						ViewRectSize.X, ViewRectSize.Y,				// Source W, H
						TargetSize,									// Dest buffer size
						FIntPoint(SceneTexture->GetSizeX(), SceneTexture->GetSizeY()),						// Source texture size
						*VertexShader,
						EDRF_Default);
				}
				RHICmdList.EndRenderPass();
			}
			
		}
	}
	
	{
		FScopeLock Lock(&CriticalSectionFlag);
		bOneFrameCapture = false;
	}
}

/*
 * DelegateへのBinding処理
 */
void UFrameCaptureComponent::SetBinding(bool bEnable)
{
#if 0
	// OnRender3DFinishHandleとDelegate内のBindingの状態が違う事があるっぽい
	// すでに同じ状態
	if (bEnable == OnRender3DFinishHandle.IsValid())
	{
		return;
	}
#endif
	
	IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>(TEXT("Renderer"));
	if (RendererModule)
	{
		IRendererModule::FOnRender3DFinishDelegate& Delegate = RendererModule->GetOnRender3DFinishDelegate();
		if (bEnable)
		{
			if (!Delegate.IsBoundToObject(this))
			{
				OnRender3DFinishHandle = Delegate.AddUObject(this, &UFrameCaptureComponent::OnRender3DFinish);
			}
		}
		else
		{
			if (Delegate.IsBoundToObject(this))
			{
				Delegate.Remove(OnRender3DFinishHandle);
				OnRender3DFinishHandle = FDelegateHandle();
			}
		}
	}

}

