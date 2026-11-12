# Eksiksiz örnekler

## Merhaba dünya: ok tuşlarıyla bir 2D kareyi hareket ettirmek

```lua
propiedades = { cuadro = "objeto" }

function inicio()
  beep(440, 120, 0.5)          -- başladığını anlamak için bir bip sesi
end

function actualizar(dt)
  local c = objeto("cuadro")
  local x, y = posPx(c)
  if tecla("arriba") then y = y - 200 * dt end
  if tecla("abajo")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## Yürüyen ve zıplayan bir 3D karakter

```lua
propiedades = { velocidad = 4, salto = 6 }

local vz = 0                   -- dikey hız (zıplama için)

function actualizar(dt)
  local yo_ = yo()
  local v = propiedad("velocidad")
  local dx, dy = 0, 0
  if tecla("izquierda") then dx = dx - 1 end
  if tecla("derecha")   then dx = dx + 1 end
  if tecla("arriba")    then dy = dy - 1 end
  if tecla("abajo")     then dy = dy + 1 end
  mover(yo_, dx * v * dt, dy * v * dt, 0)

  -- basit zıplama: yükselir, yerçekimi onu indirir ve zeminin (z = 0) altına inmez
  local x, y, z = posicion(yo_)
  if teclaApretada("espacio") and z <= 0 then vz = propiedad("salto") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosicion(yo_, x, y, z)
end
```

## Bir hitbox ile para toplamak

```lua
-- bu script, çocuğu olarak bir hitbox'ı bulunan karaktere konur
local monedas = 0

function alEntrar(otro, miHitbox, suHitbox)
  if string.sub(nombre(otro), 1, 6) == "Moneda" then
    destruir(otro)
    monedas = monedas + 1
    setCompartido("monedas", monedas)   -- skor tablosunun script'i bunu okur
    sonido("sonidos/moneda.wav")
  end
end
```

```lua
-- bu diğer script, skor tablosunun 2D metnine konur
function actualizar(dt)
  setTexto(yo(), "MONEDAS: " .. (compartido("monedas") or 0))
end
```

## Her 2 saniyede bir düşman oluşturmak

```lua
local tiempo = 0

function actualizar(dt)
  tiempo = tiempo + dt
  if tiempo >= 2 then
    tiempo = 0
    local x = azar(-5, 5)
    instanciar("Enemigo", x, 0, -10, 180)   -- "Enemigo" prefab'ı, bize doğru bakıyor
  end
end
```

## Sahne değiştiren bir düğme

```lua
propiedades = { boton = "objeto" }

function actualizar(dt)
  if apretado(objeto("boton")) or teclaApretada("enter") then
    cambiarEscena("Nivel1")
  end
end
```

> [!TIP]
> Örnekler deposunda eksiksiz ve yorumlu bir Pong var: `Whisk3D-Examples/ui/whiskpaddle/contenido/juego.lua`. `velocidad`, `rebotarEn`, `limitar`, `dedo`, `cajaUI` ve `azar` kullanır.
