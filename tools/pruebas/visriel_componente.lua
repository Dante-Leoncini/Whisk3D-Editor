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
propiedades = {
    cam  = "objeto",   -- la camara que viaja por el riel
    zona = "objeto",   -- la VisZona modo curva (objetivo: la malla con celdas)
}

function inicio()
    setCompartido("vr_indice", "-1")
end

function actualizar(dt)
    local cam, zona = objeto("cam"), objeto("zona")
    if not (cam and zona and rielDe and setVisCurvaT) then return end
    local _, _, _, _, indice = rielDe(cam)
    if indice and indice >= 0 then setVisCurvaT(zona, indice) end
    setCompartido("vr_indice", string.format("%.2f", indice or -1))
end
