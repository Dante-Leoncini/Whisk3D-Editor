# Física

Todo hecho en casa, en C++03, sin motor externo y sin pedir memoria en cada cuadro. Si nadie la usa, no cuesta nada: cada paso sale en la primera línea.

- **W3dFisica:** velocidad y rebote de cajas alineadas a los ejes. Nada más.
- **W3dRigido:** cajas orientadas con masa, gravedad, impulsos con fricción y rebote, raycast y eventos de contacto.
- **W3dColMalla:** caminar sobre los triángulos de un escenario (rampas, escalones) y que las paredes frenen, con una grilla para no revisar miles de triángulos.
- **W3dHitbox:** cajas que solo avisan (`alEntrar`, `alQuedarse`, `alSalir`, `alTocar`).
- **W3dRagdoll:** muñecos de trapo, y cinemática inversa de dos huesos y FABRIK.
- **W3dNav:** el camino de un punto a otro sobre una rejilla del piso (A* y después tensado).

Por qué importa: el Play del editor y el juego compilado llaman al mismo paso con el mismo `dt`, así que el juego se comporta igual en los dos, en la PC y en el N95. Desde Lua: [Física simple](../lua/api-fisica-simple.md) y [Cuerpos rígidos, choques y hitboxes](../lua/api-cuerpos-rigidos.md).
