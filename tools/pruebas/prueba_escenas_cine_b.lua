-- JUEGO "Nivel2" de prueba_escenas_cine.w3s (prueba del motor): cuenta sus ticks
local n = 0
function start()
  setShared("b_inicios", (getShared("b_inicios") or 0) + 1)
end
function update(dt)
  n = n + 1
  setShared("b_n", n)
end
