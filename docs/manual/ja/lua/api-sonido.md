# サウンド

| 関数 | 説明 |
|---|---|
| `beep(freq [, ms, vol])` | ビープ音（ファイルは不要です）。 |
| `sound("sounds/x.wav" [, vol, pitch, loop, pan]) → h` | サウンドを再生し、識別子を返します。 |
| `sound("sounds/x.wav", {vol, pitch, loop, pan, attack, decay, sustain, fall, release, duration, reverb}) → h` | 名前付きの同じ関数。エンベロープ（アタック、ディケイ、0〜1 のサステイン、`fall`：サステインがその秒数で 60 dB 下がる、リリース、`duration`：その秒数後にノートが自動で離される）と、リバーブに送る量（0〜1）を指定できます。 |
| `stopSound(h [, fundido])`、`stopSounds()`、`adjustSound(h, vol [, pitch])` | サウンドの停止や調整。 |
| `releaseSound(h)` | ノートを離します。エンベロープがリリースに移ります。 |
| `reverb(room, damping, level)` | 全体のリバーブ：部屋の大きさ、ダンピング、レベル（それぞれ 0〜1、レベル 0 でオフ）。 |
| `mute(bool)`、`isMuted()` | 全体のミュート。 |
| `playSound(obj [, seg])`、`muteSound(obj)` | **Sound オブジェクト**を最初から（区間の `seg` 秒から）鳴らし直す／止める。 |
| `soundObject(obj) → t`、`setSoundObject(obj, t)` | Sound オブジェクトを読む／変える：`file`（ソース）、`active`、`volume`、`pitch`、`reverb`、`pan`、`start`、`clipStart`、`clipEnd`、`loop`、`positional`、`radius`、`range`（読み取り時は `duration`、`playing` も）。 |
| `createSound("sounds/x.wav" [, parent]) → obj` | 新しい Sound オブジェクト（`destroy(obj)` で削除）。プレハブ内では他のオブジェクトと同様にインスタンス化されます。 |
