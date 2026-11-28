-- ==========================================================================
--  La FUERZA DE LA EMISION de un material desde lua (prueba_material_animado.w3s: la lee con luaprops, que corre el
--  cuerpo del script con la API). Cada propiedad dice si la llamada hizo lo que tenia que
-- ==========================================================================
setEmissionStrength("Material", 3.5)
properties = {
  strength = getEmissionStrength("Material"),
  missing = getEmissionStrength("NoExiste"),   -- (un material que no esta: 1, la de siempre)
}
