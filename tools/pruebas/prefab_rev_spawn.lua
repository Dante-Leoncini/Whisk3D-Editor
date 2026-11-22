-- prueba de prefabs: un SPAWNER que instancia en su inicio() (la partida todavia esta arrancando). Lo creado
-- es de la partida igual: su inicio() corre (cuenta en "enemigos") y el Stop lo borra
function start()
  local e = instantiate("Enemigo", 0, 5, 0)
  setShared("spawn", e and getName(e) or "nil")
end
