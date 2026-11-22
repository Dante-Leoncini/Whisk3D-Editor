-- escena A de prueba_escenas_play.w3s (prueba del motor): cuenta sus arranques, mueve su
-- objeto (el Stop lo tiene que devolver) y al 3er tick pasa a la escena 3D "Nivel2"
local n = 0
function start()
  setShared("a_inicios", (getShared("a_inicios") or 0) + 1)
end
function update(dt)
  n = n + 1
  local x, y, z = getPosition(self())
  setPosition(self(), x + 1, y, z)
  setShared("a_n", n)
  if n == 3 then changeScene("Nivel2") end
end
