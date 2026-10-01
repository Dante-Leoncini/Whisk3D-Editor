-- prueba_escenas_juego.w3s (prueba del motor): el objeto avanza un paso en X por tick. Lo que
-- simula es lo que queda en el CACHE de estados, y eso es lo que se renderiza de un JUEGO.
function actualizar(dt)
  local x, y, z = posicion(yo())
  setPosicion(yo(), x + 1, y, z)
end
