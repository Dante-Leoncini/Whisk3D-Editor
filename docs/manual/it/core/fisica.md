# Fisica

Tutto fatto in casa, in C++03, senza motore esterno e senza chiedere memoria a ogni fotogramma. Se nessuno la usa, non costa niente: ogni passo esce alla prima riga.

- **W3dFisica:** velocità e rimbalzo di box allineati agli assi. Nient'altro.
- **W3dRigido:** box orientati con massa, gravità, impulsi con attrito e rimbalzo, raycast ed eventi di contatto.
- **W3dColMalla:** camminare sui triangoli di uno scenario (rampe, gradini) e farsi fermare dai muri, con una griglia per non dover controllare migliaia di triangoli.
- **W3dHitbox:** box che si limitano ad avvisare (`alEntrar`, `alQuedarse`, `alSalir`, `alTocar`).
- **W3dRagdoll:** ragdoll, e cinematica inversa a due ossa e FABRIK.
- **W3dNav:** il percorso da un punto a un altro su una griglia del pavimento (A* e poi tensionamento).

Perché è importante: il Play dell'editor e il gioco compilato chiamano lo stesso passo con lo stesso `dt`, quindi il gioco si comporta allo stesso modo in entrambi, sul PC e sull'N95. Da Lua: [Fisica semplice](../lua/api-fisica-simple.md) e [Corpi rigidi, collisioni e hitbox](../lua/api-cuerpos-rigidos.md).
