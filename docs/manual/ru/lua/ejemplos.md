# Полные примеры

## Привет, мир: двигаем 2D-квадрат стрелками

```lua
propiedades = { cuadro = "objeto" }

function inicio()
  beep(440, 120, 0.5)          -- сигнал, чтобы знать, что скрипт запустился
end

function actualizar(dt)
  local c = objeto("cuadro")
  local x, y = posPx(c)
  if tecla("arriba") then y = y - 200 * dt end
  if tecla("abajo")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## 3D-персонаж, который ходит и прыгает

```lua
propiedades = { velocidad = 4, salto = 6 }

local vz = 0                   -- вертикальная скорость (для прыжка)

function actualizar(dt)
  local yo_ = yo()
  local v = propiedad("velocidad")
  local dx, dy = 0, 0
  if tecla("izquierda") then dx = dx - 1 end
  if tecla("derecha")   then dx = dx + 1 end
  if tecla("arriba")    then dy = dy - 1 end
  if tecla("abajo")     then dy = dy + 1 end
  mover(yo_, dx * v * dt, dy * v * dt, 0)

  -- простой прыжок: вверх, гравитация тянет вниз, ниже пола (z = 0) не опускается
  local x, y, z = posicion(yo_)
  if teclaApretada("espacio") and z <= 0 then vz = propiedad("salto") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosicion(yo_, x, y, z)
end
```

## Собираем монеты с помощью хитбокса

```lua
-- этот скрипт висит на персонаже, у которого есть дочерний хитбокс
local monedas = 0

function alEntrar(otro, miHitbox, suHitbox)
  if string.sub(nombre(otro), 1, 6) == "Moneda" then
    destruir(otro)
    monedas = monedas + 1
    setCompartido("monedas", monedas)   -- его читает скрипт счётчика
    sonido("sonidos/moneda.wav")
  end
end
```

```lua
-- а этот скрипт висит на 2D-тексте счётчика
function actualizar(dt)
  setTexto(yo(), "MONEDAS: " .. (compartido("monedas") or 0))
end
```

## Создаём врагов каждые 2 секунды

```lua
local tiempo = 0

function actualizar(dt)
  tiempo = tiempo + dt
  if tiempo >= 2 then
    tiempo = 0
    local x = azar(-5, 5)
    instanciar("Enemigo", x, 0, -10, 180)   -- префаб "Enemigo", повёрнутый к нам
  end
end
```

## Кнопка, которая меняет сцену

```lua
propiedades = { boton = "objeto" }

function actualizar(dt)
  if apretado(objeto("boton")) or teclaApretada("enter") then
    cambiarEscena("Nivel1")
  end
end
```

> [!TIP]
> В репозитории примеров есть полная игра Pong с комментариями: `Whisk3D-Examples/ui/whiskpaddle/contenido/juego.lua`. В ней используются `velocidad`, `rebotarEn`, `limitar`, `dedo`, `cajaUI` и `azar`.
