-- avanza el objeto en X un paso FIJO por tick: x = x0 + paso * n (determinista:
-- no depende del dt, asi las cuentas de entrar/quedarse/salir son exactas)
-- La propiedad "y" es la ALTURA (asi se llamaba cuando lua era Y arriba): con
-- Z arriba va en la z de setPosicion.
propiedades = { x0 = 0, paso = 0.4, y = 0 }

local n = 0
function actualizar(dt)
  n = n + 1
  setPosicion(yo(), propiedad("x0") + propiedad("paso") * n, 0, propiedad("y"))
end
