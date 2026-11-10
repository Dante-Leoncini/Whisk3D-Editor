# ログ

printf形式の3つのレベルがあります：`w3dLogf`（情報）、`w3dLogfW`（警告）、`w3dLogfE`（エラー）。`g_w3dLogNivel`で実行時に絞り込めます（3 = すべて、0 = なし）。`W3D_DEV_LOG=0`でコンパイルすると、呼び出しそのものが実行ファイルから消えます。最後の数行はメモリにも残り、それがエディターの[Console](../editor/consola.md)に表示されます。

| システム | 出力先 |
|---|---|
| PC | `whisk3d.log`。各行にミリ秒単位のタイムスタンプと`[INFO]`、`[WARN]`、`[ERROR]`が付きます |
| Android | logcat。タグは`Whisk3D`（`adb logcat -s Whisk3D`） |
| ブラウザー | コンソール：レベルに応じて`console.log`、`console.warn`、`console.error` |
| Symbian | `E:\whisk3d.log` |
| Dreamcast | シリアルポート（dc-toolまたはFlycastのコンソール） |
| PSP | 標準出力（PPSSPP、psplink）と、`EBOOT.PBP`の隣の`whisk3d.log` |
| PS Vita | `sceClibPrintf`と`ux0:data/Whisk3D/whisk3d.log` |
| 3DS | `svcOutputDebugString`と`sdmc:/3ds/Whisk3D/whisk3d.log` |

これが重要な理由：実機のコンソールにはターミナルがなく、フリーズしたあとに残るのはカードのログだけです。そのため3DS、Vita、PSPでは、各行をその場で書き込みます。N95では逆でした：1行ごとにその場で書き込むと数十ミリ秒かかり、ゲームがカクついていました。そこで、情報は貯めて3 KBごとに書き出し、警告はすぐに書き出し、エラーは強制的にディスクへ書き込みます。Luaからは：[メッセージとデバッグ](../lua/api-mensajes-y-depuracion.md)。
