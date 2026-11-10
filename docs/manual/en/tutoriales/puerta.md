# Changing the origin: a door

In this tutorial you'll make a door open from the hinge. To do that, you have to change its **origin**: the point the object turns around (see [The origin](modo-objeto.md#the-origin) in the Object Mode tutorial).

We start from the door in the previous tutorial, [Modeling a door](modelar-puerta.md), with the origin in the center. If you made it by moving one side, the origin ended up off-center: **Object → Set Origin → Origin to Geometry** puts it back in the center.

## The door turns the wrong way

Try turning it. The door turns around its center, like a revolving door, and not from the hinge:

![PC](img/icono-pc.svg) With the gizmo: a click on its center switches it to **Rotate**, and the blue ring turns it around the vertical axis.

![A click on the center of the gizmo switches it to Rotate; the blue ring is dragged and the door turns around its center, like a revolving door, and comes back](img/puerta-gira-mal-raton.webp)

![Android](img/icono-android.svg) With your finger: a tap on the center of the gizmo switches it to **Rotate**, and you drag the blue ring.

![The finger taps the center of the gizmo and switches it to Rotate; then it drags the blue ring and the door turns around its center, and comes back](img/puerta-gira-mal-dedo.webp)

![Nokia](img/icono-nokia.svg) On the Nokia N95 the gizmo is off: <kbd>2</kbd> (rotate), <kbd>3</kbd> (the Z axis, the vertical one) and the arrows; <kbd>OK</kbd> confirms.

![A Nokia N95: 2 and 3 start rotating around the vertical axis; the right arrow turns the door around its center, the left one turns it back and OK confirms](img/puerta-gira-mal-nokia.webp)

It turns the wrong way because its origin is in the center. Let's move it to the hinge.

## Move the origin to the hinge

1. **Enter Edit Mode.** A click on the mode button and another on **Edit Mode** (on PC, also with <kbd>Tab</kbd>).

   ![The mode button open, with Edit Mode highlighted](img/puerta-paso-1.webp)

2. **Choose faces.** In Edit Mode the **Vertex / Edge / Face** button appears: click it and choose **Face**.

   ![The Vertex, Edge, Face button open, with Face highlighted](img/puerta-paso-2.webp)

3. **Select the hinge face.** Click on the narrow face on the side of the door, on the side where the hinge goes: it turns green.

   ![The narrow face on the side of the door, selected in green and marked with a circle](img/puerta-paso-3.webp)

4. **Move the 3D cursor there.** In the **Mesh** menu, **Snap** → **Cursor to Selected**: the 3D cursor ends up on the hinge.

   ![The Mesh menu open, with the Snap submenu and Cursor to Selected highlighted](img/puerta-paso-4.webp)

5. **Go back to Object Mode.** A click on the mode button and another on **Object Mode** (or <kbd>Tab</kbd>).

   ![The mode button open, with Object Mode highlighted; the 3D cursor is already on the hinge](img/puerta-paso-5.webp)

6. **Move the origin to the 3D cursor.** In the **Object** menu, **Set Origin** → **Origin to 3D Cursor**. The door doesn't move: what moves is its origin, and with it the gizmo, which ends up on the hinge.

   ![The Object menu open, with the Set Origin submenu and Origin to 3D Cursor highlighted](img/puerta-paso-6.webp)

## Now it turns the right way

Turn it again with the gizmo: now it opens and closes from the hinge.

![With the origin on the hinge, the blue ring of the gizmo opens the door until it's sideways and closes it again](img/puerta-gira-bien-raton.webp)

![With a finger, the blue ring opens the door from the hinge and closes it again](img/puerta-gira-bien-dedo.webp)

![A Nokia N95: 2, 3 and the right arrow open the door from the hinge; the left one closes it and OK confirms](img/puerta-gira-bien-nokia.webp)

![Android](img/icono-android.svg) On a touch screen, the steps above are the same, with taps: menus open and options are chosen with a tap.

![Nokia](img/icono-nokia.svg) On the Nokia N95, <kbd>OK</kbd> switches from Object Mode to Edit Mode and menus open with the left soft key.

## The Set Origin options

| Option | What it does |
|---|---|
| **Origin to Geometry** | Moves the origin to the center of the mesh. |
| **Origin to 3D Cursor** | Moves the origin to where the 3D cursor is. |
| **Geometry to Origin** | The other way around: it leaves the origin and moves the mesh until its center is on it. |

In the next tutorial we'll make its handle: [Modeling the door handle](modelar-picaporte.md).
