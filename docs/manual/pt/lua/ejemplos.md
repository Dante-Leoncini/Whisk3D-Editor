# Exemplos completos

## Olá, mundo: mover um quadrado 2D com as setas

```lua
properties = { frame = "object" }

function start()
  beep(440, 120, 0.5)          -- um bipe para saber que começou
end

function update(dt)
  local c = object("frame")
  local x, y = getPosPx(c)
  if key("up") then y = y - 200 * dt end
  if key("down")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## Um personagem 3D que anda e pula

```lua
properties = { speed = 4, jump = 6 }

local vz = 0                   -- velocidade vertical (para o pulo)

function update(dt)
  local yo_ = self()
  local v = property("speed")
  local dx, dy = 0, 0
  if key("left") then dx = dx - 1 end
  if key("right")   then dx = dx + 1 end
  if key("up")    then dy = dy - 1 end
  if key("down")     then dy = dy + 1 end
  move(yo_, dx * v * dt, dy * v * dt, 0)

  -- pulo simples: sobe, a gravidade o traz para baixo e ele não passa do chão (z = 0)
  local x, y, z = getPosition(yo_)
  if keyPressed("space") and z <= 0 then vz = property("jump") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosition(yo_, x, y, z)
end
```

## Juntar moedas com um hitbox

```lua
-- este script vai no personagem, que tem um hitbox filho
local monedas = 0

function onEnter(other, myHitbox, otherHitbox)
  if string.sub(getName(other), 1, 6) == "Moneda" then
    destroy(other)
    monedas = monedas + 1
    setShared("monedas", monedas)   -- o script do placar o lê
    sound("sounds/moneda.wav")
  end
end
```

```lua
-- este outro script vai no texto 2D do placar
function update(dt)
  setText(self(), "MONEDAS: " .. (getShared("monedas") or 0))
end
```

## Criar inimigos a cada 2 segundos

```lua
local time = 0

function update(dt)
  time = time + dt
  if time >= 2 then
    time = 0
    local x = random(-5, 5)
    instantiate("Enemigo", x, 0, -10, 180)   -- prefab "Enemigo", olhando para nós
  end
end
```

## Um botão que muda de cena

```lua
properties = { button = "object" }

function update(dt)
  if pressed(object("button")) or keyPressed("enter") then
    changeScene("Nivel1")
  end
end
```

> [!TIP]
> Há um Pong completo e comentado no repositório de exemplos: `Whisk3D-Examples/ui/whiskpaddle/content/juego.lua`. Ele usa `speed`, `bounceOn`, `limit`, `finger`, `uiBox` e `random`.
