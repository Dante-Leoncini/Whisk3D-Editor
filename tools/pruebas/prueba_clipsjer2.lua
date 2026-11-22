-- prueba_clipsjer2.w3s (prueba del motor): las CAPAS de clips de jerarquia desde lua. La raiz "Auto" mezcla su
-- clip "Clip" entero (sube la raiz y gira la puerta) y le SUMA "Giro" a la mitad (gira el chasis). La "Moto" (sin
-- biblioteca propia) mezcla el clip ajeno "Auto/Clip" al 50% en su unica capa.
function start()
  local a = find("Auto")
  objectLayer(a, 1, "Clip", 1, "mix", false)
  objectLayer(a, 2, "Giro", 0.5, "add", false)
  setShared("capas", objectLayers(a))
  setShared("ajeno", objectLayer(find("Moto"), 1, "Auto/Clip", 0.5, "mix", false))
  setShared("nada", objectLayer(find("Moto"), 2, "Auto/noexiste") == false)
  -- (el retarget propio tambien es estado de la partida: el Stop lo devuelve)
  animRetarget(find("Farol"), "rotations")
end
function update(dt)
  local a = find("Auto")
  setShared("f1", objectLayerFrame(a, 1))
  setShared("peso2", objectLayerWeight(a, 2))
  setShared("termino1", objectLayerFinished(a, 1))
end
