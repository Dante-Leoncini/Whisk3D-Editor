-- ==========================================================================
--  inspector_componente.lua — prueba del "inspector" en vivo: un cubo que gira con la velocidad de `propiedades`,
--  leida en CADA cuadro con propiedad(). La usa prueba_inspector_en_vivo.w3s: con el juego andando cambia la
--  velocidad como lo hace el panel de Propiedades (scriptval) y el giro tiene que cambiar en ese momento.
-- ==========================================================================
properties = {
    velocidad = 90,   -- grados por segundo
}

function update(dt)
    local x, y, z = getRotation(self())
    z = z + property("velocidad") * dt
    setRotation(self(), x, y, z)
    setShared("giro", string.format("%.0f", z))
end
