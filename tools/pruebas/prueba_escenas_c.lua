-- ESCENA "Intro" de prueba_escenas_cine.w3s (prueba del motor): una cinematica. Su animacion la
-- arranca el motor al entrar (una ESCENA pedida con cambiarEscena se reproduce desde el inicio);
-- el script solo la mira y, cuando llega al ultimo frame, pasa al nivel.
local request = false
function start()
  setShared("c_inicios", (getShared("c_inicios") or 0) + 1)
end
function update(dt)
  local f, name = animSceneCurrent()
  if f == nil then return end
  setShared("intro_f", f)
  setShared("intro_anim", name)
  if f >= 11 and not request then
    request = true
    changeScene("Nivel2")
  end
end
