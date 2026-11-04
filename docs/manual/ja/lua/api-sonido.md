# サウンド

| 関数 | 説明 |
|---|---|
| `beep(freq [, ms, vol])` | ビープ音（ファイルは不要です）。 |
| `sonido("sonidos/x.wav" [, vol, tono, loop, pan]) → h` | サウンドを再生し、識別子を返します。 |
| `sonido("sonidos/x.wav", {vol, tono, loop, pan, ataque, decaimiento, sostenido, caida, liberacion, duracion, reverb}) → h` | 名前付きの同じ関数。エンベロープ（アタック、ディケイ、0〜1 のサステイン、`caida`：サステインがその秒数で 60 dB 下がる、リリース、`duracion`：その秒数後にノートが自動で離される）と、リバーブに送る量（0〜1）を指定できます。 |
| `pararSonido(h [, fundido])`、`pararSonidos()`、`ajustarSonido(h, vol [, tono])` | サウンドの停止や調整。 |
| `soltarSonido(h)` | ノートを離します。エンベロープがリリースに移ります。 |
| `reverb(sala, amortiguacion, nivel)` | 全体のリバーブ：部屋の大きさ、ダンピング、レベル（それぞれ 0〜1、レベル 0 でオフ）。 |
| `silenciar(bool)`、`estaMudo()` | 全体のミュート。 |
| `tocarSonido(obj [, seg])`、`callarSonido(obj)` | **Sound オブジェクト**を最初から（区間の `seg` 秒から）鳴らし直す／止める。 |
| `sonidoObj(obj) → t`、`setSonidoObj(obj, t)` | Sound オブジェクトを読む／変える：`archivo`（ソース）、`activo`、`volumen`、`tono`、`reverb`、`paneo`、`inicio`、`recorteIni`、`recorteFin`、`bucle`、`posicional`、`radio`、`alcance`（読み取り時は `duracion`、`sonando` も）。 |
| `crearSonido("sonidos/x.wav" [, padre]) → obj` | 新しい Sound オブジェクト（`destruir(obj)` で削除）。プレハブ内では他のオブジェクトと同様にインスタンス化されます。 |
