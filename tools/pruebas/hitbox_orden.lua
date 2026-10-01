-- ==========================================================================
--  Anota el ORDEN de los eventos de hitbox y de contactos (prueba del motor).
--  En compartido("<prefijo>_entrar" / _quedarse / _salir / _tocar) va la lista
--  "A;B;" de los 'otro' en el orden en que llegaron; _quedarse solo los dos
--  primeros; _dentro = hitboxDentro(yo()) al ultimo alEntrar. Con apagarEn = N
--  el script apaga su hitbox en el tick N (el tick siguiente llegan los alSalir).
-- ==========================================================================
propiedades = { prefijo = "x", apagarEn = 0 }

local p = "x"
local t = 0
local nq = 0
local function anotar(c, o)
  local k = p .. "_" .. c
  setCompartido(k, (compartido(k) or "") .. nombre(o) .. ";")
end

function inicio()
  p = propiedad("prefijo")
  setCompartido(p .. "_entrar", "")
  setCompartido(p .. "_quedarse", "")
  setCompartido(p .. "_salir", "")
  setCompartido(p .. "_tocar", "")
  setCompartido(p .. "_dentro", "")
end

function alEntrar(otro, mio, suyo)
  anotar("entrar", otro)
  local l = hitboxDentro(yo())
  local s = ""
  for i = 1, #l do s = s .. nombre(l[i]) .. ";" end
  setCompartido(p .. "_dentro", s)
end

function alQuedarse(otro, mio, suyo)
  nq = nq + 1
  if nq <= 2 then anotar("quedarse", otro) end
end

function alSalir(otro, mio, suyo)
  anotar("salir", otro)
end

function alTocar(otro, impulso, x, y, z, nx, ny, nz)
  anotar("tocar", otro)
end

function actualizar(dt)
  t = t + 1
  local n = tonumber(propiedad("apagarEn")) or 0
  if n > 0 and t == n then setHitboxActivo(yo(), false) end
end
