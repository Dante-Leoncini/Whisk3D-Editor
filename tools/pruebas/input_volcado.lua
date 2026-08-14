-- ==========================================================================
--  input_volcado.lua — vuelca el INPUT que ve el script al mapa compartido.
--
--  Lo usa prueba_input.w3s: el harness mueve la cruceta / el stick / las
--  flechas del teclado con `pad ...`, corre un tick con `simplay 1` y despues
--  asserta lo que el SCRIPT vio, con `scriptinfo compartido <clave> <valor>`.
--  O sea: no verifica el estado interno del motor, verifica que el valor haya
--  llegado hasta lua, que es el contrato que le importa a un juego.
--
--  Los valores se guardan como texto sin espacios porque el assert del harness
--  compara UN token.
-- ==========================================================================

local function txt(n)
    -- -1 / 0 / 1 salen sin decimales; el analogico sale con 3
    if n == math.floor(n) then return string.format("%d", n) end
    return string.format("%.3f", n)
end

function inicio()
    actualizar(0)
end

function actualizar(dt)
    local x, y = stick("izq")
    setCompartido("ejeX", txt(x))
    setCompartido("ejeY", txt(y))
    -- las cuatro direcciones POR NOMBRE (la cruceta y las flechas entran por el
    -- mismo camino que los botones con nombre)
    setCompartido("bArriba",     boton("arriba")     and "si" or "no")
    setCompartido("bAbajo",      boton("abajo")      and "si" or "no")
    setCompartido("bIzquierda",  boton("izquierda")  and "si" or "no")
    setCompartido("bDerecha",    boton("derecha")    and "si" or "no")
    -- y un boton de accion cualquiera, para que se vea que no es un caso aparte
    setCompartido("bA", boton("a") and "si" or "no")
end
