-- ESCENA "Intro" de prueba_escenas_cine.w3s (prueba del motor): una cinematica. Su animacion la
-- arranca el motor al entrar (una ESCENA pedida con cambiarEscena se reproduce desde el inicio);
-- el script solo la mira y, cuando llega al ultimo frame, pasa al nivel.
local pedido = false
function inicio()
  setCompartido("c_inicios", (compartido("c_inicios") or 0) + 1)
end
function actualizar(dt)
  local f, nombre = animEscenaActual()
  if f == nil then return end
  setCompartido("intro_f", f)
  setCompartido("intro_anim", nombre)
  if f >= 11 and not pedido then
    pedido = true
    cambiarEscena("Nivel2")
  end
end
