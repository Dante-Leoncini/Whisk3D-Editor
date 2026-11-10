# ネットワークで遊ぶ（LAN、Wi-Fi、インターネット）

ネットワークのセッションには、**ホスト**（セッションを作成する人：プレイヤー0）と、**参加**する人（1、2…）がいます。すべてホストを経由します：プレイヤーはホストに送り、ホストがそれを配ります。同じネットワーク（同じLANや同じWi-Fi）にいる人同士は自動で見つかります。インターネット経由の場合は、ホストのアドレスを指定して参加する必要があります（ホスト側のルーターでポート47023が開いている必要もあります）。

| 関数 | 説明 |
|---|---|
| `redDisponible() → bool [, motivo]` | このゲームがネットワークを使えるかどうか。使えない場合はその理由：`"sin permiso"`（エディターがこのプロジェクトに許可を与えていない）または`"sin red"`（このプラットフォームにはまだネットワーク機能がない）。 |
| `redDispositivo() → texto` | このデバイスの名前（ほかの人がセッションを探すときに表示される名前）。エディターの**Settings**カードの*Device name*で変更できます。 |
| `redDireccion() → texto` | ローカルネットワークでのこの機器のIP（インターネット経由で参加するときや、手入力で参加するときに使います）。 |
| `redHostear(nombre [, maximo [, juego]]) → bool [, error]` | セッションを作成し、プレイヤーを待ちます（`maximo`はホストを含めた人数で、初期値は2）。`juego`は自分のゲームの名前です：同じゲームのセッションだけが表示されます。 |
| `redAbrir(bool)` | ホストを検索結果に表示してプレイヤーを受け付けるか（`true`）、受け付けないか（たとえばセッションが始まったとき）。 |
| `redBuscar([juego]) → bool` | ローカルネットワークでセッションの検索を始めます。もう一度呼ぶと一覧を**更新**します。 |
| `redPartidas() → { {nombre, dispositivo, jugadores, maximo, direccion}, … }` | 応答したセッション（検索中は自動で更新されます）。 |
| `redUnirse(direccion) → bool` | セッションに参加します：`redPartidas()`の`direccion`か、手入力したアドレス（`"192.168.0.10"`、`"x.x.x.x:puerto"`）。 |
| `redEstado() → estado, error` | `"nada"`、`"buscando"`、`"esperando"`（自分がホスト）、`"conectando"`、`"conectado"`、または`"error"`（理由付き）。 |
| `redYo() → id` | セッション内での自分の番号（0 = ホスト、セッションに入っていなければ-1）。 |
| `redJugadores() → { {id, nombre, yo}, … }` | セッションに参加している人（名前はそのデバイスの名前）。 |
| `redSilencio([id]) → segundos, limite` | そのプレイヤーから何も届かなくなってからの時間（`id`なしなら、最も長く途絶えているプレイヤー）。参加した側にはホストからしか届かないので、参加者にとってはホストの沈黙時間になります。`limite`（8秒）を過ぎると、いなくなったとみなされます：ホストはそのプレイヤーをセッションから外し、参加した側では`redEstado()`が`"error"`になります。 |
| `redEnviar(valor [, para [, confiable]]) → bool` | Luaの値（数値、テキスト、真偽値、テーブル。テーブルの入れ子も可）を、1人のプレイヤー（`para` = そのid）か全員（`nil`）に送ります。**信頼できる**送信（初期設定）：確実に、順番どおりに届きます。`false`にすると、失われても再送しません：毎フレーム送るもの（位置、ゲームパッドの入力）のように、次のものが代わりになる場合に使います。 |
| `redRecibir() → { {de, datos}, … }` | 前回から届いたものすべてを、順番どおりに返します。 |
| `redCerrar()` | セッションから抜けます（ホストの場合は、全員のセッションを閉じます）。エディターのStopでも閉じます。 |

## 許可

セッションを作成したり、探したり、参加したりするプロジェクトは、ユーザーが許可するまでネットワークに接続しません。プロジェクトを開いたとき（または、あとからネットワークを使い始めた場合はPlayしたとき）に、エディターがファイルごとに**1回だけ**確認し、その答えを覚えておきます。あとから**File**カードの*Network access*で変更できます。許可がなくてもゲームはそのまま動き、`redDisponible()`は`false, "sin permiso"`を返します：ネットワークプレイの選択肢の代わりにメッセージを表示してください。

## 例：ロビー

```lua
local JUEGO = "mi_juego"

function inicio()
  if not redDisponible() then info("sin red") return end
  if propiedad("anfitrion") then redHostear(redDispositivo(), 2, JUEGO) else redBuscar(JUEGO) end
end

function actualizar(dt)
  local estado = redEstado()
  if estado == "buscando" then
    local p = redPartidas()
    if p[1] then redUnirse(p[1].direccion) end
  end
  if teclaApretada("enter") then redEnviar({t = "listo"}) end
  for _, m in ipairs(redRecibir()) do
    if m.datos.t == "listo" then info("el jugador " .. m.de .. " esta listo") end
  end
end
```

## ヒント

- **計算は1か所で**：最もシンプルで、異なる機器同士（PCと電話など）でもいちばんうまく動くのは、ホストがゲームを計算し、ほかの人には表示すべきものを送る方法です。ほかの人はゲームパッドの入力だけを送ります。Dragon Ballのデモ（`dbz2w3d/lua/pelea.lua`）はこの方法です。
- 毎フレーム送るものは**信頼できない**送信で、一度だけ起こること（通知、「開始」）は**信頼できる**送信で。
- **ネットワークが詰まったとき**：対戦中に`redSilencio()`が0.数秒を超えたら、相手からの送信が止まっています（Wi-Fiが切れた、電話がバックグラウンドに移った）。ゲームを一時停止して知らせてください（「…を待っています」と、`limite`までの残り時間）。戻ってくれば元の場所から続きます。戻らなければ、`redEstado()`か`redJugadores()`でわかります。
- 送るのは必要なものだけに：数値と短いテキスト。1つのメッセージは60000バイトを超えられません。
- ブラウザーにはまだネットワーク機能がありません（UDPがないため）。Androidには`INTERNET`の権限が必要で（ビルドしたゲームには含まれています）、Symbianには`NetworkServices`のケーパビリティが必要です（どのアクセスポイントで接続するかを電話が尋ねます）。
