# Changelog
Todos los cambios importantes seran documentados en el siguiente archivo

## Actualmente:
- Crear y Eliminar multiples objetos: Suzanne, Cube y Vertice

![photo Example](https://github.com/Dante-Leoncini/Whisk3D/blob/main/capturas/Screenshot0007.jpg?raw=true)
![photo Example](https://github.com/Dante-Leoncini/Whisk3D/blob/main/capturas/Screenshot0034.jpg?raw=true)

- Edicion basica de mallas 3d: Mover vertices, Extruir Vertices, Eliminar Vertices, Seleccionar vertices, Bordes y Caras (se esta trabajando en mas opciones)

![photo Example](https://github.com/Dante-Leoncini/Whisk3D/blob/main/capturas/Screenshot0027.jpg?raw=true)

- Editar la posicion, rotacion y escala de los objetos

![photo Example](https://github.com/Dante-Leoncini/Whisk3D/blob/main/capturas/Screenshot0026.jpg?raw=true)

- Poder editar el material, Su color (diffuse), Emission, Specular, Shader Smooth, Cambiar Textura, Cambiar la interpolacion de textura entre Closest y Lineal, Activar/desactivar transparents

![photo Example](https://github.com/Dante-Leoncini/Whisk3D/blob/main/capturas/Screenshot0014.jpg)
![photo Example](https://github.com/Dante-Leoncini/Whisk3D/blob/main/capturas/Screenshot0016.jpg?raw=true)
![photo Example](https://github.com/Dante-Leoncini/Whisk3D/blob/main/capturas/Screenshot0024.jpg)
![photo Example](https://github.com/Dante-Leoncini/Whisk3D/blob/main/capturas/Screenshot0023.jpg)
![photo Example](https://github.com/Dante-Leoncini/Whisk3D/blob/main/capturas/Screenshot0025.jpg)

- Poder entrar en Modo Object y Modo Edicion
- Proyeccion en perspectiva/orthographic, poder mover la camara a una vista superior, de perfil o frente (trabajando en mas funciones)

![photo Example](https://github.com/Dante-Leoncini/Whisk3D/blob/main/capturas/mejores%20(1).jpg?raw=true)

- Poder cambiar la vista 3d entre Material preview, Solid y Wireframe

![photo Example](https://github.com/Dante-Leoncini/Whisk3D/blob/main/capturas/Screenshot0034.jpg)
![photo Example](https://github.com/Dante-Leoncini/Whisk3D/blob/main/capturas/Screenshot0036.jpg)
![photo Example](https://github.com/Dante-Leoncini/Whisk3D/blob/main/capturas/Screenshot0038.jpg)

## Novedades - 20/06/2023:
- se quito el logo de Blender ya que tiene derechos de autor y le pertenece a la Blender Foundation.
- se arreglaron multiples errores al importar OBJ.
- Se agrego soporte de Quads y Ngones en el importador de obj
- se puede hacer renders del viewport
- se puede hacer animaciones basicas lineales
- se puede renderizar secuencias de imagenes
- es posible hacer multiples pases de render, como el zbuffer para poder usar en composicion
- se avanza en el arreglo del modo edicion, pero aun falta.
- se experimento con un modo de postprocesado por cpu. falta terminar pero parece dar buenos resultados
- se agrego un modo de render. pero las luces aun no funcionan correctamente
- se agrego "fog" para que la apariencia de la UI sea mas parecida

## Novedades - 08/05/2023:
- Ya se puede importar archivos obj!
![photo Example](https://github.com/Dante-Leoncini/Whisk3D/blob/main/capturas/importObj.png?raw=true)

-  tambien se agrego el soporte de luces, camaras y emptys.
-  Ahora se puede duplicar mallas 3d linkeadas.
-  ahora los materiales no son parte del objeto. los objetos se pueden linkear a un material como se haria en el blender original,
-  se esta trabajando en una UI.
-  se agrego el cursor 3d. actualmente se puede hacer un snap al objeto seleccionado y al crear un objeto se crea donde esta el cursor 3d (planeo agregar mas funcionalidades a futuro)
-  soporte de vertex color (hace falta desactivar la iluminacion y activarlo en el material)
   
![photo Example](https://github.com/Dante-Leoncini/Whisk3D/blob/main/capturas/vertexcolor.png?raw=true)

lo malo... se rompio el modo edicion, el agrupado de vertices y bordes en esta version. cuando este completo el importador, voy a retomar el editor de mallas 3d

![photo Example](https://github.com/Dante-Leoncini/Whisk3D/blob/main/capturas/nuevaUI.png?raw=true)