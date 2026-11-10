# ネットワーク

**w3dRed**は、ローカルネットワーク、Wi-Fi、インターネットを通じて、UDPでネットワークプレイを行います。1人がセッションを**ホスト**し（ポート47023）、ほかの人はローカルネットワークでそれを**探す**か、アドレスを指定して**参加**します。すべてホストを経由し、最大8人まで遊べます。メッセージは**信頼できる**もの（すべてが順番どおりに届く：「準備完了」「開始」）か、**信頼できない**もの（毎フレーム送るもので、失われても次のものが代わりになる）にできます。

```cpp
w3dRed::Hostear("mi_juego", "La partida de Dante", 2);
// または、相手側で：
w3dRed::Buscar("mi_juego");   // ... w3dRed::Partidas() ... w3dRed::Unirse(direccion);
w3dRed::Enviar(-1, datos, bytes, true);   // -1 = ほかの全員へ
```

| システム | ソケット |
|---|---|
| Windows | Winsock |
| Linux、macOS、Android | POSIX（インターフェースの一覧はAndroid 7以降） |
| Symbian | Open Cのソケット |
| Dreamcast | KallistiOSのソケット（ネットワークアダプター経由） |
| ブラウザー、PSP、PS Vita、3DS | 今のところネットワークなし：`Soportada()`は`false`を返します |

アプリケーションが許可しなければ（`SetPermitida`）、ゲームはネットワークを使いません。エディターは、ネットワークを使うプロジェクトを開くときに確認します。Luaからは：[ネットワークで遊ぶ](../lua/api-red.md)。
