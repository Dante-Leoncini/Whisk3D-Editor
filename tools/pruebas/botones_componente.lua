-- botones_componente.lua — la usa prueba_botones_script.w3s: dos BOTONES en las propiedades (una funcion en
-- `propiedades`). "Subir lo elegido" es un complemento del editor (anda sin el juego: sube 1 m, en Z, lo elegido);
-- "Volver a cero" pone la rotacion en 0 (con el juego andando, en la instancia viva). "Punta arriba" mueve el primer
-- vertice de la malla (un complemento que toca la malla: Ctrl+Z la devuelve). "Poner un bloque" y "Borrar el bloque"
-- crean y borran (instanciar / destruir): con el juego parado, tambien se deshacen.
propiedades = {
  velocidad = 90,
  subir = { "Subir lo elegido", function()
    for _, o in ipairs(seleccionados()) do mover(o, 0, 0, 1) end
  end },
  cero = { "Volver a cero", function()
    setRotacion(yo(), 0, 0, 0)
    setCompartido("boton", "apretado")
  end },
  punta = { "Punta arriba", function()
    setVerticePos(yo(), 1, 0, 0, 3)
  end },
  poner = { "Poner un bloque", function() instanciar("Bloque", 3, 0, 0) end },
  borrar = { "Borrar el bloque", function() destruir(buscar("Bloque")) end },
}

function actualizar(dt)
  setRotacionZ(yo(), rotacionZ(yo()) + propiedad("velocidad") * dt)
end
