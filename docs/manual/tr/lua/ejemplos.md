# Eksiksiz örnekler

## Merhaba dünya: ok tuşlarıyla bir 2D kareyi hareket ettirmek

```lua
properties = { frame = "object" }

function start()
  beep(440, 120, 0.5)          -- başladığını anlamak için bir bip sesi
end

function update(dt)
  local c = object("frame")
  local x, y = getPosPx(c)
  if key("up") then y = y - 200 * dt end
  if key("down")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## Yürüyen ve zıplayan bir 3D karakter

```lua
properties = { speed = 4, jump = 6 }

local vz = 0                   -- dikey hız (zıplama için)

function update(dt)
  local yo_ = self()
  local v = property("speed")
  local dx, dy = 0, 0
  if key("left") then dx = dx - 1 end
  if key("right")   then dx = dx + 1 end
  if key("up")    then dy = dy - 1 end
  if key("down")     then dy = dy + 1 end
  move(yo_, dx * v * dt, dy * v * dt, 0)

  -- basit zıplama: yükselir, yerçekimi onu indirir ve zeminin (z = 0) altına inmez
  local x, y, z = getPosition(yo_)
  if keyPressed("space") and z <= 0 then vz = property("jump") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosition(yo_, x, y, z)
end
```

## Bir hitbox ile para toplamak

```lua
-- bu script, çocuğu olarak bir hitbox'ı bulunan karaktere konur
local monedas = 0

function onEnter(other, myHitbox, otherHitbox)
  if string.sub(getName(other), 1, 6) == "Moneda" then
    destroy(other)
    monedas = monedas + 1
    setShared("monedas", monedas)   -- skor tablosunun script'i bunu okur
    sound("sounds/moneda.wav")
  end
end
```

```lua
-- bu diğer script, skor tablosunun 2D metnine konur
function update(dt)
  setText(self(), "MONEDAS: " .. (getShared("monedas") or 0))
end
```

## Her 2 saniyede bir düşman oluşturmak

```lua
local time = 0

function update(dt)
  time = time + dt
  if time >= 2 then
    time = 0
    local x = random(-5, 5)
    instantiate("Enemigo", x, 0, -10, 180)   -- "Enemigo" prefab'ı, bize doğru bakıyor
  end
end
```

## Sahne değiştiren bir düğme

```lua
properties = { button = "object" }

function update(dt)
  if pressed(object("button")) or keyPressed("enter") then
    changeScene("Nivel1")
  end
end
```

> [!TIP]
> Örnekler deposunda eksiksiz ve yorumlu bir Pong var: `Whisk3D-Examples/ui/whiskpaddle/content/juego.lua`. `speed`, `bounceOn`, `limit`, `finger`, `uiBox` ve `random` kullanır.
