-- JUEGO "Scene" de prueba_escenas_cine.w3s (prueba del motor): a los 3 ticks pasa a la cinematica
local n = 0
function update(dt)
  n = n + 1
  setShared("a_n", n)
  if n == 3 then changeScene("Intro") end
end
