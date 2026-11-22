-- prueba de prefabs: destruir() de un objeto PUESTO en la escena que un script tiene en una ref. Despues
-- objeto("objetivo") tiene que dar nil, igual que en el juego compilado
properties = { objetivo = "object" }
local n = 0
function update(dt)
  n = n + 1
  if n == 1 then destroy(object("objetivo")) end
  if n == 3 then setShared("sigue", tostring(object("objetivo") ~= nil)) end
end
