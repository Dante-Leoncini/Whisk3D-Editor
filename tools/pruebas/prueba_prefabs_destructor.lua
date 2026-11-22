-- prueba de prefabs: JUGANDO, destruye un enemigo PUESTO en la escena y crea otro (el Stop del editor tiene que
-- devolver la escena como estaba: el destruido vuelve y el creado se va). El puntero que lua se guardo del
-- destruido ya no es un objeto (tipo() = ""), igual que en el juego compilado
local n = 0
local dead = nil
function update(dt)
  n = n + 1
  if n == 3 then
    dead = find("Enemigo.001")
    destroy(dead)
    local c = instantiate("Enemigo", 0, 10, 0, 45)
    setShared("creado", getName(c))
  end
  if n == 5 then
    setShared("destruido", (dead ~= nil and getType(dead) == "") and "si" or "no")
  end
end
