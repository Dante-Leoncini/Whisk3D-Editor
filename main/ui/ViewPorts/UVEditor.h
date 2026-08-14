#ifndef UVEDITOR_H
#define UVEDITOR_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/WithBorder.h" // borde (verde si activo) como Outliner/Properties

class Mesh; // pick UV (la seleccion vive en Mesh::uvSelVert)

// ROLES estables de los botones de la barra del UV editor. El dispatch (LayoutClickBarraUV) y la
// visibilidad (SyncBarra) los buscan por ROL (BarRolBtn), NO por indice -> agregar/reordenar
// botones no rompe nada (mismo esquema que BarRol3D en el 3D).
enum BarRolUV {
    BRUV_Modo = 1,  // selector de modo del UV: Objeto / Edicion / Pintura de pesos / Huesos / Pose
                    // (desplegable, como el Mode del 3D; ver el enum UVModo* mas abajo). El modo OBJETO
                    // es el de arriba de todo: elige QUE se edita (la geometria o uno de los rigs 2D).
    BRUV_View,      // checkboxes (Sync Selection / Repeat Texture / Chrome UV) + Frame Selected / Frame All
    BRUV_SelMode,   // Vertex/Edge/Face (solo icono)
    BRUV_Pivot,     // Transform Pivot (solo icono, = el del 3D)
    BRUV_Snap,      // boton "Cursor": las ops del CURSOR 2D (Cursor to Selection / Selection to Cursor /
                    // Cursor to Center). SE LLAMA "Cursor" y NO "Snap" a proposito: en el viewport 3D
                    // "Snap" es el IMAN (BR_Snap, g_snap) y estas ops viven en Mesh > Snap. Tener dos
                    // botones con el mismo nombre y distinto significado era la trampa de consistencia
                    // que reporto la auditoria; el UV todavia no tiene iman (ver AplicarXformValores).
    BRUV_Texture,   // dropdown: que textura/parte ver
    BRUV_Animation, // menu Animation: keyframes de la VERTEX ANIM activa (icono keyframe, como BR_Animation)
    BRUV_Add,       // menu Add: crear el ARMATURE 2D del mesh / agregar huesos (icono +, como BR_Add)
    BRUV_Armature,  // menu Armature (solo en UVModoHuesos): Extrude/Duplicate/Delete/Set Parent/Clear Parent
    BRUV_Select     // menu Select (icono seleccion, CALCADO del BR_Select del 3D): All / None /
                    // Invert / Select Linked. Contextual: en edicion opera los UVs, en Huesos/Pose
                    // los huesos 2D. Oculto en pintura de pesos (ahi no hay seleccion que operar).
};

// MODO del editor UV. Es un estado PROPIO del viewport, NO el InteractionMode global del 3D:
//  - el WeightPaint global cambia toda la app (seleccion, gizmos, menus del 3D); aca se quiere
//    pintar pesos EN el UV mientras el 3D sigue en Edit Mode (el flujo de rigging 2D),
//  - y con varios UV editors abiertos cada uno puede estar en su modo.
// Por eso NO se sincronizan (decision documentada; si a futuro conviene espejarlos, es un solo
// punto: el setter del menu Modo en LayoutInput.cpp).
enum UVEditorModo {
    UVModoEdicion = 0,  // seleccionar/mover/rotar/escalar UVs (el "Edit Mode" de la GEOMETRIA)
    UVModoPesos   = 1,  // pintura de pesos: relleno por peso + pincel (Fase 2, edit/WeightPaint.cpp)
    UVModoHuesos  = 2,  // ARMATURE 2D del mesh: EDITAR los huesos (mover puntas/extruir/borrar)
    UVModoPose    = 3,  // ARMATURE 2D del mesh: POSAR (G/R/S 2D -> los UV pesados siguen al hueso)
    UVModoObjeto  = 4   // DEFAULT: elegir QUE se edita. Se dibujan la geometria (los UVs) y TODOS
                        // los armatures 2D de la malla; el click selecciona la entidad bajo el
                        // cursor (armature 2D si le pegas a un hueso, si no la geometria) y Tab
                        // entra a editar la seleccionada. Ver uvObjArm mas abajo.
};

// ROLES de la TOOLBAR inferior del UV editor (mecanismo compartido de ViewportBase).
// Los TBR_Pincel*/TBR_Grupo son los CONTROLES DEL PINCEL (Fase 2) y estan COMPARTIDOS con la
// toolbar del 3D en modo Weight Paint (ViewPort3D_Toolbar.cpp los usa con estos mismos valores).
// Deshacer/Rehacer usan los roles TBR_Undo/TBR_Redo del 3D (ViewPort3D.h), mismos iconos/labels.
enum ToolbarRolUV {
    TBR_UVHist = 1,      // .. TBR_UVHist+2: historial MRU (Move/Rotate/Scale), en edicion/huesos/pose
    TBR_PincelTam = 20,  // tamano del pincel (px) - abre un menu deslizable (slider)
    TBR_PincelFuerza,    // fuerza (0..1, se muestra en %) - menu deslizable
    TBR_PincelModo,      // modo del pincel: sumar (+) / restar (-), toggle
    TBR_Grupo,           // vertex group activo (a que hueso se pinta) - dropdown
    TBR_SoloSel,         // "editar solo lo seleccionado": el pincel SOLO pinta verts de caras
                         // seleccionadas en edit mode. Toggle GLOBAL compartido 3D/UV
                         // (WeightPaintSoloSel), tinte accent cuando esta ON
    // acciones de HUESO 2D (solo visibles en UVModoHuesos), pensadas para pantallas tactiles
    TBR_BoneExtrude = 30, // E: hueso nuevo desde el tail del activo
    TBR_BoneDup,          // Shift+D: duplicar los seleccionados (sueltos)
    TBR_BoneDel,          // X/Supr: borrar los seleccionados (re-parenta los hijos)
    TBR_BoneDesc          // Alt+P: desconectar (los seleccionados quedan raiz)
};

// =====================================================================
//  UV Editor (2D) — por ahora un VISOR (no edita todavia).
// =====================================================================
//  - Si el objeto activo NO es una malla: no muestra nada (solo el fondo).
//  - Si hay una malla: muestra la TEXTURA de la mesh part activa/seleccionada,
//    centrada, con el WIREFRAME de las UV encima.
//  - Rueda del mouse = zoom; boton del MEDIO + arrastrar = paneo.
//  - Menu del viewport (boton de icono): checkbox "Show Whole Mesh UV" (todo el
//    mesh vs solo la parte activa) y "Repeat Texture" (tiling hasta el infinito).
class UVEditor : public ViewportBase, public WithBorder {
    public:
        int ViewportKind() const { return 4; } // 1=3D 2=outliner 3=properties 4=UV

        float zoom;          // 1 = la textura llena ~80% del viewport
        float panX, panY;    // desplazamiento en pixeles (paneo)
        // ================= SYNC SELECTION (semantica de Blender) =================
        // El toggle vive en el menu "View" de la barra del UV (checkbox "Sync Selection").
        //
        //  OFF (DEFAULT, el modelo que se usa para editar UVs):
        //    - el viewport 3D es un FILTRO/MASCARA: en el UV se ven (y se editan) SOLO las caras
        //      seleccionadas en 3D; el resto del mapa ni se dibuja.
        //    - la seleccion DENTRO del UV es PROPIA, INDEPENDIENTE y POR RENDER-VERT (por
        //      esquina/corner): clickear una copia UV de un vertice 3D selecciona EXACTAMENTE
        //      esa copia. En una COSTURA un mismo vertice 3D tiene VARIAS copias UV (render-verts
        //      distintos con la misma posicion 3D) y NO se contagian entre si. Idem bordes/caras.
        //    - nada de expandir por posicion (posRep) ni de escribir la seleccion del 3D.
        //    - al ENTRAR o cuando CAMBIA el filtro (las caras seleccionadas en 3D), la seleccion
        //      propia se INICIALIZA con las esquinas visibles: ver SincronizarFiltro3D.
        //
        //  ON (modo espejo, opcional):
        //    - ves TODO el UV del modelo: las caras seleccionadas en 3D en verde, el resto gris.
        //    - la seleccion del UV es un ESPEJO de la del mesh y es BIDIRECCIONAL (pickear en el
        //      UV selecciona en el 3D). Ahi SI un click agarra TODAS las copias UV de ese vertice
        //      3D (Mesh::VertsSelPorModo expande por posRep): en este modo es lo ESPERADO, no un
        //      bug -- es la razon por la que el default es OFF.
        bool  syncSelection;
        bool  repeatTexture; // true: textura repetida (tiling)
        bool  mostrarChromeUV; // overlay LIVE de las UV del reflejo chrome equirect (demo/debug; off = 0 CPU extra)
        int   lastMx, lastMy;// para el delta del paneo

        // --- TRANSFORM 2D de la seleccion UV (G/R/S en PC; 1/2/3 en Symbian) ---
        int   uvXform;              // 0=nada 1=mover 2=rotar 3=escalar
        int   uvXAxis;              // BLOQUEO DE EJE (teclas X/Y durante el transform): 0=libre 1=X 2=Y.
                                    // Siempre en espacio de la VISTA (en el UV no hay local/global).
        float uvXPivotU, uvXPivotV; // pivot: MEDIANA de la seleccion (promedio de posiciones UV UNICAS)
                                    // o el cursor 2D si el pivote de la barra es "3D Cursor"
        float uvXStartU, uvXStartV; // posicion del mouse al iniciar, en UV
        // cursor VIRTUAL del transform (en UV): acumula los deltas del mouse FILTRANDO el salto
        // del warp (el cursor se envuelve en los bordes, como el 3D). Las cuentas usan esto,
        // nunca la posicion real (mismo esquema que los acum* del Editor 2D).
        float uvXCurU, uvXCurV;
        // valores APLICADOS del transform (post-lock), para la barra de INFO: delta U/V del
        // mover, angulo (grados) del rotar, factor del escalar. Los setea AplicarXformValores.
        float uvXValU, uvXValV, uvXValAng, uvXValFac;
        std::vector<int>   uvXIdx;  // verts de render seleccionados (los que se mueven)
        std::vector<float> uvXOrig; // su uv ORIGINAL (2 por vert): base estable + para cancelar
        std::vector<float> uvXPivots; // pivote POR VERT (2 por vert; solo con Pivot=Individual en modo
                                      // cara: cada cara rota/escala alrededor de SU centro). Vacio = global
        float uvCursorU, uvCursorV; // CURSOR 2D (en UV): pivot opcional (modo "3D Cursor") + snap
        // ---- BOX SELECT (tecla B, como el 3D de Blender) ----
        // B ARMA el gesto (uvBoxArmado) y el siguiente arrastre con el boton izquierdo define el
        // rectangulo (uvBoxSel + las 4 coords, en px LOCALES del viewport). Al soltar se aplica
        // sobre lo que el modo actual selecciona: los render-verts/bordes/caras UV en edicion, o
        // los HUESOS 2D (por punta) en Edit Bones / Pose. Shift SUMA a la seleccion.
        bool  uvBoxArmado;          // se apreto B: el proximo drag izquierdo es el rectangulo
        bool  uvBoxSel;             // rectangulo en curso (se dibuja)
        bool  uvBoxAdd;             // el gesto arranco con shift -> suma en vez de reemplazar
        int   uvBoxX0, uvBoxY0;     // esquina inicial (px LOCALES)
        int   uvBoxX1, uvBoxY1;     // esquina actual  (px LOCALES)
        int   uvSelMode;            // modo de seleccion PROPIO del UV (SelVertex/Edge/Face), independiente
                                    // del 3D. En sync (syncSelection) se usa el del 3D. El efectivo = ModoUV().
        int   uvModo;               // UVEditorModo: objeto (default) / edicion / pintura / huesos / pose.
                                    // PROPIO del viewport (NO es el InteractionMode global: ver el enum arriba)
        int   uvModoPrevio;         // modo al que vuelve el TAB al salir de UVModoHuesos (ver TabToggleHuesos)
        // OBJETO ACTIVO del UV (lo elige el click en UVModoObjeto): false = la GEOMETRIA (los UVs),
        // true = el ARMATURE 2D activo de la malla (Mesh::armature2dActivo). Es lo que decide a
        // donde entra el Tab (edicion de UVs vs Edit Bones) y que se resalta al dibujar.
        bool  uvObjArm;
        // --- estado del FILTRO 3D con el que se INICIALIZO la seleccion propia (sync OFF) ---
        // firma del conjunto de caras seleccionadas en el 3D + con que malla y con que valor de
        // syncSelection se calculo. Si algo de eso cambia, SincronizarFiltro3D re-inicializa la
        // seleccion propia del UV. NO es un cache de render: es el "al entrar / al cambiar el
        // filtro" del modelo de seleccion (el resto del tiempo el UV manda solo).
        // La malla va por SERIAL (Object::serial), NO por puntero: borrar una malla y crear otra
        // recicla la direccion y "filtroMesh == m" daba TRUE para una malla distinta -> el UV se
        // quedaba con la seleccion inicializada para la malla vieja y no re-inicializaba nada.
        unsigned filtroSig;
        unsigned filtroSerial;
        bool     filtroSync;

        UVEditor();
        virtual ~UVEditor();

        // el UV editor esta OPERATIVO: el objeto activo es una malla y esta en Edit Mode (g_editMesh).
        // Es la condicion de visibilidad de los botones de barra propios (SelMode/Snap/Animation/Modo).
        bool EnEdicionUV() const;
        // estado de la BARRA superior (visibilidad/iconos/textos por rol). Separado del Render
        // (que es GL) para poder testearlo headless (comando uvbar). Render la llama por frame.
        void SyncBarra();
        // toolbar inferior compartida (ToolbarBase): G/R/S (MRU) en modo edicion; en PINTURA los
        // controles del PINCEL (roles TBR_Pincel*/TBR_Grupo). Oculta si el UV no esta operativo.
        bool ToolbarVisible() const override;
        void ToolbarSincronizar() override;
        void ToolbarAccionRol(int rol) override;

        void Render() override;
        void Resize(int newW, int newH) override;
        void event_mouse_motion(int mx, int my) override;
        bool event_finger_scroll(int px, int py, int dx, int dy) override;             // 1 dedo: panear la vista UV
        void event_finger_gesture(float zoomDelta, float panDx, float panDy) override; // 2 dedos: zoom + paneo
        void button_left() override; // click izquierdo = seleccionar UV (o confirmar transform)
        void button_right() override; // click derecho = cancelar transform
        // pick en coords LOCALES del viewport (lx,ly); add=true -> toggle/sumar (shift), false -> reemplazar.
        // Escribe Mesh::uvSelVert. Compartido (PC lo llama desde button_left; Symbian desde OK).
        // FUERA de sync el pick es POR RENDER-VERT: toca EXACTAMENTE el vert/borde/cara UV que
        // clickeaste, sin expandir a las otras copias de la costura ni tocar la seleccion del 3D.
        void PickUV(Mesh* m, int lx, int ly, bool add);
        // SYNC SELECTION: en sync, uvSelVert es un ESPEJO de la seleccion del 3D. Esto la re-deriva
        // (Mesh::VertsSelPorModo) para que seleccionar una cara/vert/borde en el viewport 3D quede
        // resaltada/editable en el UV. No-op fuera de sync (ahi uvSelVert es independiente).
        void SincronizarSelDesde3D(Mesh* m);
        // UN solo punto de entrada por frame (lo llama el Render) para la relacion 3D <-> UV:
        //  - en SYNC: espeja la seleccion del 3D (SincronizarSelDesde3D), como siempre.
        //  - fuera de sync: NO toca la seleccion propia salvo cuando CAMBIA el filtro (las caras
        //    seleccionadas en 3D), la malla o el propio toggle. Ahi la INICIALIZA con todas las
        //    esquinas VISIBLES (lo que aparece, aparece seleccionado, como Blender). Nunca corre
        //    con un transform en curso (no pisa lo que se esta moviendo).
        // outFiltro (opcional) devuelve el FILTRO ya calculado (1 por cara de faces3d) para que
        // el Render no lo recalcule.
        void SincronizarFiltro3D(Mesh* m, std::vector<char>* outFiltro = NULL);
        // modo de seleccion EFECTIVO: si syncSelection -> EditSelectMode (3D); si no -> uvSelMode (propio).
        int ModoUV() const;
        // parametros del mapeo UV->pantalla (centro cx,cy y escala s), iguales que el Render.
        void ParamsUV(float& cx, float& cy, float& s) const;
        // transform 2D (COMPARTIDO; Symbian llama Iniciar desde 1/2/3 y Confirmar/Cancelar desde sus teclas).
        void IniciarXform(Mesh* m, int modo);          // 1=mover 2=rotar 3=escalar (snapshot + pivot)
        void AplicarXform(Mesh* m, float curU, float curV); // deriva delta/angulo/factor del cursor y aplica
        // nucleo del apply (el lock de eje se aplica ADENTRO): lo comparten el mouse (AplicarXform)
        // y la entrada numerica exacta (XformNumValor), sin duplicar los loops.
        void AplicarXformValores(Mesh* m, float dU, float dV, float ang, float f);
        void ConfirmarXform();                          // fija el cambio
        void CancelarXform(Mesh* m);                    // restaura los uv originales

        // ---- hooks del TRANSFORM UI COMPARTIDO (TransformUI.cpp): barra de info, entrada
        // numerica, warp del cursor y tilde/cruz/X/Y de la toolbar. Cubren el G/R/S de UVs
        // (uvXform) y el transform modal de huesos 2D de ESTE editor. ----
        bool XformEnCurso() const override;
        void XformNumValor(float v) override;           // valor exacto tipeado (1.0 = un tile UV)
        void XformConfirmar() override;                 // gate: expresion invalida NO confirma
        void XformCancelar() override;
        void XformToggleEje(int eje) override;          // teclas X/Y y botones de la toolbar
        std::string XformTextoBarra() override;         // texto de la barra de info (unidades UV)
        // SNAP (menu del UV editor): cursor<->seleccion.
        void SnapCursorToSel();  // el cursor 2D va al centro de la seleccion
        void SnapSelToCursor();  // la seleccion se mueve para que su centro quede en el cursor
        void CursorToCenter();   // cursor 2D -> (0.5,0.5)

        // ===== ENCUADRAR (View > Frame Selected / Frame All, atajo Numpad . como el 3D, el
        // Editor 2D y el Timeline) =====
        // Calcula el bounding box UV de lo que corresponde al MODO y ajusta zoom+pan para que
        // llene el ~80% del viewport, igual que Editor2DEncuadrarSeleccion.
        //   todo=false -> la SELECCION (UVs seleccionados; huesos seleccionados en Huesos/Pose).
        //                 Sin seleccion cae a "todo" (mismo criterio que el 3D: siempre encuadra algo).
        //   todo=true  -> TODO lo visible (las caras que pasan el filtro del 3D + los armatures 2D).
        // false = no habia NADA que encuadrar (malla vacia / sin UVs): no toca la vista.
        bool EncuadrarUV(bool todo);
        // bbox UV de lo encuadrable (lo usa EncuadrarUV; publico para el harness headless)
        bool BBoxUV(Mesh* m, bool todo, float& u0, float& v0, float& u1, float& v1) const;

        // ===== BOX SELECT (B + arrastre) =====
        void BoxSelectArmar();                    // tecla B: arma el gesto (el proximo drag dibuja)
        void BoxSelectCancelar();                 // ESC / click sin arrastre
        // CIERRA el gesto y aplica: lo llaman el soltar del mouse y el "up perdido" del motion
        // (se solto fuera del viewport). Un rectangulo degenerado (click sin arrastrar) solo
        // cancela: no deselecciona todo por accidente.
        void BoxSelectSoltar();
        // aplica el rectangulo (px LOCALES, ya normalizado adentro) a la seleccion del modo actual.
        // add = suma a lo que ya estaba (shift). Devuelve cuantos elementos toco.
        int  BoxSelectAplicar(Mesh* m, bool add);

        // ===== TAB DEL UV EDITOR (con el mouse sobre ESTE editor) =====
        // Con el MODO OBJETO el Tab es el de Blender: alterna OBJETO con la EDICION del objeto
        // ACTIVO del UV (la geometria o el armature 2D seleccionado).
        // CICLO FINAL:
        //    Objeto (geometria activa) --Tab--> Edicion de UVs --Tab--> Objeto
        //    Objeto (armature activo)  --Tab--> Edit Bones     --Tab--> Objeto
        //    Pose                      --Tab--> Edit Bones     --Tab--> Pose
        //    Weight Paint              --Tab--> Edit Bones     --Tab--> Weight Paint
        // (uvModoPrevio guarda de donde se entro a Edit Bones; desde Objeto/Edicion se vuelve a
        // Objeto). Al entrar a Edit Bones/Pose se opera SIEMPRE el armature ACTIVO de la malla.
        // CUANDO SE CONSUME LA TECLA: con el UV OPERATIVO (malla activa en Edit Mode) y ademas
        //   - la malla YA TIENE algun armature 2D  (el caso del rig 2D, como antes), o
        //   - el UV esta en MODO OBJETO            (sino no habria como salir del modo objeto).
        // Fuera de eso el Tab cae al comportamiento GLOBAL de siempre (Object <-> Edit Mode del 3D
        // / ciclar viewport): una malla SIN rig 2D que ya esta editando UVs sigue exactamente como
        // antes de que existiera el modo objeto.
        // NUNCA toca el InteractionMode global: el modo del UV es propio del viewport.
        // CONSECUENCIA (querida): con armature 2D, el Tab CON EL MOUSE SOBRE EL UV ya no saca la
        // malla de Edit Mode -- para eso se pasa el mouse al viewport 3D (o se usa el desplegable de
        // Modo). Es justo el pedido: el Tab del UV es del rig 2D y no mueve el modo del 3D.
        // false = no lo consumio (el llamador sigue con el Tab global).
        bool TabToggleHuesos();
        // MODO OBJETO: elige el objeto del UV bajo el cursor (px LOCALES). Si el click cae sobre un
        // hueso de CUALQUIER armature 2D, ese armature pasa a ser el ACTIVO (Mesh::armature2dActivo)
        // y el objeto activo del UV; si no, el objeto activo pasa a ser la GEOMETRIA.
        // Devuelve el indice del armature elegido, o -1 si quedo la geometria.
        int  PickObjetoUV(Mesh* m, float lx, float ly);
        // hueso mas cercano al punto (px locales) mirando TODOS los armatures 2D de la malla.
        // outArm = armature del hueso encontrado. -1 = ninguno cerca.
        int  Bone2DPickTodos(Mesh* m, float lx, float ly, int* outArm) const;
        // Ctrl+click en PINTURA DE PESOS: el hueso bajo el cursor (px locales, de cualquier armature)
        // pasa a ser el activo, SU rig el ACTIVO de la malla y su UV group homonimo el que se pinta.
        // true = engancho un hueso (el click no pinta). Separada de button_left para poder testearla.
        bool Bone2DElegirGrupoPorClick(Mesh* m, float lx, float ly);

        // ===== ARMATURES 2D DEL MESH (Mesh::armatures2d; ver W3dBone2D / Armature2D en el Core) =====
        // Los huesos se CREAN y EDITAN aca (menu Add de la barra + modos Huesos/Pose).
        // Publicos para el harness headless (comando uvarm2d).
        void Armature2DCrear(Mesh* m);      // crea el armature (1 hueso + su UV group) si no hay, y entra a UVModoHuesos
        int  Armature2DNuevo(Mesh* m);      // OTRO armature 2D independiente (1 hueso), activo + Edit Bones. idx
        int  Bone2DAgregar(Mesh* m);        // hueso RAIZ nuevo (con vertex group del mismo nombre). idx o -1
        int  Bone2DExtruir(Mesh* m);        // E: hueso nuevo desde el tail del ACTIVO (hijo conectado). idx o -1
        int  Bone2DDuplicar(Mesh* m);       // Shift+D: duplica los seleccionados (sueltos). idx del activo nuevo o -1
        bool Bone2DBorrar(Mesh* m);         // X/Supr: borra seleccionados; los hijos se re-parentan al abuelo
        bool Bone2DDesconectar(Mesh* m);    // Clear Parent: los seleccionados quedan raiz (= Bone2DClearParentSel)
        // A / Alt+A en Huesos/Pose: TODOS los huesos (con sus dos puntas) o ninguno
        void Bone2DSeleccionarTodos(Mesh* m, bool sel);
        // A / Alt+A en EDICION de UVs: todo / nada (en sync toca la seleccion del 3D y re-deriva)
        void SeleccionarTodoUV(Mesh* m, bool sel);
        // ===== menu Select de la barra (calcado del 3D) + sus teclas =====
        // Ctrl+I en EDICION de UVs: INVIERTE uvSelVert pero SOLO DENTRO DE LO VISIBLE (las esquinas
        // de las caras que pasan el filtro del 3D): lo que no se ve NUNCA queda seleccionado (sino
        // un G movia UVs invisibles). En sync invierte la seleccion del 3D y re-deriva el espejo.
        void InvertirSeleccionUV(Mesh* m);
        // L / menu Select > Select Linked en EDICION de UVs: selecciona la ISLA UV conectada.
        // semilla = render-vert de arranque (-1 = arranca de lo que YA esta seleccionado, que es
        // como entra desde el menu); extender = true suma a la seleccion (shift), false reemplaza.
        // La adyacencia es POR BORDE UV, no por posicion 3D (ver el comentario de la implementacion).
        // false = no habia semilla (nada seleccionado ni nada visible bajo el cursor).
        bool UVSeleccionarVinculado(Mesh* m, int semilla, bool extender);
        // render-vert VISIBLE mas cercano al cursor (lastMx/lastMy): la SEMILLA del Select Linked
        // por TECLA (L agarra la isla que esta bajo el mouse, como el L del 3D). -1 = nada visible.
        int  VertBajoCursorUV(Mesh* m) const;
        // Ctrl+I en Huesos/Pose: invierte la seleccion de huesos 2D (hueso entero, con sus puntas)
        void Bone2DInvertirSeleccion(Mesh* m);
        // L / Select Linked en Huesos/Pose: la CADENA conectada (la componente del ARBOL del
        // armature 2D: se recorre por padres e hijos) de los huesos seleccionados / del activo.
        // extender = suma a la seleccion. false = no habia semilla.
        bool Bone2DSeleccionarVinculado(Mesh* m, bool extender);
        // transform MODAL de huesos (G/R/S). En UVModoHuesos mueve/rota/escala el REST (las PUNTAS
        // seleccionadas: hueso entero = ambas; la compartida soldada arrastra a su par) de los
        // seleccionados; en UVModoPose edita la POSE 2D (poseT/poseRot/poseS) y re-aplica el
        // skinning UV en vivo (Mesh::Armature2DAplicar). Delta ABSOLUTO desde el arranque (sin drift).
        void Bone2DXformStart(Mesh* m, int modo);          // 1=mover 2=rotar 3=escalar
        void Bone2DDragEnd(Mesh* m, int bone, int mask, bool porClick); // deja SOLO esa punta seleccionada y arranca su drag
        void Bone2DDragSeleccion(Mesh* m, bool porClick);  // drag de LO SELECCIONADO (puntas/huesos; con soldadura)
        void Bone2DXformDelta(Mesh* m, float curU, float curV); // mouse actual (UV) -> aplica en vivo
        void Bone2DXformConfirm(Mesh* m);
        void Bone2DXformCancel(Mesh* m);
        bool Bone2DXformActivo() const;
        // pick de hueso bajo el mouse (px locales); pose=true usa la pose 2D y devuelve hueso entero.
        // En edicion las PUNTAS tienen prioridad (radio 12px) sobre el cuerpo (14px), como el 3D.
        // outMask: 1=head 2=tail 3=cuerpo/entero
        int  Bone2DPick(Mesh* m, float lx, float ly, bool pose, int* outMask);
        void Panear(float dx, float dy); // paneo de la vista UV (flechas; COMPARTIDO PC/Symbian -> FUERA del #ifndef SDL)
        void ZoomCentro(int dir);        // zoom centrado en el viewport (sin cursor): teclado 0+arriba/abajo (Symbian)
#ifndef W3D_SYMBIAN
        void event_key_down(int tecla, bool repeticion) override; // G/R/S + ESC/ENTER (transform)
        void event_mouse_wheel(float dy, int mx, int my) override;
        // IMPRESCINDIBLE: resetear ViewPortClickDown al soltar (sino queda en true y
        // viewPortActive se CONGELA -> el borde verde no cambia + no se puede resize).
        void mouse_button_up(int boton) override;
#endif
};

// dropdown "Texture" de la barra del UV editor: fija a mano que parte/textura del modelo se muestra (-1 = auto).
// Lo llama LayoutClickBarraUV (LayoutInput.cpp) al elegir una opcion del menu.
void UVSetTexOverride(Mesh* m, int part);
// que parte/textura se esta MOSTRANDO (el override si sigue valiendo, sino la parte activa).
// La usa el nombre del boton de la barra y el test 'uipunteros' (el override va por SERIAL de
// la malla, no por puntero: la direccion se recicla y se quedaba pegado a OTRA malla).
int  UVParteMostrada(Mesh* m);

// SELECCION EFECTIVA de UVs para los consumidores de AFUERA del editor (la tarjeta "Transform UV"
// del panel Properties). DECISION (modelo de seleccion nuevo): si hay un editor UV VIVO con
// seleccion PROPIA (syncSelection OFF) y esa seleccion no esta vacia, mandan ESOS render-verts
// -- es lo que el usuario ve marcado en blanco en el UV, y es POR COPIA (una costura no arrastra
// a sus hermanas). Si no hay editor UV con seleccion propia (o esta vacia), cae a la seleccion de
// EDIT MODE del 3D expandida por posRep (Mesh::VertsSelPorModo), como siempre. Devuelve false si
// no hay NADA seleccionado por ninguno de los dos caminos. La usan el centro que MUESTRA la
// tarjeta y el mover que APLICA: los dos leen el mismo conjunto (sino el centro mentiria).
bool UVVertsSelEfectivos(Mesh* m, std::vector<char>& sv);

// NUCLEO COMPARTIDO del mover UVs por valores exactos: desplaza (dU,dV) los UV de los verts de
// UVVertsSelEfectivos (la seleccion propia del UV si la hay; si no la del 3D), con el MISMO
// tratamiento que el confirmar de un G de UVs: undo liviano (UndoUV*), invalidacion del rest del
// skinning 2D con pose identidad, y re-subida del VBO. Lo usa la tarjeta "Transform UV" del panel
// Properties. Devuelve true si movio algo.
bool UVMoverSeleccionEdit(Mesh* m, float dU, float dV);

// UNA pasada del PINCEL DE PESOS del UV (UVModoPesos) en la posicion actual del mouse del editor
// (lastMx/lastMy). La llaman el click y el drag; se declara aca para que el harness pueda dar una
// pasada sin simular SDL. Escribe el UV GROUP activo y, si hay un rig 2D POSADO (cualquiera de los
// armatures de la malla, no solo el activo), re-aplica el skinning 2D.
class UVEditor;
void UVPintarPesos(UVEditor* uv, Mesh* m);

// ===== API LIBRE del ARMATURE 2D (sin instancia del editor): la comparten la tarjeta
// "Armature 2D" del panel Properties, los atajos Ctrl+P / Alt+P (Parent.cpp) y el harness. =====
// algun editor UV VIVO esta en modo de huesos (Edit Bones / Pose)? El primero que este (o NULL).
// Lo usa la tarjeta del panel para mostrarse SOLO mientras se editan huesos 2D.
UVEditor* UVEditorEnModoHuesos();
// idem pero incluyendo el MODO OBJETO (el default): es el criterio de la TARJETA "Armature 2D"
// del panel (lista de rigs + Add/Rename/Delete), que en modo objeto tambien tiene que verse.
UVEditor* UVEditorConRig2D();
// solo POSE / solo EDICION de UVs (el resto igual que arriba). Los usa InsertarKeyframeContexto
// para rutear la 'i' y el menu Animation al insert que corresponde.
UVEditor* UVEditorEnModoPose();
UVEditor* UVEditorEnModoEdicionUV();
// ESTE viewport se queda con el TAB? true solo para un UV EDITOR operativo cuya malla YA tiene
// armature 2D. Es un PREDICADO (no cambia nada): lo consulta controles.cpp para NO disparar el Tab
// global (Object<->Edit Mode del 3D / ciclar viewport) y dejar que la tecla siga su ruteo normal
// hasta UVEditor::event_key_down, el UNICO lugar que togglea (TabToggleHuesos).
bool UVEditorTomaTab(ViewportBase* vp);
// seleccion por puntas/huesos (misma semantica que BoneSel* del 3D; sincronizan la punta SOLDADA)
bool Bone2DEsConectado(Mesh* m, int b);                   // conectado = flag + head pegado al tail del padre
// INVARIANTE: 'conectado' solo puede estar en true si el hueso TIENE padre y su head cae en el tail de ese
// padre (misma tolerancia que Bone2DEsConectado). Las ops que re-parentan SIN mover el hueso (borrar el del
// medio, Clear Parent, duplicar, el desplegable Parent, Ctrl+P) dejarian el flag mintiendo: esto lo apaga
// donde ya no corresponde. NO mueve nada. Lo llaman esas ops; devuelve true si tuvo que corregir alguno.
bool Bone2DSanearConectado(Mesh* m);
void Bone2DSelLimpiar(Mesh* m);
void Bone2DSelHueso(Mesh* m, int b, bool sel);            // hueso ENTERO (select + ambas puntas)
void Bone2DSelPunta(Mesh* m, int b, int punta, bool sel); // una punta (1=head 2=tail) + sus soldadas
// jerarquia (validando ciclos; re-parentar REORDENA topologico para conservar padre < hijo)
bool Bone2DEsDescendiente(Mesh* m, int ancestro, int b);
bool Bone2DReparentar(Mesh* m, int bone, int nuevoPadre); // dropdown Parent (keep offset; undo adentro)
// Ctrl+P (menu de 2 opciones): seleccionados -> hijos del ACTIVO, en UN paso de undo.
// conectar=false -> "Keep Offset" (no se mueve nada, linea punteada al padre);
// conectar=true  -> "Connected" (el hijo se MUEVE para pegar su head al tail del padre + soldado).
bool Bone2DParentarSeleccionAlActivo(Mesh* m, bool conectar = false);
// UNICO camino de conectar/desconectar (checkbox "Connected" del panel, Ctrl+P > Connected y
// Alt+P > Disconnect Bone operan este MISMO flag). Conectar suelda (mueve el hueso); desconectar
// solo apaga el flag. Undo adentro; false = no cambio nada (sin padre o ya estaba asi).
bool Bone2DSetConectado(Mesh* m, int bone, bool con);
bool Bone2DDesconectarSel(Mesh* m);                       // Alt+P > Disconnect Bone (sigue emparentado, sin soldar)
bool Bone2DClearParentSel(Mesh* m);                       // Alt+P > Clear Parent (padre = -1)
// renombra el hueso (nombre unico) Y los UV GROUPS homonimos del mesh (el rig 2D bindea por nombre
// contra los UV groups -- pesos por corner --, NO contra los vertex groups del 3D; ver Mesh.h).
// Las dos puntas del binding viajan en UN SOLO paso de undo (UndoBone2DRenameCapturar).
bool Bone2DRenombrar(Mesh* m, int idx, const std::string& nuevo);
// mueve (du,dv) las puntas SELECCIONADAS (con soldadura), en un paso de undo. La usa la fila
// Pos de la tarjeta "Armature 2D" del panel. false = no habia seleccion (ni activo).
bool Bone2DMoverSeleccion(Mesh* m, float du, float dv);

#endif
