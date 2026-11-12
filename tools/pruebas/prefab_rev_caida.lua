-- prueba de prefabs: lo instanciado JUGANDO arranca con sus cuerpos rigidos creados ANTES de su inicio() (el
-- caja.lua del prefab hace fisicaVel(yo(), 0, 0, -4) en inicio(): sin el cuerpo se perdia y la caja, que nace
-- dormida, quedaba quieta)
local n = 0
local c = nil
function actualizar(dt)
  n = n + 1
  if n == 1 then c = instanciar("Caja", 10, 0, 5) end   -- a 5 de altura (Z arriba)
  if n == 3 then
    local cubo = c and buscar("Caja", c) or nil
    local vx, vy, vz = nil, nil, nil
    if cubo then vx, vy, vz = fisicaVel(cubo) end
    setCompartido("caida", (vz ~= nil and vz < -3) and "cae" or "quieta")   -- vz: la vertical
  end
end
