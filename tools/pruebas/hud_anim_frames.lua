-- ==========================================================================
--  hud_anim_frames.lua — el icono 2D animado POR FRAMES (setTextura por
--  tick), como la fruta del HUD de un juego: cicla 3 texturas de colores
--  planos. Lo usa prueba_hud_sustractivo.w3d; el .w3s muestrea el pixel del
--  centro del icono en CADA frame real y falla si UNO SOLO sale negro.
-- ==========================================================================
properties = {
    icono = "object",     -- ref_icono: "IconoAnim"
}

local t = 0
local texs = { "textures/rojo.png", "textures/amarillo.png", "textures/azul.png" }

function update(dt)
    local o = object and object("icono") or nil
    if not (o and setTexture) then return end
    t = t + 1
    setTexture(o, texs[(t % 3) + 1])
end
