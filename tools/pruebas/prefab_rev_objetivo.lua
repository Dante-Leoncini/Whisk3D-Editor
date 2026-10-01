-- prueba de prefabs: destruir() de un objeto PUESTO en la escena que un script tiene en una ref. Despues
-- objeto("objetivo") tiene que dar nil, igual que en el juego compilado
propiedades = { objetivo = "objeto" }
local n = 0
function actualizar(dt)
  n = n + 1
  if n == 1 then destruir(objeto("objetivo")) end
  if n == 3 then setCompartido("sigue", tostring(objeto("objetivo") ~= nil)) end
end
