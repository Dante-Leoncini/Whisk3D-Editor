-- ojo_b.lua — la usa prueba_ojo_script.w3s: el testigo (siempre prendido). Copia lo que anoto ojo_a.lua, o "no".
function actualizar(dt)
    setCompartido("vio_a", compartido("a") or "no")
end
