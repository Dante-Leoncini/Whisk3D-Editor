# Exemples complets

## Hello world : déplacer un carré 2D avec les flèches

```lua
propiedades = { cuadro = "objeto" }

function inicio()
  beep(440, 120, 0.5)          -- un bip pour savoir que le script a démarré
end

function actualizar(dt)
  local c = objeto("cuadro")
  local x, y = posPx(c)
  if tecla("arriba") then y = y - 200 * dt end
  if tecla("abajo")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## Un personnage 3D qui marche et saute

```lua
propiedades = { velocidad = 4, salto = 6 }

local vz = 0                   -- vitesse verticale (pour le saut)

function actualizar(dt)
  local yo_ = yo()
  local v = propiedad("velocidad")
  local dx, dy = 0, 0
  if tecla("izquierda") then dx = dx - 1 end
  if tecla("derecha")   then dx = dx + 1 end
  if tecla("arriba")    then dy = dy - 1 end
  if tecla("abajo")     then dy = dy + 1 end
  mover(yo_, dx * v * dt, dy * v * dt, 0)

  -- saut simple : il monte, la gravité le fait redescendre et il ne passe pas sous le sol (z = 0)
  local x, y, z = posicion(yo_)
  if teclaApretada("espacio") and z <= 0 then vz = propiedad("salto") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosicion(yo_, x, y, z)
end
```

## Ramasser des pièces avec une hitbox

```lua
-- ce script va sur le personnage, qui a une hitbox enfant
local monedas = 0

function alEntrar(otro, miHitbox, suHitbox)
  if string.sub(nombre(otro), 1, 6) == "Moneda" then
    destruir(otro)
    monedas = monedas + 1
    setCompartido("monedas", monedas)   -- le script du compteur le lit
    sonido("sonidos/moneda.wav")
  end
end
```

```lua
-- cet autre script va sur le texte 2D du compteur
function actualizar(dt)
  setTexto(yo(), "MONEDAS: " .. (compartido("monedas") or 0))
end
```

## Créer des ennemis toutes les 2 secondes

```lua
local tiempo = 0

function actualizar(dt)
  tiempo = tiempo + dt
  if tiempo >= 2 then
    tiempo = 0
    local x = azar(-5, 5)
    instanciar("Enemigo", x, 0, -10, 180)   -- prefab "Enemigo", tourné vers nous
  end
end
```

## Un bouton qui change de scène

```lua
propiedades = { boton = "objeto" }

function actualizar(dt)
  if apretado(objeto("boton")) or teclaApretada("enter") then
    cambiarEscena("Nivel1")
  end
end
```

> [!TIP]
> Un Pong complet et commenté se trouve dans le dépôt d'exemples : `Whisk3D-Examples/ui/whiskpaddle/contenido/juego.lua`. Il utilise `velocidad`, `rebotarEn`, `limitar`, `dedo`, `cajaUI` et `azar`.
