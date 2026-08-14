-- ==========================================================================
--  particulas_rafaga.lua — prueba del bind emitir(obj, n): la RAFAGA (el polvo
--  de las pisadas / el estallido magico del Aku Aku). El emisor referenciado
--  tiene cantidad: 0, o sea que SOLO emite por rafagas: las 8 particulas que
--  cuenta prueba_particulas.w3s salen de este inicio() y de ningun otro lado.
-- ==========================================================================
function inicio()
    emitir(objeto("emisor"), 8)
end

function actualizar(dt)
end
