# Полные примеры

## Привет, мир: двигаем 2D-квадрат стрелками

```lua
properties = { frame = "object" }

function start()
  beep(440, 120, 0.5)          -- сигнал, чтобы знать, что скрипт запустился
end

function update(dt)
  local c = object("frame")
  local x, y = getPosPx(c)
  if key("up") then y = y - 200 * dt end
  if key("down")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## 3D-персонаж, который ходит и прыгает

```lua
properties = { speed = 4, jump = 6 }

local vz = 0                   -- вертикальная скорость (для прыжка)

function update(dt)
  local yo_ = self()
  local v = property("speed")
  local dx, dy = 0, 0
  if key("left") then dx = dx - 1 end
  if key("right")   then dx = dx + 1 end
  if key("up")    then dy = dy - 1 end
  if key("down")     then dy = dy + 1 end
  move(yo_, dx * v * dt, dy * v * dt, 0)

  -- простой прыжок: вверх, гравитация тянет вниз, ниже пола (z = 0) не опускается
  local x, y, z = getPosition(yo_)
  if keyPressed("space") and z <= 0 then vz = property("jump") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosition(yo_, x, y, z)
end
```

## Собираем монеты с помощью хитбокса

```lua
-- этот скрипт висит на персонаже, у которого есть дочерний хитбокс
local monedas = 0

function onEnter(other, myHitbox, otherHitbox)
  if string.sub(getName(other), 1, 6) == "Moneda" then
    destroy(other)
    monedas = monedas + 1
    setShared("monedas", monedas)   -- его читает скрипт счётчика
    sound("sounds/moneda.wav")
  end
end
```

```lua
-- а этот скрипт висит на 2D-тексте счётчика
function update(dt)
  setText(self(), "MONEDAS: " .. (getShared("monedas") or 0))
end
```

## Создаём врагов каждые 2 секунды

```lua
local time = 0

function update(dt)
  time = time + dt
  if time >= 2 then
    time = 0
    local x = random(-5, 5)
    instantiate("Enemigo", x, 0, -10, 180)   -- префаб "Enemigo", повёрнутый к нам
  end
end
```

## Кнопка, которая меняет сцену

```lua
properties = { button = "object" }

function update(dt)
  if pressed(object("button")) or keyPressed("enter") then
    changeScene("Nivel1")
  end
end
```

> [!TIP]
> В репозитории примеров есть полная игра Pong с комментариями: `Whisk3D-Examples/ui/whiskpaddle/content/juego.lua`. В ней используются `speed`, `bounceOn`, `limit`, `finger`, `uiBox` и `random`.
