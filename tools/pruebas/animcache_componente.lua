-- ==========================================================================
--  animcache_componente.lua — prueba de los FRAMES COMPARTIDOS de las vertex
--  anims (AnimDatos). AniA y AniB declaran la MISMA Animation { basePath }:
--  comparten UN juego de frames, pero la POSE es por instancia (se escribe en
--  mesh->vertex con COW). El script posa SOLO AniA en el cuadro desplazado y
--  verifica que AniB no se movio. Flags 0/1 via setCompartido.
-- ==========================================================================
propiedades = {
    a = "objeto",   -- la instancia que se posa (ref_a)
    b = "objeto",   -- la instancia testigo (ref_b)
}

local tick = 0
local y0 = nil

function inicio()
    setCompartido("ac_iguales", -1)
    setCompartido("ac_movida", -1)
    setCompartido("ac_intacta", -1)
end

function actualizar(dt)
    tick = tick + 1
    local A = objeto("a")
    local B = objeto("b")
    if not A or not B then return end
    if tick == 1 then
        local _, ya, _ = verticePos(A, 1)
        local _, yb, _ = verticePos(B, 1)
        y0 = yb
        setCompartido("ac_iguales", (ya == yb) and 1 or 0)
        animar(A, 1, 1)   -- posa SOLO AniA en el clip "mov" (la pose +2)
    elseif tick == 2 then
        local _, ya, _ = verticePos(A, 1)
        local _, yb, _ = verticePos(B, 1)
        setCompartido("ac_movida", (ya ~= y0) and 1 or 0)
        setCompartido("ac_intacta", (yb == y0) and 1 or 0)
    end
end
