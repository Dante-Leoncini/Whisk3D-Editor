-- ==========================================================================
--  opcion_componente.lua — prueba de las listas desplegables de `propiedades`
--  (la de dificultad del manual de Lua). La usa prueba_opcion_desplegable.w3s:
--  dos objetos con este MISMO script; uno eligio "dificil" en el editor
--  (opt_dificultad) y el otro no eligio nada. opcion() tiene que devolver lo
--  elegido, y si no se eligio nada, la DE FABRICA (la primera de la lista),
--  la misma que muestra el panel de Propiedades. Antes devolvia "".
-- ==========================================================================
propiedades = {
    id = 0,                                         -- cual de los dos objetos (val_id)
    dificultad = { "facil", "normal", "dificil" },  -- una lista desplegable (la primera es la de fabrica)
}

function inicio()
    setCompartido("dificultad" .. propiedad("id"), opcion("dificultad"))
    -- una opcion que no esta declarada como lista: "" (como siempre)
    setCompartido("sinlista", "[" .. opcion("noexiste") .. "]")
end
