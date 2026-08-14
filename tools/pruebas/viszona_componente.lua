-- ==========================================================================
--  viszona_componente.lua — prueba de los binds de VISIBILIDAD POR CELDA:
--  setVisCurvaT(zona, t) entrega el indice FRACCIONARIO del nodo del riel
--  (como hara el juego con el ancla de camara que ya calcula) y la VisZona
--  en modo curva avanza DE A UNA celda por tick hacia ese objetivo (el
--  stepper del original). visCelda(obj) lee la celda activa: de la malla
--  (sector del modificador) o de la zona (celdaActual).
--  El orden dentro del tick es: scripts -> W3dVisZonasTick, asi que lo que
--  este script LEE en el tick N es lo aplicado al final del tick N-1.
-- ==========================================================================
propiedades = {
    malla = "objeto",   -- la malla con CullingTri metodo celdas (ref_malla)
    zona  = "objeto",   -- la VisZona modo curva (ref_zona)
}

local tick = 0

function inicio()
    setCompartido("vz_celda", -1)
    setCompartido("vz_zona", -1)
end

function actualizar(dt)
    tick = tick + 1
    local z = objeto("zona")
    local m = objeto("malla")
    if not z or not m then return end
    if tick == 1 then setVisCurvaT(z, 0.0) end   -- nodo 0 -> celda 1
    if tick == 3 then setVisCurvaT(z, 2.9) end   -- nodo 2.9 -> celda 3 (de a 1 por tick)
    setCompartido("vz_celda", visCelda(m) or -1)
    setCompartido("vz_zona",  visCelda(z) or -1)
    setCompartido("vz_tick", tick)
end
