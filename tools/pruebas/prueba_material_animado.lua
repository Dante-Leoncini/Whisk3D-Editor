-- ==========================================================================
--  La TRANSFORMACION UV y la FUERZA DE LA EMISION de un material desde lua (prueba_material_animado.w3s: la lee
--  con luaprops, que corre el cuerpo del script con la API). Cada propiedad dice si la llamada hizo lo que tenia que
-- ==========================================================================
setMaterialUV("Material", 0.25, 0.5, 2, 3, 45)
local a, b, c, d, e = getMaterialUV("Material")
setEmissionStrength("Material", 3.5)
properties = {
  uvOk = (math.abs(a - 0.25) < 1e-4 and math.abs(b - 0.5) < 1e-4 and c == 2 and d == 3 and e == 45) and 1 or 0,
  strength = getEmissionStrength("Material"),
  missing = setMaterialUV("NoExiste", 1, 1) and 1 or 0,
}
