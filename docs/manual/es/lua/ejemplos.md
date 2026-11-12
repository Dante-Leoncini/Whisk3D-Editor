# Ejemplos completos

## Hola mundo: mover un cuadro 2D con las flechas

```lua
propiedades = { cuadro = "objeto" }

function inicio()
  beep(440, 120, 0.5)          -- un pitido para saber que arrancó
end

function actualizar(dt)
  local c = objeto("cuadro")
  local x, y = posPx(c)
  if tecla("arriba") then y = y - 200 * dt end
  if tecla("abajo")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## Un personaje 3D que camina y salta

```lua
propiedades = { velocidad = 4, salto = 6 }

local vz = 0                   -- velocidad vertical (para el salto)

function actualizar(dt)
  local yo_ = yo()
  local v = propiedad("velocidad")
  local dx, dy = 0, 0
  if tecla("izquierda") then dx = dx - 1 end
  if tecla("derecha")   then dx = dx + 1 end
  if tecla("arriba")    then dy = dy - 1 end
  if tecla("abajo")     then dy = dy + 1 end
  mover(yo_, dx * v * dt, dy * v * dt, 0)

  -- salto simple: sube, la gravedad lo baja y no pasa del piso (z = 0)
  local x, y, z = posicion(yo_)
  if teclaApretada("espacio") and z <= 0 then vz = propiedad("salto") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosicion(yo_, x, y, z)
end
```

## Juntar monedas con un hitbox

```lua
-- este script va en el personaje, que tiene un hitbox hijo
local monedas = 0

function alEntrar(otro, miHitbox, suHitbox)
  if string.sub(nombre(otro), 1, 6) == "Moneda" then
    destruir(otro)
    monedas = monedas + 1
    setCompartido("monedas", monedas)   -- el script del marcador lo lee
    sonido("sonidos/moneda.wav")
  end
end
```

```lua
-- este otro script va en el texto 2D del marcador
function actualizar(dt)
  setTexto(yo(), "MONEDAS: " .. (compartido("monedas") or 0))
end
```

## Crear enemigos cada 2 segundos

```lua
local tiempo = 0

function actualizar(dt)
  tiempo = tiempo + dt
  if tiempo >= 2 then
    tiempo = 0
    local x = azar(-5, 5)
    instanciar("Enemigo", x, 0, -10, 180)   -- prefab "Enemigo", mirando hacia nosotros
  end
end
```

## Un botón que cambia de escena

```lua
propiedades = { boton = "objeto" }

function actualizar(dt)
  if apretado(objeto("boton")) or teclaApretada("enter") then
    cambiarEscena("Nivel1")
  end
end
```

> [!TIP]
> Hay un Pong completo y comentado en el repositorio de ejemplos: `Whisk3D-Examples/ui/whiskpaddle/contenido/juego.lua`. Usa `velocidad`, `rebotarEn`, `limitar`, `dedo`, `cajaUI` y `azar`.
