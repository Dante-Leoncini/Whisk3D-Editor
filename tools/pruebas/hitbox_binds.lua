-- ==========================================================================
--  Los BINDS de hitbox, tick a tick, desde el script del propio hitbox:
--    tick 12: hitboxDentro / hitboxTocando / hitboxActivo (de el y de un objeto
--             con hitbox HIJOS) y lo APAGA -> el tick 13 tiene que dar alSalir
--    tick 14: lo prende -> el tick 15 da alEntrar otra vez
--    tick 20: lo agranda (20 de ancho) -> el tick 21 vuelve a entrar el movil
-- ==========================================================================
local t = 0
local function b(v) if v then return "si" end return "no" end

function actualizar(dt)
  t = t + 1
  local mio = yo()
  if t == 12 then
    local d = hitboxDentro(mio)
    setCompartido("dentro12", #d)
    if d[1] then setCompartido("dentroNombre12", nombre(d[1])) else setCompartido("dentroNombre12", "nil") end
    setCompartido("tocaMovil12", b(hitboxTocando(mio, buscar("Movil"))))
    setCompartido("tocaCaja12", b(hitboxTocando(mio, buscar("CajaMovil"))))
    setCompartido("tocaLejos12", b(hitboxTocando(mio, buscar("Lejos"))))
    local dm = hitboxDentro(buscar("Movil"))
    setCompartido("movilDentro12", #dm)
    setCompartido("movilActivo12", b(hitboxActivo(buscar("Movil"))))
    setHitboxActivo(mio, false)
    setCompartido("activo12", b(hitboxActivo(mio)))
  elseif t == 14 then
    setHitboxActivo(mio, true)
  elseif t == 20 then
    setHitboxTam(mio, 20, 2, 2)
  end
end
