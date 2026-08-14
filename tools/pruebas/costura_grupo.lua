-- ==========================================================================
--  costura_grupo.lua — cuantos render-verts ve lua en un grupo que cruza una
--  COSTURA (dos control-points distintos parados en la misma posicion).
--
--  grupoVertices(malla, "costura") devuelve los indices de RENDER del grupo:
--  es la via por la que un juego mueve o tine un subconjunto de la malla. Si el
--  grupo llega incompleto desde el archivo, la mitad de la costura se queda
--  quieta y la malla se abre en dos justo por ahi.
-- ==========================================================================
propiedades = {
    malla = "objeto",   -- ref_malla
}

function inicio()
    local m = objeto("malla")
    local idx = grupoVertices(m, "costura")
    setCompartido("nverts", #idx)
    -- y que TODOS esten en una de las dos posiciones del grupo (x = -1 o x = 1,
    -- con z = -1 y z = 1 respectivamente): si entrara alguno de mas, se veria aca
    local fuera = 0
    for k = 1, #idx do
        local x, y, z = verticePos(m, idx[k])
        if not ((x < -0.9 and z < -0.9) or (x > 0.9 and z > 0.9)) then fuera = fuera + 1 end
    end
    setCompartido("fuera", fuera)
end

function actualizar(dt)
end
