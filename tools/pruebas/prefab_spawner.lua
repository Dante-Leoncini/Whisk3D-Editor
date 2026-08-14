-- ==========================================================================
--  prefab_spawner.lua — prueba del bind RUNTIME `instanciar(prefab, x, y, z)`
--  (coordenadas del MOTOR). Clona el prefab "caja" de la Biblioteca en medio
--  de la partida; nil = no existe (el flag queda en 0 y el .w3s falla).
-- ==========================================================================
local tick = 0

function inicio()
    setCompartido("pf_spawn", -1)
    setCompartido("pf_spawn_nil", -1)
end

function actualizar(dt)
    tick = tick + 1
    if tick == 1 then
        local o = instanciar("caja", 0.0, 4.0, 0.0)
        setCompartido("pf_spawn", (o ~= nil) and 1 or 0)
        -- un prefab que NO existe tiene que dar nil (no crashear)
        local malo = instanciar("no-existe")
        setCompartido("pf_spawn_nil", (malo == nil) and 1 or 0)
    end
end
