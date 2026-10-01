-- ==========================================================================
--  Contador de eventos de HITBOX y de CONTACTOS (pruebas del motor).
--  Cuenta alEntrar / alQuedarse / alSalir / alTocar en compartido("<prefijo>_...")
--  y anota los objetos que llegaron: <prefijo>_otro / _mio / _suyo del ultimo
--  alEntrar, <prefijo>_salio del ultimo alSalir, <prefijo>_tocado y _ny del
--  ultimo alTocar. El mismo .lua va en varios objetos con otro 'prefijo'.
-- ==========================================================================
propiedades = { prefijo = "x" }

local p = "x"
local function suma(c)
  local k = p .. "_" .. c
  setCompartido(k, (compartido(k) or 0) + 1)
end
local function nom(o)
  if o then return nombre(o) end
  return "nil"
end

function inicio()
  p = propiedad("prefijo")
  setCompartido(p .. "_entrar", 0)
  setCompartido(p .. "_quedarse", 0)
  setCompartido(p .. "_salir", 0)
  setCompartido(p .. "_tocar", 0)
end

function alEntrar(otro, mio, suyo)
  suma("entrar")
  setCompartido(p .. "_otro", nom(otro))
  setCompartido(p .. "_mio", nom(mio))
  setCompartido(p .. "_suyo", nom(suyo))
end

function alQuedarse(otro, mio, suyo)
  suma("quedarse")
end

function alSalir(otro, mio, suyo)
  suma("salir")
  setCompartido(p .. "_salio", nom(otro))
end

function alTocar(otro, impulso, x, y, z, nx, ny, nz)
  suma("tocar")
  setCompartido(p .. "_tocado", nom(otro))
  setCompartido(p .. "_ny", ny)
  setCompartido(p .. "_impulso", impulso)
end
