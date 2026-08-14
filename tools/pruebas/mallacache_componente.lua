-- ==========================================================================
--  mallacache_componente.lua — prueba del COPY-ON-WRITE de la geometria
--  compartida (MallaDatos.h). Caja1 y Caja2 importaron el MISMO .obj, o sea
--  que comparten vertex[]: este script mueve UN vertice de Caja1 con
--  setVerticePos (que desinstancia por COW) y verifica que Caja2 NO se movio.
--  Comunica los resultados como flags 0/1 via setCompartido; el .w3s los
--  aserta con `scriptinfo compartido <clave> <valor>`.
-- ==========================================================================
propiedades = {
    caja1 = "objeto",   -- la caja que se edita (ref_caja1)
    caja2 = "objeto",   -- la caja testigo, importada del mismo .obj (ref_caja2)
}

local tick = 0
local y0 = nil   -- y original del vertice 1 de la caja testigo

function inicio()
    setCompartido("mc_iguales", -1)
    setCompartido("mc_movida", -1)
    setCompartido("mc_intacta", -1)
end

function actualizar(dt)
    tick = tick + 1
    local c1 = objeto("caja1")
    local c2 = objeto("caja2")
    if not c1 or not c2 then return end
    if tick == 1 then
        -- mismas rutas = mismos datos: el vertice 1 de las dos cajas coincide
        local x1, y1, z1 = verticePos(c1, 1)
        local x2, y2, z2 = verticePos(c2, 1)
        y0 = y2
        setCompartido("mc_iguales", (x1 == x2 and y1 == y2 and z1 == z2) and 1 or 0)
        -- COW: esta escritura desinstancia SOLO la capa de posiciones de Caja1
        setVerticePos(c1, 1, 9.0, 9.0, 9.0)
    elseif tick == 2 then
        local x1, y1, z1 = verticePos(c1, 1)
        local _, y2, _ = verticePos(c2, 1)
        setCompartido("mc_movida", (x1 == 9.0 and y1 == 9.0 and z1 == 9.0) and 1 or 0)
        setCompartido("mc_intacta", (y2 == y0) and 1 or 0)
    end
end
