# シーン：検索・生成・破棄

| 関数 | 説明 |
|---|---|
| `buscar(nombre [, raiz]) → obj \| nil` | Outlinerでの名前でオブジェクトを探します（または`raiz`の中だけを探します）。 |
| `seleccionados() → { obj, … }` | エディターで選択したオブジェクト（エディターのアドオンとして使う[ボタン](scripts-y-objetos.md#ボタン)用）。 |
| `nombre(obj) → texto` | オブジェクトの名前。 |
| `tipo(obj) → texto` | 種類：`"mesh"`、`"luz"`、`"camara"`、`"vacio"`、`"esqueleto"`、`"texto"`、`"imagen"`、`"boton"`、`"hitbox"`、`"prefab"`、`"rutina"`…（存在しない場合は`""`）。 |
| `instanciar(prefab [, x, y, z [, rotY [, padre]]]) → obj \| nil` | その位置にプレハブのコピーを作成します（rotYは度）。`"Libreria/Escena"`を渡すと、リンクしたライブラリのものを作成します。`padre`を指定すると、そのオブジェクトの子になります（位置と回転はそのオブジェクトからの相対値）。 |
| `destruir(obj) → bool` | フレームの終わりにオブジェクトを削除します。 |
| `importarW3D(ruta) → obj \| nil` | ゲームの実行中に、別の.w3dファイルをシーンに追加します。 |
| `cargar(obj)` / `descargar(obj)` / `cargaAuto(obj)` | レベルの一部を手動で読み込む・解放する（ストリーミング）、または自動に戻します。 |
| `cargado(obj) → bool` | その部分がすでに読み込まれているかどうか。 |
