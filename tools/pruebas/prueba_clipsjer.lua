-- prueba_clipsjer.w3s (prueba del motor): los CLIPS DE JERARQUIA desde lua. Una raiz SIN biblioteca propia
-- ("Bici") reproduce un clip ajeno por su nombre calificado "Biblioteca/clip" (su jerarquia tiene los mismos
-- nombres) a media velocidad, y otra raiz mas grande ("Auto.002") lo reproduce con retarget "rotaciones".
function start()
  local d = animObject(find("Bici"), "Auto/abrir", true)
  setShared("bici_dur", d ~= nil)
  setShared("bici_vel", animObjectSpeed(find("Bici"), 0.5))
  animRetarget(find("Auto.002"), "rotations")
  animObject(find("Auto.002"), "abrir", false)
  -- un clip que no existe: nil (sin romper el juego)
  setShared("nada", animObject(find("Bici"), "Auto/noexiste") == nil)
end
function update(dt)
  setShared("bici_f", animObjectFrame(find("Bici")))
end
