-- ==========================================================================
--  prefab_spawner.lua — prueba del bind RUNTIME `instanciar(prefab, x, y, z)`
--  (ejes de lua: Z arriba). Clona el prefab "caja" de la Biblioteca en medio
--  de la partida; nil = no existe (el flag queda en 0 y el .w3s falla).
-- ==========================================================================
local tick = 0

function start()
    setShared("pf_spawn", -1)
    setShared("pf_spawn_nil", -1)
end

function update(dt)
    tick = tick + 1
    if tick == 1 then
        local o = instantiate("caja", 0.0, 0.0, 4.0)   -- a 4 de altura
        setShared("pf_spawn", (o ~= nil) and 1 or 0)
        -- un prefab que NO existe tiene que dar nil (no crashear)
        local bad = instantiate("no-existe")
        setShared("pf_spawn_nil", (bad == nil) and 1 or 0)
    end
end
