# Esempi completi

## Ciao mondo: muovere un riquadro 2D con le frecce

```lua
propiedades = { cuadro = "objeto" }

function inicio()
  beep(440, 120, 0.5)          -- un bip per sapere che è partito
end

function actualizar(dt)
  local c = objeto("cuadro")
  local x, y = posPx(c)
  if tecla("arriba") then y = y - 200 * dt end
  if tecla("abajo")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## Un personaggio 3D che cammina e salta

```lua
propiedades = { velocidad = 4, salto = 6 }

local vz = 0                   -- velocità verticale (per il salto)

function actualizar(dt)
  local yo_ = yo()
  local v = propiedad("velocidad")
  local dx, dy = 0, 0
  if tecla("izquierda") then dx = dx - 1 end
  if tecla("derecha")   then dx = dx + 1 end
  if tecla("arriba")    then dy = dy - 1 end
  if tecla("abajo")     then dy = dy + 1 end
  mover(yo_, dx * v * dt, dy * v * dt, 0)

  -- salto semplice: sale, la gravità lo riporta giù e non scende sotto il pavimento (z = 0)
  local x, y, z = posicion(yo_)
  if teclaApretada("espacio") and z <= 0 then vz = propiedad("salto") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosicion(yo_, x, y, z)
end
```

## Raccogliere monete con un hitbox

```lua
-- questo script va sul personaggio, che ha un hitbox figlio
local monedas = 0

function alEntrar(otro, miHitbox, suHitbox)
  if string.sub(nombre(otro), 1, 6) == "Moneda" then
    destruir(otro)
    monedas = monedas + 1
    setCompartido("monedas", monedas)   -- lo script del segnapunti lo legge
    sonido("sonidos/moneda.wav")
  end
end
```

```lua
-- quest'altro script va sul testo 2D del segnapunti
function actualizar(dt)
  setTexto(yo(), "MONEDAS: " .. (compartido("monedas") or 0))
end
```

## Creare nemici ogni 2 secondi

```lua
local tiempo = 0

function actualizar(dt)
  tiempo = tiempo + dt
  if tiempo >= 2 then
    tiempo = 0
    local x = azar(-5, 5)
    instanciar("Enemigo", x, 0, -10, 180)   -- prefab "Enemigo", rivolto verso di noi
  end
end
```

## Un pulsante che cambia scena

```lua
propiedades = { boton = "objeto" }

function actualizar(dt)
  if apretado(objeto("boton")) or teclaApretada("enter") then
    cambiarEscena("Nivel1")
  end
end
```

> [!TIP]
> C'è un Pong completo e commentato nel repository degli esempi: `Whisk3D-Examples/ui/whiskpaddle/contenido/juego.lua`. Usa `velocidad`, `rebotarEn`, `limitar`, `dedo`, `cajaUI` e `azar`.
