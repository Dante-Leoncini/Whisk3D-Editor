# Exemplos completos

## Olá, mundo: mover um quadrado 2D com as setas

```lua
propiedades = { cuadro = "objeto" }

function inicio()
  beep(440, 120, 0.5)          -- um bipe para saber que começou
end

function actualizar(dt)
  local c = objeto("cuadro")
  local x, y = posPx(c)
  if tecla("arriba") then y = y - 200 * dt end
  if tecla("abajo")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## Um personagem 3D que anda e pula

```lua
propiedades = { velocidad = 4, salto = 6 }

local vz = 0                   -- velocidade vertical (para o pulo)

function actualizar(dt)
  local yo_ = yo()
  local v = propiedad("velocidad")
  local dx, dy = 0, 0
  if tecla("izquierda") then dx = dx - 1 end
  if tecla("derecha")   then dx = dx + 1 end
  if tecla("arriba")    then dy = dy - 1 end
  if tecla("abajo")     then dy = dy + 1 end
  mover(yo_, dx * v * dt, dy * v * dt, 0)

  -- pulo simples: sobe, a gravidade o traz para baixo e ele não passa do chão (z = 0)
  local x, y, z = posicion(yo_)
  if teclaApretada("espacio") and z <= 0 then vz = propiedad("salto") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosicion(yo_, x, y, z)
end
```

## Juntar moedas com um hitbox

```lua
-- este script vai no personagem, que tem um hitbox filho
local monedas = 0

function alEntrar(otro, miHitbox, suHitbox)
  if string.sub(nombre(otro), 1, 6) == "Moneda" then
    destruir(otro)
    monedas = monedas + 1
    setCompartido("monedas", monedas)   -- o script do placar o lê
    sonido("sonidos/moneda.wav")
  end
end
```

```lua
-- este outro script vai no texto 2D do placar
function actualizar(dt)
  setTexto(yo(), "MONEDAS: " .. (compartido("monedas") or 0))
end
```

## Criar inimigos a cada 2 segundos

```lua
local tiempo = 0

function actualizar(dt)
  tiempo = tiempo + dt
  if tiempo >= 2 then
    tiempo = 0
    local x = azar(-5, 5)
    instanciar("Enemigo", x, 0, -10, 180)   -- prefab "Enemigo", olhando para nós
  end
end
```

## Um botão que muda de cena

```lua
propiedades = { boton = "objeto" }

function actualizar(dt)
  if apretado(objeto("boton")) or teclaApretada("enter") then
    cambiarEscena("Nivel1")
  end
end
```

> [!TIP]
> Há um Pong completo e comentado no repositório de exemplos: `Whisk3D-Examples/ui/whiskpaddle/contenido/juego.lua`. Ele usa `velocidad`, `rebotarEn`, `limitar`, `dedo`, `cajaUI` e `azar`.
