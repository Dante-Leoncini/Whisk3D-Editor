-- ==========================================================================
--  pvs_componente.lua — prueba del bind setSector(obj, s) del modificador
--  "Culling" POR TRIANGULO (PVS). Es el patron del juego real: el lua cambia
--  el sector activo cuando la camara pasa de nodo del riel; aca lo cambia en
--  inicio() para que el .w3s pueda verificarlo con pvsinfo tras simplay.
--  s es 1-based; 0 o fuera de rango = malla completa (fallback).
-- ==========================================================================
properties = {
    malla = "object",   -- referencia (ref_malla en el .w3d)
    sector = 0,         -- a que sector saltar en inicio() (val_sector)
}

function start()
    local m = object("malla")
    if m then setSector(m, property("sector")) end
    setShared("pvs_listo", 1)
end

function update(dt)
end
