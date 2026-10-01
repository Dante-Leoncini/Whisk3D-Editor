-- prueba_clipsjer2.w3s (prueba del motor): las CAPAS de clips de jerarquia desde lua. La raiz "Auto" mezcla su
-- clip "Clip" entero (sube la raiz y gira la puerta) y le SUMA "Giro" a la mitad (gira el chasis). La "Moto" (sin
-- biblioteca propia) mezcla el clip ajeno "Auto/Clip" al 50% en su unica capa.
function inicio()
  local a = buscar("Auto")
  objetoCapa(a, 1, "Clip", 1, "mezclar", false)
  objetoCapa(a, 2, "Giro", 0.5, "sumar", false)
  setCompartido("capas", objetoCapas(a))
  setCompartido("ajeno", objetoCapa(buscar("Moto"), 1, "Auto/Clip", 0.5, "mezclar", false))
  setCompartido("nada", objetoCapa(buscar("Moto"), 2, "Auto/noexiste") == false)
  -- (el retarget propio tambien es estado de la partida: el Stop lo devuelve)
  animRetarget(buscar("Farol"), "rotaciones")
end
function actualizar(dt)
  local a = buscar("Auto")
  setCompartido("f1", objetoCapaFrame(a, 1))
  setCompartido("peso2", objetoCapaPeso(a, 2))
  setCompartido("termino1", objetoCapaTermino(a, 1))
end
