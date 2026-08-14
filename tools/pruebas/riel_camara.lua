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
propiedades = {
    cam = "objeto",     -- ref_cam: "Camara"
}

function inicio()
    local cam = objeto and objeto("cam") or nil
    if not (cam and setRiel and rielDe) then return end

    -- sin espacios: el assert del harness (scriptinfo compartido) lee UN token
    local function corto(n) return (tostring(n):gsub("%s", "")) end

    local n0 = rielDe(cam)
    setCompartido("rielInicial", corto(n0))

    -- cambiar de riel POR NOMBRE, con offset
    setRiel(cam, "Riel B", 1)
    local n1, off1 = rielDe(cam)
    setCompartido("rielCambiado", corto(n1))
    setCompartido("offsetCambiado", string.format("%d", off1))

    -- plantar un nodo por INDICE y leerlo de vuelta
    setRielNodo(cam, 2)
    local _, _, nodo2 = rielDe(cam)
    setCompartido("nodoPlantado", string.format("%d", nodo2))

    -- soltar el nodo manual y volver al riel de arranque
    setRielNodo(cam, -1)
    setRiel(cam, "Riel A", 0)
    local n2, off2, nodo3 = rielDe(cam)
    setCompartido("rielFinal", corto(n2))
    setCompartido("offsetFinal", string.format("%d", off2))
    setCompartido("nodoFinal", (nodo3 < 0) and "suelto" or "plantado")
end

function actualizar(dt)
end
