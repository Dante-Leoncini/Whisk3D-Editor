# पूरे उदाहरण

## हैलो वर्ल्ड: ऐरो कुंजियों से एक 2D बॉक्स मूव करना

```lua
properties = { frame = "object" }

function start()
  beep(440, 120, 0.5)          -- एक बीप, ताकि पता चले कि स्क्रिप्ट शुरू हो गई
end

function update(dt)
  local c = object("frame")
  local x, y = getPosPx(c)
  if key("up") then y = y - 200 * dt end
  if key("down")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## एक 3D कैरेक्टर जो चलता और कूदता है

```lua
properties = { speed = 4, jump = 6 }

local vz = 0                   -- ऊर्ध्वाधर वेग (छलाँग के लिए)

function update(dt)
  local yo_ = self()
  local v = property("speed")
  local dx, dy = 0, 0
  if key("left") then dx = dx - 1 end
  if key("right")   then dx = dx + 1 end
  if key("up")    then dy = dy - 1 end
  if key("down")     then dy = dy + 1 end
  move(yo_, dx * v * dt, dy * v * dt, 0)

  -- सरल छलाँग: ऊपर जाता है, गुरुत्वाकर्षण उसे नीचे लाता है और वह फ़र्श (z = 0) से नीचे नहीं जाता
  local x, y, z = getPosition(yo_)
  if keyPressed("space") and z <= 0 then vz = property("jump") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosition(yo_, x, y, z)
end
```

## हिटबॉक्स से सिक्के इकट्ठा करना

```lua
-- यह स्क्रिप्ट कैरेक्टर पर लगती है, जिसका एक चाइल्ड हिटबॉक्स है
local monedas = 0

function onEnter(other, myHitbox, otherHitbox)
  if string.sub(getName(other), 1, 6) == "Moneda" then
    destroy(other)
    monedas = monedas + 1
    setShared("monedas", monedas)   -- स्कोरबोर्ड की स्क्रिप्ट इसे पढ़ती है
    sound("sounds/moneda.wav")
  end
end
```

```lua
-- यह दूसरी स्क्रिप्ट स्कोरबोर्ड के 2D टेक्स्ट पर लगती है
function update(dt)
  setText(self(), "MONEDAS: " .. (getShared("monedas") or 0))
end
```

## हर 2 सेकंड में दुश्मन बनाना

```lua
local time = 0

function update(dt)
  time = time + dt
  if time >= 2 then
    time = 0
    local x = random(-5, 5)
    instantiate("Enemigo", x, 0, -10, 180)   -- "Enemigo" प्रीफ़ैब, हमारी ओर मुँह किए हुए
  end
end
```

## एक बटन जो सीन बदलता है

```lua
properties = { button = "object" }

function update(dt)
  if pressed(object("button")) or keyPressed("enter") then
    changeScene("Nivel1")
  end
end
```

> [!TIP]
> उदाहरणों वाले रिपॉज़िटरी में टिप्पणियों के साथ एक पूरा Pong है: `Whisk3D-Examples/ui/whiskpaddle/content/juego.lua`। इसमें `speed`, `bounceOn`, `limit`, `finger`, `uiBox` और `random` का इस्तेमाल होता है।
