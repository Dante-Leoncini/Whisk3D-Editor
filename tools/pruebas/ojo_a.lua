-- ojo_a.lua — la usa prueba_ojo_script.w3s: el script que se apaga y se prende con el ojo de la lista de scripts.
-- Si corre, lo anota en el espacio compartido (inicio y actualizar por separado).
function start()
    setShared("a", "corrio")
end

function update(dt)
    setShared("a_act", "si")
end
