# Ejemplos completos

## Hola mundo: mover un cuadro 2D con las flechas

```lua
properties = { frame = "object" }

function start()
  beep(440, 120, 0.5)          -- un pitido para saber que arrancó
end

function update(dt)
  local c = object("frame")
  local x, y = getPosPx(c)
  if key("up") then y = y - 200 * dt end
  if key("down")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## Un personaje 3D que camina y salta

```lua
properties = { speed = 4, jump = 6 }

local vz = 0                   -- velocidad vertical (para el salto)

function update(dt)
  local yo_ = self()
  local v = property("speed")
  local dx, dy = 0, 0
  if key("left") then dx = dx - 1 end
  if key("right")   then dx = dx + 1 end
  if key("up")    then dy = dy - 1 end
  if key("down")     then dy = dy + 1 end
  move(yo_, dx * v * dt, dy * v * dt, 0)

  -- salto simple: sube, la gravedad lo baja y no pasa del piso (z = 0)
  local x, y, z = getPosition(yo_)
  if keyPressed("space") and z <= 0 then vz = property("jump") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosition(yo_, x, y, z)
end
```

## Juntar monedas con un hitbox

```lua
-- este script va en el personaje, que tiene un hitbox hijo
local monedas = 0

function onEnter(other, myHitbox, otherHitbox)
  if string.sub(getName(other), 1, 6) == "Moneda" then
    destroy(other)
    monedas = monedas + 1
    setShared("monedas", monedas)   -- el script del marcador lo lee
    sound("sounds/moneda.wav")
  end
end
```

```lua
-- este otro script va en el texto 2D del marcador
function update(dt)
  setText(self(), "MONEDAS: " .. (getShared("monedas") or 0))
end
```

## Crear enemigos cada 2 segundos

```lua
local time = 0

function update(dt)
  time = time + dt
  if time >= 2 then
    time = 0
    local x = random(-5, 5)
    instantiate("Enemigo", x, 0, -10, 180)   -- prefab "Enemigo", mirando hacia nosotros
  end
end
```

## Un botón que cambia de escena

```lua
properties = { button = "object" }

function update(dt)
  if pressed(object("button")) or keyPressed("enter") then
    changeScene("Nivel1")
  end
end
```

> [!TIP]
> Hay un Pong completo y comentado en el repositorio de ejemplos: `Whisk3D-Examples/ui/whiskpaddle/content/juego.lua`. Usa `speed`, `bounceOn`, `limit`, `finger`, `uiBox` y `random`.
