-- escena B ("Nivel2") de prueba_escenas_play.w3s: sube su objeto un paso por tick y, la
-- PRIMERA vez que corre, al 4to tick vuelve a la escena del bloque ("Scene")
local n = 0
function inicio()
  setCompartido("b_inicios", (compartido("b_inicios") or 0) + 1)
end
function actualizar(dt)
  n = n + 1
  local x, y, z = posicion(yo())
  setPosicion(yo(), x, y + 1, z)
  setCompartido("b_n", n)
  if n == 4 and (compartido("b_inicios") or 0) < 2 then cambiarEscena("Scene") end
end
