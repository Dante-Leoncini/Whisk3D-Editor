-- los cuerpos rigidos DINAMICOS arrancan dormidos: se despierta con una velocidad
function start()
  physicsVel(self(), 0, 0, -0.5)   -- hacia abajo (Z arriba)
end
