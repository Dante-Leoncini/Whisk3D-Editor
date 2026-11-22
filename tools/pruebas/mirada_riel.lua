-- ==========================================================================
--  mirada_riel.lua — el MODO DE MIRADA de la camara, desde lua.
--
--    rielDe(camara)               -> nombre, offsetRiel, nodo, miradaRiel
--    setMiradaRiel(camara, on)    prende/apaga la mirada AUTORAL del riel
--
--  Lo usa prueba_mirada_riel.w3d: el .w3d abre con el modo PRENDIDO, el script
--  lo lee, lo APAGA, lo vuelve a leer y deja las dos lecturas en el mapa
--  compartido para que el .w3s las asserte con `scriptinfo compartido`.
-- ==========================================================================
properties = {
    cam = "object",     -- ref_cam: "Camara" (la camara del riel)
}

function start()
    local cam = object and object("cam") or nil
    if not (cam and getRail and setRailLook) then return end
    local name, off, node, look = getRail(cam)
    -- sin espacios: el assert del harness (scriptinfo compartido) lee UN token
    setShared("rielNodos", (name ~= "") and "hay" or "ninguno")
    setShared("miradaAntes", look and "si" or "no")

    setRailLook(cam, false)
    local _, _, _, m2 = getRail(cam)
    setShared("miradaDespues", m2 and "si" or "no")

    -- y de vuelta a prendida, para dejar la escena como estaba
    setRailLook(cam, true)
    local _, _, _, m3 = getRail(cam)
    setShared("miradaFinal", m3 and "si" or "no")
end

function update(dt)
end
