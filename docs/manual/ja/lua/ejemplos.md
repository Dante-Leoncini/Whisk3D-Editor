# 完全なサンプル

## Hello World：方向キーで2Dの四角を動かす

```lua
properties = { frame = "object" }

function start()
  beep(440, 120, 0.5)          -- 起動したことがわかるようにビープ音を鳴らす
end

function update(dt)
  local c = object("frame")
  local x, y = getPosPx(c)
  if key("up") then y = y - 200 * dt end
  if key("down")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## 歩いてジャンプする3Dキャラクター

```lua
properties = { speed = 4, jump = 6 }

local vz = 0                   -- 垂直方向の速度（ジャンプ用）

function update(dt)
  local yo_ = self()
  local v = property("speed")
  local dx, dy = 0, 0
  if key("left") then dx = dx - 1 end
  if key("right")   then dx = dx + 1 end
  if key("up")    then dy = dy - 1 end
  if key("down")     then dy = dy + 1 end
  move(yo_, dx * v * dt, dy * v * dt, 0)

  -- シンプルなジャンプ：上昇し、重力で下がり、床（z = 0）より下には行かない
  local x, y, z = getPosition(yo_)
  if keyPressed("space") and z <= 0 then vz = property("jump") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosition(yo_, x, y, z)
end
```

## ヒットボックスでコインを集める

```lua
-- このスクリプトは、子にヒットボックスを持つキャラクターに付ける
local monedas = 0

function onEnter(other, myHitbox, otherHitbox)
  if string.sub(getName(other), 1, 6) == "Moneda" then
    destroy(other)
    monedas = monedas + 1
    setShared("monedas", monedas)   -- スコア表示のスクリプトがこれを読む
    sound("sounds/moneda.wav")
  end
end
```

```lua
-- こちらのスクリプトは、スコア表示の2Dテキストに付ける
function update(dt)
  setText(self(), "MONEDAS: " .. (getShared("monedas") or 0))
end
```

## 2秒ごとに敵を生成する

```lua
local time = 0

function update(dt)
  time = time + dt
  if time >= 2 then
    time = 0
    local x = random(-5, 5)
    instantiate("Enemigo", x, 0, -10, 180)   -- プレハブ「Enemigo」、こちらを向いた状態
  end
end
```

## シーンを切り替えるボタン

```lua
properties = { button = "object" }

function update(dt)
  if pressed(object("button")) or keyPressed("enter") then
    changeScene("Nivel1")
  end
end
```

> [!TIP]
> サンプルのリポジトリに、コメント付きの完全なPongがあります：`Whisk3D-Examples/ui/whiskpaddle/content/juego.lua`。`speed`、`bounceOn`、`limit`、`finger`、`uiBox`、`random`を使っています。
