# ファイル

読み書きのAPIは1つだけです：`w3dFileSystem::ReadFileBytes`、`ReadTextFile`、`WriteTextFile`、`ReadFileRange`（ファイルの一部分だけを読みます。メッシュを分割して読み込むためのもの）、`ListDir`、`FileExists`。`fopen`を直接呼ぶコードはありません。

`ReadFileBytes`は次の順番で探します：

1. **リンクされたライブラリ**（`lib:<nombre>/...`のパス）。
2. **埋め込みpak：** 「Assets: Packed (protected)」でビルドしたゲームは、すべてのファイルを実行ファイルの中に持っています。
3. **マウントされたコンテナー：** 開いている.w3dです。コンテナーは**展開せずにマウントします**。テクスチャ、フォント、スクリプト、インターフェースは、誰もコードを変えずにzipの中から読み込まれます。
4. **APKのアセット**（Android）：相対パスは、NDKの`AAssetManager`でAPKから読みます。
5. **ディスク。**

パスは探す前に正規化されます（`.`や`..`を取り除きます）。`modelos/crash/../../texturas/sombra_blob.png`を指定した.mtlはどのインデックスにも見つからず、APKの中ではテクスチャが白く表示されていました。

| システム | リソース（`res/`） | 書き込み先 |
|---|---|---|
| Windows、Linux | 実行ファイルの隣（Linuxでは`<prefijo>/share/Whisk3d/res`も） | ユーザーのフォルダー。設定はSDLのフォルダー（`SDL_GetPrefPath`） |
| Android | APKの中 | `/storage/emulated/0`。エクスポートしたものはダウンロードフォルダー |
| ブラウザー | `.data`にまとめられ、`/res`にマウント | 設定は`localStorage`。保存するとファイルがダウンロードされます（[方法](../editor/proyecto.md)） |
| Symbian | 電話独自のパス | `E:/`（メモリーカード）または`C:/` |
| Dreamcast | 実行ファイルの中のromdisk（`/rd`） | `/ram`（電源を切ると消えます） |
| PSP | `EBOOT.PBP`の隣 | `ms0:/`（PSP Goでは`ef0:/`） |
| PS Vita | `.vpk`の中（`app0:res`） | `ux0:` |
| 3DS | `.3dsx`のromfs（`romfs:`） | SDカード（`sdmc:/`） |

このモジュールには、ほかに2つの部品があります：

- **W3dRecursos**は、重いデータ（テクスチャ、メッシュ、アニメーション）ごとに、誰が使っていて、いつ解放するかを管理します。すぐに必要ないものは、少しずつ、毎フレーム一部分ずつ読み込みます。
- **W3dConfig**は、設定を`clave=valor`の形で保存します。ブラウザーでは`localStorage`に、それ以外ではテキストファイルに保存します。動的メモリを確保しないので、Symbianでも同じように使えます。Luaからは：[保存される設定](../lua/api-configuracion.md)。
