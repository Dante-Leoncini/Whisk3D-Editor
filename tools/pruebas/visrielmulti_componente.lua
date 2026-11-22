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
properties  = {
    cam     = "object",   -- la camara (viaja por RielVis o RielOtro)
    zona    = "object",   -- VisZona con riel declarado (recibe identificador)
    zonaSec = "object",   -- VisZona con riel declarado (SIN identificador)
}

function start()
    setShared("vrm_indice", "-1")
    setShared("vrm_riel", "")
end

function update(dt)
    local cam, zone, zoneSec = object("cam"), object("zona"), object("zonaSec")
    if not (cam and zone and getRail and setVisCurveT) then return end
    local name, _, _, _, index = getRail(cam)
    if index and index >= 0 then
        setVisCurveT(zone, index, name)          -- con identificador
        if zoneSec then setVisCurveT(zoneSec, index) end -- sin: decide la camara
    end
    setShared("vrm_indice", string.format("%.2f", index or -1))
    setShared("vrm_riel", name or "")
end
