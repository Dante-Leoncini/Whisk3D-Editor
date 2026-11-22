-- avanza el objeto en X un paso FIJO por tick: x = x0 + paso * n (determinista:
-- no depende del dt, asi las cuentas de entrar/quedarse/salir son exactas)
-- La propiedad "y" es la ALTURA (asi se llamaba cuando lua era Y arriba): con
-- Z arriba va en la z de setPosicion.
properties = { x0 = 0, paso = 0.4, y = 0 }

local n = 0
function update(dt)
  n = n + 1
  setPosition(self(), property("x0") + property("paso") * n, 0, property("y"))
end
