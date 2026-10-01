-- JUEGO "Scene" de prueba_escenas_cine.w3s (prueba del motor): a los 3 ticks pasa a la cinematica
local n = 0
function actualizar(dt)
  n = n + 1
  setCompartido("a_n", n)
  if n == 3 then cambiarEscena("Intro") end
end
