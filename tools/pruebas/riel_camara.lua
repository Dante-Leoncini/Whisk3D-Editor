-- ==========================================================================
--  riel_camara.lua — el RIEL DE LA CAMARA desde lua.
--
--    setRiel(camara, curva|nombre, offset)   cambia de riel (y su offset)
--    setRielNodo(camara, i)                  planta la camara en el nodo i
--                                            (i < 0 = volver al seguimiento)
--    rielDe(camara) -> nombre, offset, nodo, miradaRiel
--
--  Lo usa prueba_riel_camara.w3d. El script cambia de riel EN inicio(), o sea
--  adentro del motor y no desde el harness: es la prueba de que los binds
--  corren de verdad. Deja lo leido en el mapa compartido para que el .w3s lo
--  asserte con `scriptinfo compartido`.
-- ==========================================================================
properties = {
    cam = "object",     -- ref_cam: "Camara"
}

function start()
    local cam = object and object("cam") or nil
    if not (cam and setRail and getRail) then return end

    -- sin espacios: el assert del harness (scriptinfo compartido) lee UN token
    local function short(n) return (tostring(n):gsub("%s", "")) end

    local n0 = getRail(cam)
    setShared("rielInicial", short(n0))

    -- cambiar de riel POR NOMBRE, con offset
    setRail(cam, "Riel B", 1)
    local n1, off1 = getRail(cam)
    setShared("rielCambiado", short(n1))
    setShared("offsetCambiado", string.format("%d", off1))

    -- plantar un nodo por INDICE y leerlo de vuelta
    setRailNode(cam, 2)
    local _, _, node2 = getRail(cam)
    setShared("nodoPlantado", string.format("%d", node2))

    -- soltar el nodo manual y volver al riel de arranque
    setRailNode(cam, -1)
    setRail(cam, "Riel A", 0)
    local n2, off2, node3 = getRail(cam)
    setShared("rielFinal", short(n2))
    setShared("offsetFinal", string.format("%d", off2))
    setShared("nodoFinal", (node3 < 0) and "suelto" or "plantado")
end

function update(dt)
end
