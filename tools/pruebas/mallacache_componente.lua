-- ==========================================================================
--  mallacache_componente.lua — prueba del COPY-ON-WRITE de la geometria
--  compartida (MallaDatos.h). Caja1 y Caja2 importaron el MISMO .obj, o sea
--  que comparten vertex[]: este script mueve UN vertice de Caja1 con
--  setVerticePos (que desinstancia por COW) y verifica que Caja2 NO se movio.
--  Comunica los resultados como flags 0/1 via setCompartido; el .w3s los
--  aserta con `scriptinfo compartido <clave> <valor>`.
-- ==========================================================================
properties = {
    caja1 = "object",   -- la caja que se edita (ref_caja1)
    caja2 = "object",   -- la caja testigo, importada del mismo .obj (ref_caja2)
}

local tick = 0
local z0 = nil   -- z (altura) original del vertice 1 de la caja testigo

function start()
    setShared("mc_iguales", -1)
    setShared("mc_movida", -1)
    setShared("mc_intacta", -1)
end

function update(dt)
    tick = tick + 1
    local c1 = object("caja1")
    local c2 = object("caja2")
    if not c1 or not c2 then return end
    if tick == 1 then
        -- mismas rutas = mismos datos: el vertice 1 de las dos cajas coincide
        local x1, y1, z1 = vertexPos(c1, 1)
        local x2, y2, z2 = vertexPos(c2, 1)
        z0 = z2
        setShared("mc_iguales", (x1 == x2 and y1 == y2 and z1 == z2) and 1 or 0)
        -- COW: esta escritura desinstancia SOLO la capa de posiciones de Caja1
        setVertexPos(c1, 1, 9.0, 9.0, 9.0)
    elseif tick == 2 then
        local x1, y1, z1 = vertexPos(c1, 1)
        local _, _, z2 = vertexPos(c2, 1)
        setShared("mc_movida", (x1 == 9.0 and y1 == 9.0 and z1 == 9.0) and 1 or 0)
        setShared("mc_intacta", (z2 == z0) and 1 or 0)
    end
end
