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
propiedades = {
    cam = "objeto",     -- ref_cam: "Camara" (la camara del riel)
}

function inicio()
    local cam = objeto and objeto("cam") or nil
    if not (cam and rielDe and setMiradaRiel) then return end
    local nombre, off, nodo, mirada = rielDe(cam)
    -- sin espacios: el assert del harness (scriptinfo compartido) lee UN token
    setCompartido("rielNodos", (nombre ~= "") and "hay" or "ninguno")
    setCompartido("miradaAntes", mirada and "si" or "no")

    setMiradaRiel(cam, false)
    local _, _, _, m2 = rielDe(cam)
    setCompartido("miradaDespues", m2 and "si" or "no")

    -- y de vuelta a prendida, para dejar la escena como estaba
    setMiradaRiel(cam, true)
    local _, _, _, m3 = rielDe(cam)
    setCompartido("miradaFinal", m3 and "si" or "no")
end

function actualizar(dt)
end
