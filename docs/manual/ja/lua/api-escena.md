# シーン：検索・生成・破棄

| 関数 | 説明 |
|---|---|
| `find(name [, root]) → obj \| nil` | Outlinerでの名前でオブジェクトを探します（または`root`の中だけを探します）。 |
| `selected() → { obj, … }` | エディターで選択したオブジェクト（エディターのアドオンとして使う[ボタン](scripts-y-objetos.md#ボタン)用）。 |
| `getName(obj) → text` | オブジェクトの名前。 |
| `getType(obj) → text` | 種類：`"mesh"`、`"light"`、`"camera"`、`"empty"`、`"armature"`、`"text"`、`"image"`、`"button"`、`"hitbox"`、`"prefab"`、`"rutina"`…（存在しない場合は`""`）。 |
| `instantiate(prefab [, x, y, z [, rotY [, parent]]]) → obj \| nil` | その位置にプレハブのコピーを作成します（rotYは度）。`"Libreria/Escena"`を渡すと、リンクしたライブラリのものを作成します。`parent`を指定すると、そのオブジェクトの子になります（位置と回転はそのオブジェクトからの相対値）。 |
| `destroy(obj) → bool` | フレームの終わりにオブジェクトを削除します。 |
| `importW3D(path) → obj \| nil` | ゲームの実行中に、別の.w3dファイルをシーンに追加します。 |
| `streamLoad(obj)` / `streamUnload(obj)` / `streamAuto(obj)` | レベルの一部を手動で読み込む・解放する（ストリーミング）、または自動に戻します。 |
| `isLoaded(obj) → bool` | その部分がすでに読み込まれているかどうか。 |
