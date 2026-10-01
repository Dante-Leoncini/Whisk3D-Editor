-- prueba de prefabs: la camara del usuario toma como RIEL una curva de una instancia creada jugando; despues
-- la instancia se destruye. Rebobinar (el cache del Play) no le puede devolver a la camara la curva liberada
propiedades = { cam = "objeto" }
local n = 0
local inst = nil
function actualizar(dt)
  n = n + 1
  if n == 2 then
    inst = instanciar("RielB", 0, 0, 0)
    setCompartido("riel", tostring(setRiel(objeto("cam"), buscar("RielB", inst))))
  end
  if n == 6 then destruir(inst) end
end
