-- ==========================================================================
--  Contador de eventos de HITBOX y de CONTACTOS (pruebas del motor).
--  Cuenta alEntrar / alQuedarse / alSalir / alTocar en compartido("<prefijo>_...")
--  y anota los objetos que llegaron: <prefijo>_otro / _mio / _suyo del ultimo
--  alEntrar, <prefijo>_salio del ultimo alSalir, <prefijo>_tocado y _nz del
--  ultimo alTocar (nz: la componente VERTICAL de la normal; lua es Z arriba).
--  El mismo .lua va en varios objetos con otro 'prefijo'.
-- ==========================================================================
properties = { prefijo = "x" }

local p = "x"
local function sum(c)
  local k = p .. "_" .. c
  setShared(k, (getShared(k) or 0) + 1)
end
local function name(o)
  if o then return getName(o) end
  return "nil"
end

function start()
  p = property("prefijo")
  setShared(p .. "_entrar", 0)
  setShared(p .. "_quedarse", 0)
  setShared(p .. "_salir", 0)
  setShared(p .. "_tocar", 0)
end

function onEnter(other, mine, own)
  sum("entrar")
  setShared(p .. "_otro", name(other))
  setShared(p .. "_mio", name(mine))
  setShared(p .. "_suyo", name(own))
end

function onStay(other, mine, own)
  sum("quedarse")
end

function onExit(other, mine, own)
  sum("salir")
  setShared(p .. "_salio", name(other))
end

function onTouch(other, impulse, x, y, z, nx, ny, nz)
  sum("tocar")
  setShared(p .. "_tocado", name(other))
  setShared(p .. "_nz", nz)
  setShared(p .. "_impulso", impulse)
end
