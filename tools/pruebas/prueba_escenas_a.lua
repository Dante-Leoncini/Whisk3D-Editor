-- escena A de prueba_escenas_play.w3s (prueba del motor): cuenta sus arranques, mueve su
-- objeto (el Stop lo tiene que devolver) y al 3er tick pasa a la escena 3D "Nivel2"
local n = 0
function inicio()
  setCompartido("a_inicios", (compartido("a_inicios") or 0) + 1)
end
function actualizar(dt)
  n = n + 1
  local x, y, z = posicion(yo())
  setPosicion(yo(), x + 1, y, z)
  setCompartido("a_n", n)
  if n == 3 then cambiarEscena("Nivel2") end
end
