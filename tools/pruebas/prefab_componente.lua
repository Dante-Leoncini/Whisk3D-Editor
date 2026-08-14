-- ==========================================================================
--  prefab_componente.lua — script DEL PREFAB (viaja con el subarbol de la
--  Biblioteca). Cada clon corre su propia instancia; `etiqueta` es un valor
--  editable por instancia (val_etiqueta en el Clon lo pisa). El script marca
--  su presencia: setCompartido("pf_<etiqueta>", 1) -> el .w3s verifica que el
--  default y el override conviven en clones distintos del MISMO prefab.
-- ==========================================================================
propiedades = {
    etiqueta = "sin",
}

function inicio()
    setCompartido("pf_" .. propiedad("etiqueta"), 1)
end

function actualizar(dt)
end
