-- ojo_a.lua — la usa prueba_ojo_script.w3s: el script que se apaga y se prende con el ojo de la lista de scripts.
-- Si corre, lo anota en el espacio compartido (inicio y actualizar por separado).
function inicio()
    setCompartido("a", "corrio")
end

function actualizar(dt)
    setCompartido("a_act", "si")
end
