-- ==========================================================================
--  tarjeta_componente.lua — la usa prueba_tarjeta_script.w3s: una sola propiedad, una lista desplegable, para que
--  sea la ULTIMA fila de la tarjeta del script en Propiedades (las filas: Archivo y dificultad).
-- ==========================================================================
propiedades = {
    dificultad = { "facil", "normal", "dificil" },
}

function inicio()
    setCompartido("dificultad", opcion("dificultad"))
end
