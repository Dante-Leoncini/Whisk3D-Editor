-- prueba_clipsjer.w3s (prueba del motor): los CLIPS DE JERARQUIA desde lua. Una raiz SIN biblioteca propia
-- ("Bici") reproduce un clip ajeno por su nombre calificado "Biblioteca/clip" (su jerarquia tiene los mismos
-- nombres) a media velocidad, y otra raiz mas grande ("Auto.002") lo reproduce con retarget "rotaciones".
function inicio()
  local d = animObjeto(buscar("Bici"), "Auto/abrir", true)
  setCompartido("bici_dur", d ~= nil)
  setCompartido("bici_vel", animObjetoVelocidad(buscar("Bici"), 0.5))
  animRetarget(buscar("Auto.002"), "rotaciones")
  animObjeto(buscar("Auto.002"), "abrir", false)
  -- un clip que no existe: nil (sin romper el juego)
  setCompartido("nada", animObjeto(buscar("Bici"), "Auto/noexiste") == nil)
end
function actualizar(dt)
  setCompartido("bici_f", animObjetoFrame(buscar("Bici")))
end
