-- ==========================================================================
--  Anota el ORDEN de los eventos de hitbox y de contactos (prueba del motor).
--  En compartido("<prefijo>_entrar" / _quedarse / _salir / _tocar) va la lista
--  "A;B;" de los 'otro' en el orden en que llegaron; _quedarse solo los dos
--  primeros; _dentro = hitboxDentro(yo()) al ultimo alEntrar. Con apagarEn = N
--  el script apaga su hitbox en el tick N (el tick siguiente llegan los alSalir).
-- ==========================================================================
properties = { prefijo = "x", apagarEn = 0 }

local p = "x"
local t = 0
local nq = 0
local function annotate(c, o)
  local k = p .. "_" .. c
  setShared(k, (getShared(k) or "") .. getName(o) .. ";")
end

function start()
  p = property("prefijo")
  setShared(p .. "_entrar", "")
  setShared(p .. "_quedarse", "")
  setShared(p .. "_salir", "")
  setShared(p .. "_tocar", "")
  setShared(p .. "_dentro", "")
end

function onEnter(other, mine, own)
  annotate("entrar", other)
  local l = hitboxInside(self())
  local s = ""
  for i = 1, #l do s = s .. getName(l[i]) .. ";" end
  setShared(p .. "_dentro", s)
end

function onStay(other, mine, own)
  nq = nq + 1
  if nq <= 2 then annotate("quedarse", other) end
end

function onExit(other, mine, own)
  annotate("salir", other)
end

function onTouch(other, impulse, x, y, z, nx, ny, nz)
  annotate("tocar", other)
end

function update(dt)
  t = t + 1
  local n = tonumber(property("apagarEn")) or 0
  if n > 0 and t == n then setHitboxActive(self(), false) end
end
