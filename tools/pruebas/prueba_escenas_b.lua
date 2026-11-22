-- escena B ("Nivel2") de prueba_escenas_play.w3s: sube su objeto un paso por tick y, la
-- PRIMERA vez que corre, al 4to tick vuelve a la escena del bloque ("Scene")
local n = 0
function start()
  setShared("b_inicios", (getShared("b_inicios") or 0) + 1)
end
function update(dt)
  n = n + 1
  local x, y, z = getPosition(self())
  setPosition(self(), x, y, z + 1)   -- sube: la z (Z arriba)
  setShared("b_n", n)
  if n == 4 and (getShared("b_inicios") or 0) < 2 then changeScene("Scene") end
end
