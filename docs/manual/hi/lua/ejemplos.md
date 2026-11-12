# पूरे उदाहरण

## हैलो वर्ल्ड: ऐरो कुंजियों से एक 2D बॉक्स मूव करना

```lua
propiedades = { cuadro = "objeto" }

function inicio()
  beep(440, 120, 0.5)          -- एक बीप, ताकि पता चले कि स्क्रिप्ट शुरू हो गई
end

function actualizar(dt)
  local c = objeto("cuadro")
  local x, y = posPx(c)
  if tecla("arriba") then y = y - 200 * dt end
  if tecla("abajo")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## एक 3D कैरेक्टर जो चलता और कूदता है

```lua
propiedades = { velocidad = 4, salto = 6 }

local vz = 0                   -- ऊर्ध्वाधर वेग (छलाँग के लिए)

function actualizar(dt)
  local yo_ = yo()
  local v = propiedad("velocidad")
  local dx, dy = 0, 0
  if tecla("izquierda") then dx = dx - 1 end
  if tecla("derecha")   then dx = dx + 1 end
  if tecla("arriba")    then dy = dy - 1 end
  if tecla("abajo")     then dy = dy + 1 end
  mover(yo_, dx * v * dt, dy * v * dt, 0)

  -- सरल छलाँग: ऊपर जाता है, गुरुत्वाकर्षण उसे नीचे लाता है और वह फ़र्श (z = 0) से नीचे नहीं जाता
  local x, y, z = posicion(yo_)
  if teclaApretada("espacio") and z <= 0 then vz = propiedad("salto") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosicion(yo_, x, y, z)
end
```

## हिटबॉक्स से सिक्के इकट्ठा करना

```lua
-- यह स्क्रिप्ट कैरेक्टर पर लगती है, जिसका एक चाइल्ड हिटबॉक्स है
local monedas = 0

function alEntrar(otro, miHitbox, suHitbox)
  if string.sub(nombre(otro), 1, 6) == "Moneda" then
    destruir(otro)
    monedas = monedas + 1
    setCompartido("monedas", monedas)   -- स्कोरबोर्ड की स्क्रिप्ट इसे पढ़ती है
    sonido("sonidos/moneda.wav")
  end
end
```

```lua
-- यह दूसरी स्क्रिप्ट स्कोरबोर्ड के 2D टेक्स्ट पर लगती है
function actualizar(dt)
  setTexto(yo(), "MONEDAS: " .. (compartido("monedas") or 0))
end
```

## हर 2 सेकंड में दुश्मन बनाना

```lua
local tiempo = 0

function actualizar(dt)
  tiempo = tiempo + dt
  if tiempo >= 2 then
    tiempo = 0
    local x = azar(-5, 5)
    instanciar("Enemigo", x, 0, -10, 180)   -- "Enemigo" प्रीफ़ैब, हमारी ओर मुँह किए हुए
  end
end
```

## एक बटन जो सीन बदलता है

```lua
propiedades = { boton = "objeto" }

function actualizar(dt)
  if apretado(objeto("boton")) or teclaApretada("enter") then
    cambiarEscena("Nivel1")
  end
end
```

> [!TIP]
> उदाहरणों वाले रिपॉज़िटरी में टिप्पणियों के साथ एक पूरा Pong है: `Whisk3D-Examples/ui/whiskpaddle/contenido/juego.lua`। इसमें `velocidad`, `rebotarEn`, `limitar`, `dedo`, `cajaUI` और `azar` का इस्तेमाल होता है।
