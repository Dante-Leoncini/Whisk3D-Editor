-- año, señal, canción: la ñ y las tildes en el editor
function update(dt)
    local x, y = getPosPx(self())
    if key("up") then
        y = y - 200 * dt

        if y < 0 then
            y = 0
        end
    end
    setPosPx(self(), x, y)
end
