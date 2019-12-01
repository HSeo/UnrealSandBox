3Dのレンダリング結果をコピーして持ってくる機能サンプルです。
資料説明はQiitaに有ります。


UE4.23.1

■Engine
diff.patch
Engine以下の変更差分パッチです

■Project
プロジェクト側のC++コードです。
CUSTOM4_23_API 指定のみ書き換えてください。
また、.Build.cs の PrivateDependencyModuleNames に下記モジュールの参照が必要です
- Renderer
- RenderCore
- RHI

