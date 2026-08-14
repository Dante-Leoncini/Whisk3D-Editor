-- ==========================================================================
--  hud_anim_frames.lua — el icono 2D animado POR FRAMES (setTextura por
--  tick), como la fruta del HUD de un juego: cicla 3 texturas de colores
--  planos. Lo usa prueba_hud_sustractivo.w3d; el .w3s muestrea el pixel del
--  centro del icono en CADA frame real y falla si UNO SOLO sale negro.
-- ==========================================================================
propiedades = {
    icono = "objeto",     -- ref_icono: "IconoAnim"
}

local t = 0
local texs = { "texturas/rojo.png", "texturas/amarillo.png", "texturas/azul.png" }

function actualizar(dt)
    local o = objeto and objeto("icono") or nil
    if not (o and setTextura) then return end
    t = t + 1
    setTextura(o, texs[(t % 3) + 1])
end
