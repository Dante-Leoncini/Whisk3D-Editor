-- ==========================================================================
--  componente.lua — prueba de los scripts "estilo Unity" en CUALQUIER objeto:
--  propiedades de VALOR por instancia (val_*) + estado COMPARTIDO entre scripts.
--  La usa prueba_componentes.w3d: dos Wavefront con este MISMO .lua y val_id
--  distintos. Cada instancia corre en su propio lua_State: la unica forma de
--  sumar entre ellas es el mapa compartido (setCompartido / compartido).
-- ==========================================================================
properties = {
    id = 0,             -- valor NUMERICO por instancia (val_id en el .w3d)
    agarrada = false,   -- valor BOOL por instancia (val_agarrada)
    etiqueta = "caja",  -- valor de TEXTO por instancia (val_etiqueta)
    empuje = 2.5,       -- solo DEFAULT declarado: ninguna instancia lo configura
}

function start()
    -- propiedad() devuelve lo configurado en ESTA instancia (o el default de arriba)
    local n = property("id")
    setShared("suma", (getShared("suma") or 0) + n)
    if property("agarrada") then
        setShared("agarradas", (getShared("agarradas") or 0) + 1)
    end
    setShared("ultima", property("etiqueta"))

    -- parametro(nombre [, default]) es el ALIAS NUMERICO de propiedad(): el
    -- vocabulario lo declara el PROYECTO (val_* del .w3d y la tabla de arriba),
    -- no el motor. Las tres precedencias, de mayor a menor:
    --   1) val_id de la instancia   2) el default declarado aca
    --   3) el default del call site (una clave que nadie declaro)
    setShared("paramId",     string.format("%d",   param("id")))
    setShared("paramEmpuje", string.format("%.2f", param("empuje")))
    setShared("paramNadie",  string.format("%.2f", param("noExiste", 7.25)))
    -- y sin default, una clave desconocida da 0 (no rompe ni devuelve nil)
    setShared("paramCero",   string.format("%.2f", param("tampocoExiste")))
end

function update(dt)
end
