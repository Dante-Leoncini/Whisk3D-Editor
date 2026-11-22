-- ojo_b.lua — la usa prueba_ojo_script.w3s: el testigo (siempre prendido). Copia lo que anoto ojo_a.lua, o "no".
function update(dt)
    setShared("vio_a", getShared("a") or "no")
end
