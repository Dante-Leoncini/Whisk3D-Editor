-- ==========================================================================
--  lente_camara.lua — el LENTE de la camara desde lua.
--
--    lenteDe(camara)                      -> fov, cerca, lejos
--    setLente(camara [, fov] [, cerca] [, lejos])   nil = no tocar ese
--
--  Lo usa prueba_lente.w3d. El script mueve el PLANO LEJANO segun el tick:
--  arranca con el alcance largo (se ve todo) y al tick 3 lo acerca (la
--  geometria lejana deja de dibujarse). Es el caso de uso real: entrar a un
--  espacio cerrado y no querer que asome lo que esta lejos.
--  Deja lo leido en el mapa compartido para que el .w3s lo asserte.
-- ==========================================================================
properties = {
    cam = "object",     -- ref_cam: "Camara"
}

local tick = 0

local function dump(cam)
    local fov, near, far = getLens(cam)
    setShared("fov",   string.format("%.1f", fov))
    setShared("cerca", string.format("%.2f", near))
    setShared("lejos", string.format("%.1f", far))
end

function start()
    tick = 0
    local cam = object and object("cam") or nil
    if not (cam and getLens and setLens) then return end
    -- NO se toca nada: se lee lo que declaro el .w3d (fov 60, cerca 0.10,
    -- lejos 1000). Asi el primer assert del .w3s prueba de paso que los tres
    -- viajan en el formato de texto.
    dump(cam)
end

function update(dt)
    local cam = object and object("cam") or nil
    if not cam then return end
    tick = tick + 1
    if tick == 3 then
        setLens(cam, nil, nil, 60.0)  -- alcance CORTO: el fondo se va
    elseif tick == 6 then
        setLens(cam, 30.0, 0.5, 1000.0)  -- y los tres a la vez
    end
    dump(cam)
end
