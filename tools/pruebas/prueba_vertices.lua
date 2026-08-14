-- ==========================================================================
--  prueba_vertices.lua — prueba de los VERTICES POR GRUPO desde lua (el patron
--  "agua de Crash": mover solo ciertos vertices y tenirlos, por frame).
--  La usa prueba_vertices.w3d: el quad viene de vertices-quad.obj con el
--  sidecar vertices-quad.grupos.json (grupo "punta" = los 2 verts con z=1).
--  El mapeo grupo->render se pide UNA vez en inicio() (grupoVertices); el tick
--  escribe con setVertices (batch: tabla plana {i,x,y,z,...}, un solo cruce
--  lua<->C) y setVerticeColor. Posiciones ABSOLUTAS (base + dx) para que el
--  resultado sea igual con 1 o con N ticks (determinista para el .w3s).
-- ==========================================================================
propiedades = {
    malla = "objeto",   -- referencia (ref_malla en el .w3d)
    dx = 0,             -- corrimiento en X del grupo (val_dx)
}

local idx     -- indices de RENDER del grupo (los que da grupoVertices, 1-based)
local base    -- posicion original de cada uno {x,y,z}

function inicio()
    local m = objeto("malla")
    idx = grupoVertices(m, "punta")
    base = {}
    for k = 1, #idx do
        local x, y, z = verticePos(m, idx[k])
        base[k] = { x, y, z }
    end
    setCompartido("nverts", #idx)
end

function actualizar(dt)
    local m = objeto("malla")
    if not m or not idx or #idx == 0 then return end
    local dx = propiedad("dx")
    local t = {}   -- tabla PLANA para el batch: {i1,x1,y1,z1, i2,x2,y2,z2, ...}
    for k = 1, #idx do
        local b = base[k]
        local n = #t
        t[n+1] = idx[k]; t[n+2] = b[1] + dx; t[n+3] = b[2]; t[n+4] = b[3]
        setVerticeColor(m, idx[k], 1, 0, 0)   -- la punta se tine de rojo
    end
    setVertices(m, t)
    -- verificacion: releer lo escrito (el .w3s lo chequea via compartido)
    local x = verticePos(m, idx[1])
    setCompartido("xpunta", x)
    -- smoke del bind mundo->pantalla: pantallaDe devuelve px,py (numeros, el
    -- mismo espacio de setPosPx) + bool "delante de la camara"
    local px, py, delante = pantallaDe(m)
    setCompartido("pantallaok",
        (type(px) == "number" and type(py) == "number" and type(delante) == "boolean") and 1 or 0)
end
