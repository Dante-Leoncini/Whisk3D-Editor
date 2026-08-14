-- ==========================================================================
--  visrielmulti_componente.lua — el cable de la visibilidad por celda con
--  VARIOS RIELES. Igual que visriel_componente.lua, pero:
--
--    * a Zona le pasa el IDENTIFICADOR del riel del que salio el indice:
--          setVisCurvaT(zona, t, nombreRiel)
--      (el 1er valor de rielDe(cam) ES ese nombre). Si la zona declara
--      `riel:` y no coincide, cae a su fallback.
--    * a ZonaSec le pasa el indice SIN identificador: ahi la evidencia es el
--      riel de la Camera ACTIVA (el motor lo mira solo).
--
--  Es exactamente lo que un juego con varios rieles (pueblo/elevador/bonus)
--  tiene que hacer: pasar el indice SIEMPRE, venga del riel que venga; el
--  motor decide si aplica la playlist o el fallback declarado.
--  Deja nombre e indice leidos en el mapa compartido para los asserts.
-- ==========================================================================
propiedades = {
    cam     = "objeto",   -- la camara (viaja por RielVis o RielOtro)
    zona    = "objeto",   -- VisZona con riel declarado (recibe identificador)
    zonaSec = "objeto",   -- VisZona con riel declarado (SIN identificador)
}

function inicio()
    setCompartido("vrm_indice", "-1")
    setCompartido("vrm_riel", "")
end

function actualizar(dt)
    local cam, zona, zonaSec = objeto("cam"), objeto("zona"), objeto("zonaSec")
    if not (cam and zona and rielDe and setVisCurvaT) then return end
    local nombre, _, _, _, indice = rielDe(cam)
    if indice and indice >= 0 then
        setVisCurvaT(zona, indice, nombre)          -- con identificador
        if zonaSec then setVisCurvaT(zonaSec, indice) end -- sin: decide la camara
    end
    setCompartido("vrm_indice", string.format("%.2f", indice or -1))
    setCompartido("vrm_riel", nombre or "")
end
