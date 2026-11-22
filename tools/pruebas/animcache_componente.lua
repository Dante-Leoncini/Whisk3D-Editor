-- ==========================================================================
--  animcache_componente.lua — prueba de los FRAMES COMPARTIDOS de las vertex
--  anims (AnimDatos). AniA y AniB declaran la MISMA Animation { basePath }:
--  comparten UN juego de frames, pero la POSE es por instancia (se escribe en
--  mesh->vertex con COW). El script posa SOLO AniA en el cuadro desplazado y
--  verifica que AniB no se movio. Flags 0/1 via setCompartido.
--  La pose "mov" sube los vertices: se mira la ALTURA, que en lua es la z
--  (Z arriba; el tercer valor de verticePos).
-- ==========================================================================
properties = {
    a = "object",   -- la instancia que se posa (ref_a)
    b = "object",   -- la instancia testigo (ref_b)
}

local tick = 0
local z0 = nil

function start()
    setShared("ac_iguales", -1)
    setShared("ac_movida", -1)
    setShared("ac_intacta", -1)
end

function update(dt)
    tick = tick + 1
    local A = object("a")
    local B = object("b")
    if not A or not B then return end
    if tick == 1 then
        local _, _, za = vertexPos(A, 1)
        local _, _, zb = vertexPos(B, 1)
        z0 = zb
        setShared("ac_iguales", (za == zb) and 1 or 0)
        animate(A, 1, 1)   -- posa SOLO AniA en el clip "mov" (la pose +2)
    elseif tick == 2 then
        local _, _, za = vertexPos(A, 1)
        local _, _, zb = vertexPos(B, 1)
        setShared("ac_movida", (za ~= z0) and 1 or 0)
        setShared("ac_intacta", (zb == z0) and 1 or 0)
    end
end
