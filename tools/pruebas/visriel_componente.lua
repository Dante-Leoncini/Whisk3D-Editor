-- ==========================================================================
--  visriel_componente.lua — el CABLE ENTERO de la visibilidad por celda:
--
--      camara sobre su riel  ->  rielDe(cam) 5to valor (indice FRACCIONARIO
--      del nodo, lo publica el motor con el frame)  ->  setVisCurvaT(zona, t)
--      ->  la VisZona modo curva avanza DE A UNA celda por tick  ->  la celda
--      recorta los triangulos de la malla (playlist del `.w3dvis`).
--
--  Es exactamente lo que un juego tiene que hacer para que la lista de
--  visibilidad POR NODO gobierne el dibujo: UNA linea por tick.
--  Deja el indice leido en el mapa compartido para los asserts del .w3s.
-- ==========================================================================
properties = {
    cam  = "object",   -- la camara que viaja por el riel
    zona = "object",   -- la VisZona modo curva (objetivo: la malla con celdas)
}

function start()
    setShared("vr_indice", "-1")
end

function update(dt)
    local cam, zone = object("cam"), object("zona")
    if not (cam and zone and getRail and setVisCurveT) then return end
    local _, _, _, _, index = getRail(cam)
    if index and index >= 0 then setVisCurveT(zone, index) end
    setShared("vr_indice", string.format("%.2f", index or -1))
end
