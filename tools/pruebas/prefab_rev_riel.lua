-- prueba de prefabs: la camara del usuario toma como RIEL una curva de una instancia creada jugando; despues
-- la instancia se destruye. Rebobinar (el cache del Play) no le puede devolver a la camara la curva liberada
properties = { cam = "object" }
local n = 0
local inst = nil
function update(dt)
  n = n + 1
  if n == 2 then
    inst = instantiate("RielB", 0, 0, 0)
    setShared("riel", tostring(setRail(object("cam"), find("RielB", inst))))
  end
  if n == 6 then destroy(inst) end
end
