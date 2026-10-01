-- prueba de prefabs: un SPAWNER que instancia en su inicio() (la partida todavia esta arrancando). Lo creado
-- es de la partida igual: su inicio() corre (cuenta en "enemigos") y el Stop lo borra
function inicio()
  local e = instanciar("Enemigo", 0, 0, 5)
  setCompartido("spawn", e and nombre(e) or "nil")
end
