-- JUEGO "Nivel2" de prueba_escenas_cine.w3s (prueba del motor): cuenta sus ticks
local n = 0
function inicio()
  setCompartido("b_inicios", (compartido("b_inicios") or 0) + 1)
end
function actualizar(dt)
  n = n + 1
  setCompartido("b_n", n)
end
