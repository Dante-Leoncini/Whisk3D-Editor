# Whisk3D Editorマニュアル

**Whisk3D Editor**でできることを、パネル別・モード別にまとめています。レビュー用の草稿です。

1. [Whisk3Dとは](que-es-whisk3d.md)
2. [インターフェース](interfaz.md)
3. [3D Viewportでのナビゲーション](navegacion.md)
4. [3D Viewport](vista-3d.md)
   - [3D Viewportのメニューバー](vista-3d-barra.md)
   - [移動・回転・拡大縮小](transformar.md)
   - [Object Mode](modo-objeto.md)
     - [3Dメッシュ](objeto-malla.md)
       - [Screw](modificador-screw.md)
       - [Mirror](modificador-espejo.md)
       - [Array](modificador-array.md)
       - [Subdivision Surface](modificador-subdivision.md)
       - [Boolean](modificador-booleano.md)
       - [Culling](modificador-culling.md)
       - [Armature（モディファイアー）](modificador-esqueleto.md)
     - [カメラ](objeto-camara.md)
     - [ライト](objeto-luz.md)
     - [アーマチュア](objeto-esqueleto.md)
     - [サウンド](objeto-sonido.md)
     - [エンプティ](objeto-vacio.md)
     - [コンストレイント](restricciones.md)
   - [Edit Mode（メッシュ）](modo-edicion.md)
   - [アーマチュア](esqueletos.md)
   - [ペイントモード](pintura.md)
   - [3D Viewportのその他の機能](otras-funciones.md)
5. [UV Editor](editor-uv.md)
6. [Timeline](linea-de-tiempo.md)
7. [Outliner](outliner.md)
8. [Properties](propiedades.md)
   - [自作シェーダー](shaders.md)
9. [2D Editor](editor-2d.md)
10. [Script Editor（IDE）](editor-de-scripts.md)
11. [Console](consola.md)
12. [プロジェクト（.w3d）とファイル](proyecto.md)
13. [キーボードショートカット](atajos.md)

ゲームのプログラミングについては、[Whisk3DのLua](../lua/README.md)を参照してください。内部で何が起きているかは、CやC++でプログラミングする人向けの[Whisk3D Coreマニュアル](../core/README.md)にあります。

Whisk3D · エディターマニュアル（2026年10月）。ショートカットの表は、`tools/genmanual_atajos.py`によって`main/app/W3dAtajos.cpp`から生成されています。
