-- ==========================================================================
--  costura_grupo.lua — cuantos render-verts ve lua en un grupo que cruza una
--  COSTURA (dos control-points distintos parados en la misma posicion).
--
--  grupoVertices(malla, "costura") devuelve los indices de RENDER del grupo:
--  es la via por la que un juego mueve o tine un subconjunto de la malla. Si el
--  grupo llega incompleto desde el archivo, la mitad de la costura se queda
--  quieta y la malla se abre en dos justo por ahi.
-- ==========================================================================
properties = {
    malla = "object",   -- ref_malla
}

function start()
    local m = object("malla")
    local idx = vertexGroup(m, "costura")
    setShared("nverts", #idx)
    -- y que TODOS esten en una de las dos posiciones del grupo (x = -1 o x = 1,
    -- con y = -1 e y = 1 respectivamente; la y de lua es la profundidad, Z arriba):
    -- si entrara alguno de mas, se veria aca
    local outside = 0
    for k = 1, #idx do
        local x, y, z = vertexPos(m, idx[k])
        if not ((x < -0.9 and y < -0.9) or (x > 0.9 and y > 0.9)) then outside = outside + 1 end
    end
    setShared("fuera", outside)
end

function update(dt)
end
