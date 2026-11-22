-- ==========================================================================
--  pantalla_componente.lua — vigila el LIENZO que ven los binds del juego.
--
--  pantalla() devuelve el tamano de la pantalla logica del juego. Con VARIOS
--  viewports 3D ese tamano tiene que ser UNO solo y quieto: el del viewport
--  ACTIVO (antes, el ultimo viewport en dibujar pisaba el override global y la
--  UI del juego se re-armaba a cada rato con una pantalla distinta).
--  Publica en el mapa compartido:
--    pv_lado     "angosto" (w < h) | "ancho" (w >= h)  -> de que viewport es
--    pv_cambios  cuantas veces CAMBIO el tamano entre ticks (el arranque, del
--                lienzo por defecto a la pantalla del viewport activo, es el
--                unico cambio legitimo: mas de 1 = la pantalla esta bailando)
--    pv_w/pv_h   el tamano leido (diagnostico)
-- ==========================================================================
properties = {}

local w0, h0 = -1, -1
local changes = 0

function start()
    setShared("pv_cambios", "0")
end

function update(dt)
    if not screen then return end
    local w, h = screen()
    if w0 >= 0 and (w ~= w0 or h ~= h0) then changes = changes + 1 end
    w0, h0 = w, h
    setShared("pv_cambios", string.format("%d", changes))
    setShared("pv_lado", (w < h) and "angosto" or "ancho")
    setShared("pv_w", string.format("%.1f", w))
    setShared("pv_h", string.format("%.1f", h))
end
