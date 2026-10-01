-- prueba de prefabs: JUGANDO, destruye un enemigo PUESTO en la escena y crea otro (el Stop del editor tiene que
-- devolver la escena como estaba: el destruido vuelve y el creado se va). El puntero que lua se guardo del
-- destruido ya no es un objeto (tipo() = ""), igual que en el juego compilado
local n = 0
local muerto = nil
function actualizar(dt)
  n = n + 1
  if n == 3 then
    muerto = buscar("Enemigo.001")
    destruir(muerto)
    local c = instanciar("Enemigo", 0, 0, 10, 45)
    setCompartido("creado", nombre(c))
  end
  if n == 5 then
    setCompartido("destruido", (muerto ~= nil and tipo(muerto) == "") and "si" or "no")
  end
end
