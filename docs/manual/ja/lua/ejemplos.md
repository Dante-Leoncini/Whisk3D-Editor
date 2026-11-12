# 完全なサンプル

## Hello World：方向キーで2Dの四角を動かす

```lua
propiedades = { cuadro = "objeto" }

function inicio()
  beep(440, 120, 0.5)          -- 起動したことがわかるようにビープ音を鳴らす
end

function actualizar(dt)
  local c = objeto("cuadro")
  local x, y = posPx(c)
  if tecla("arriba") then y = y - 200 * dt end
  if tecla("abajo")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## 歩いてジャンプする3Dキャラクター

```lua
propiedades = { velocidad = 4, salto = 6 }

local vz = 0                   -- 垂直方向の速度（ジャンプ用）

function actualizar(dt)
  local yo_ = yo()
  local v = propiedad("velocidad")
  local dx, dy = 0, 0
  if tecla("izquierda") then dx = dx - 1 end
  if tecla("derecha")   then dx = dx + 1 end
  if tecla("arriba")    then dy = dy - 1 end
  if tecla("abajo")     then dy = dy + 1 end
  mover(yo_, dx * v * dt, dy * v * dt, 0)

  -- シンプルなジャンプ：上昇し、重力で下がり、床（z = 0）より下には行かない
  local x, y, z = posicion(yo_)
  if teclaApretada("espacio") and z <= 0 then vz = propiedad("salto") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosicion(yo_, x, y, z)
end
```

## ヒットボックスでコインを集める

```lua
-- このスクリプトは、子にヒットボックスを持つキャラクターに付ける
local monedas = 0

function alEntrar(otro, miHitbox, suHitbox)
  if string.sub(nombre(otro), 1, 6) == "Moneda" then
    destruir(otro)
    monedas = monedas + 1
    setCompartido("monedas", monedas)   -- スコア表示のスクリプトがこれを読む
    sonido("sonidos/moneda.wav")
  end
end
```

```lua
-- こちらのスクリプトは、スコア表示の2Dテキストに付ける
function actualizar(dt)
  setTexto(yo(), "MONEDAS: " .. (compartido("monedas") or 0))
end
```

## 2秒ごとに敵を生成する

```lua
local tiempo = 0

function actualizar(dt)
  tiempo = tiempo + dt
  if tiempo >= 2 then
    tiempo = 0
    local x = azar(-5, 5)
    instanciar("Enemigo", x, 0, -10, 180)   -- プレハブ「Enemigo」、こちらを向いた状態
  end
end
```

## シーンを切り替えるボタン

```lua
propiedades = { boton = "objeto" }

function actualizar(dt)
  if apretado(objeto("boton")) or teclaApretada("enter") then
    cambiarEscena("Nivel1")
  end
end
```

> [!TIP]
> サンプルのリポジトリに、コメント付きの完全なPongがあります：`Whisk3D-Examples/ui/whiskpaddle/contenido/juego.lua`。`velocidad`、`rebotarEn`、`limitar`、`dedo`、`cajaUI`、`azar`を使っています。
