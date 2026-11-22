-- ==========================================================================
--  Los BINDS de hitbox, tick a tick, desde el script del propio hitbox:
--    tick 12: hitboxDentro / hitboxTocando / hitboxActivo (de el y de un objeto
--             con hitbox HIJOS) y lo APAGA -> el tick 13 tiene que dar alSalir
--    tick 14: lo prende -> el tick 15 da alEntrar otra vez
--    tick 20: lo agranda (20 de ancho) -> el tick 21 vuelve a entrar el movil
-- ==========================================================================
local t = 0
local function b(v) if v then return "si" end return "no" end

function update(dt)
  t = t + 1
  local mine = self()
  if t == 12 then
    local d = hitboxInside(mine)
    setShared("dentro12", #d)
    if d[1] then setShared("dentroNombre12", getName(d[1])) else setShared("dentroNombre12", "nil") end
    setShared("tocaMovil12", b(hitboxTouching(mine, find("Movil"))))
    setShared("tocaCaja12", b(hitboxTouching(mine, find("CajaMovil"))))
    setShared("tocaLejos12", b(hitboxTouching(mine, find("Lejos"))))
    local dm = hitboxInside(find("Movil"))
    setShared("movilDentro12", #dm)
    setShared("movilActivo12", b(hitboxActive(find("Movil"))))
    setHitboxActive(mine, false)
    setShared("activo12", b(hitboxActive(mine)))
  elseif t == 14 then
    setHitboxActive(mine, true)
  elseif t == 20 then
    setHitboxSize(mine, 20, 2, 2)
  end
end
