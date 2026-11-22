-- rotacion_eje.lua — la usa prueba_rotacion_eje.w3s: rotacionX/Y/Z y setRotacionX/Y/Z (un eje solo, ejes de Lua:
-- Z arriba). Pone (10, 20, 0) con setRotacion, cambia SOLO la Z (la vertical) a 30 y anota lo que leen las tres.
function start()
    setRotation(self(), 10, 20, 0)
    setRotationZ(self(), 30)              -- solo la Z: la X y la Y quedan en 10 y 20
    setShared("x", getRotationX(self()))
    setShared("y", getRotationY(self()))
    setShared("z", getRotationZ(self()))
end
