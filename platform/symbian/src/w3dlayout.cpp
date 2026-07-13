/*
 * ==============================================================================
 *  w3dlayout.cpp — ver w3dlayout.h y PLAN-UNIFICACION.md (Fase 1.5)
 *
 *  Arbol de viewports compartido (core/ui) en Symbian:
 *   - horizontal: Row( Viewport3D | Properties, 0.7 ); vertical: Column.
 *   - Bordes redondeados con el MISMO 9-patch que PC (WithBorder.cpp):
 *     el arte vive en font.png (region 115..128 del atlas de 128x128) y los
 *     indices omiten la celda central. Borde accent = viewport con el mouse
 *     encima (hover), negro = inactivo, igual que PC.
 *   - Properties: muestra y EDITA pos/rot/escala del objeto seleccionado.
 *     Click en una fila la selecciona; la rueda ajusta el valor (los pasos
 *     vienen de PropFloat de PC: +-0.1; rotacion usa 1 grado por "click" de
 *     rueda porque la rueda es discreta, no continua como las flechas de PC).
 *   - Driver del N95: los arrays solo se reconstruyen cuando CAMBIA el
 *     contenido (texto) o el tamano (borde); nunca entre draws.
 *   - Escala global: PC usa SetGlobalScale(3); el N95 a 240p usa 1.
 * ==============================================================================
 */

#include "w3dlayout.h"
#include "w3dmouse.h" // cursor virtual (W3dMouseWarp para el ruteo)
#include <e32keys.h>
#include "ViewPorts/WithBorder.h" // ViewportBase REAL de PC
#include "ViewPorts/Outliner.h" // Outliner REAL de PC
#include "ViewPorts/ViewPort3D.h" // Viewport3D REAL de PC
#include "ViewPorts/Properties.h" // Properties REAL de PC
#include "WhiskUI/card.h"
#include "ViewPorts/ScrollBar.h"
#include "objects/Textures.h"
#include "objects/Materials.h"            // Material (cargar textura)
#include "ViewPorts/PopUp/FileBrowser.h"  // AbrirFileBrowser (explorador compartido)
#include "w3dnewscene.h"                   // W3dNewImportObj
#include "WhiskUI/icons.h"
extern int W3dPantallaAlto;
extern bool MouseWheel;
#include "WhiskUI/font.h"      // Font/WhiskFont REALES de PC
#include "WhiskUI/bitmapText.h"
#include "WhiskUI/colores.h"
#include "WhiskUI/W3dFont.h"
#include "w3dlog.h"
#include "w3dnewscene.h" // mundo nuevo (Fase 3c-2)
#include "ViewPorts/LayoutInput.h" // ruteo COMPARTIDO (menus/barras/paneles)
#include "render/OpcionesRender.h"      // g_redraw (render event-driven)
#include <GLES/gl.h>
#include <stdio.h>     // sprintf para formatear los valores

// globals que siguen viviendo en Whisk3D.cpp hasta la limpieza final

class SymViewportProperties;

static ViewportBase* gRoot = 0;
static CWhisk3D* gWhisk = 0;
static TInt gScreenW = 0;
static TInt gScreenH = 0;
static TBool gHorizontal = ETrue;

static Viewport3D* gView3D = 0; // el REAL de PC
static Properties* gProps = 0; // el REAL de PC
static Outliner* gOutliner = 0; // el REAL de PC
static ViewportBase* gContRaiz = 0;   // contenedor raiz (Row o Column)
static ViewportBase* gContHijo = 0;   // contenedor anidado
static TBool gRaizEsRow = EFalse;     // tipo del raiz (el hijo es lo opuesto)

static ViewportBase* gSplitterGrab = 0;

// ----------------------------------------------------------------------------
// borde redondeado: 9-patch identico al de PC (WithBorder.cpp).
// UVs constantes: region 115..128 del atlas font.png (128x128).
// ----------------------------------------------------------------------------

static const GLfloat KBorderUV[32] = {
    // fila V=115/128
    0.898438f, 0.898438f,   0.945313f, 0.898438f,   0.953125f, 0.898438f,   1.0f, 0.898438f,
    // fila V=121/128
    0.898438f, 0.945313f,   0.945313f, 0.945313f,   0.953125f, 0.945313f,   1.0f, 0.945313f,
    // fila V=122/128
    0.898438f, 0.953125f,   0.945313f, 0.953125f,   0.953125f, 0.953125f,   1.0f, 0.953125f,
    // fila V=128/128
    0.898438f, 1.0f,        0.945313f, 1.0f,        0.953125f, 1.0f,        1.0f, 1.0f
};

// mismos indices que PC: 16 triangulos, la celda CENTRAL no se dibuja
static const GLushort KBorderIdx[48] = {
    0,1,4,  1,4,5,    1,2,5,  5,2,6,    2,3,6,  6,3,7,
    4,5,8,  8,5,9,                      6,7,10, 10,7,11,
    8,9,12, 12,9,13,  9,10,13, 13,10,14, 10,11,14, 14,11,15
};

// esquina del 9-patch: 6px a escala 1 (PC: 6 * GlobalScale)
static const TInt KBorderCorner = 6;

// UVs expandidos por indice (el driver del N95 NO dibuja glDrawElements en
// la fase 2D del frame -- probado por biseccion; glDrawArrays si funciona)
static GLfloat gBorderUVExp[96];
static TBool gBorderUVInit = EFalse;

// setea ortho de pantalla en pixeles (push); RestaurarOrtho hace el pop
static void SetOrthoPantalla() {
    glViewport(0, 0, gScreenW, gScreenH);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrthof(0.0f, (GLfloat)gScreenW, (GLfloat)gScreenH, 0.0f, -5.0f, 1000.0f);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
}

static void RestaurarOrtho() {
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

static void ClearRect(TInt aX, TInt aY, TInt aW, TInt aH, const float* aRGBA) {
    if (aW < 1 || aH < 1) return;
    glScissor(aX, gScreenH - aY - aH, aW, aH);
    glEnable(GL_SCISSOR_TEST);
    glClearColor(aRGBA[0], aRGBA[1], aRGBA[2], aRGBA[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
}

// dibuja el 9-patch de una hoja (aMesh = 16 vertices ya armados por Resize)
// texturas point-sprite de los glifos y las esquinas del borde (las
// fabrica W3dLayoutBuildGlyphTex cuando el TexMgr sube font.png)

// envuelve el DibujarBordes REAL de PC (WithBorder.cpp) con el ortho y el
// estado que la fase 2D del N95 necesita
static void DibujarBordeLeaf(ViewportBase* aLeaf, WithBorder* aBorde) {
    if (Textures.empty() || Textures[0]->iID == 0) return;
    SetOrthoPantalla();
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, Textures[0]->iID);
    glTranslatef((GLfloat)aLeaf->x, (GLfloat)aLeaf->y, 0.0f);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY); // el render 3D lo apaga
    glDisableClientState(GL_NORMAL_ARRAY);
    aBorde->DibujarBordes(aLeaf); // <- el de PC, identico arte y colores
    glEnableClientState(GL_NORMAL_ARRAY);
    glBindTexture(GL_TEXTURE_2D, (GLuint)W3dNewWhiteTex());
    glDisable(GL_BLEND);
    RestaurarOrtho();
}

// (DrawBorder9 borrado: el borde es WithBorder::DibujarBordes de PC)


// arma los 16 vertices del 9-patch para un rect w x h (solo en Resize)
// (BuildBorderMesh borrado: WithBorder::ResizeBorder de PC)

// (W3dLayoutBuildGlyphTex se borro: la UI la arma W3dInitUI compartido)

// (el texto por glifos del puente se borro: ahora dibuja RenderBitmapText
//  de PC via el shim de glesdraw)

// ----------------------------------------------------------------------------
// Hojas Symbian
// ----------------------------------------------------------------------------

// (SymViewport3D borrado: corre el Viewport3D REAL de PC)

// (el outliner puente se borro: ahora corre el Outliner REAL de PC)

// (el panel puente de propiedades se borro: corre el Properties REAL de PC)

// ----------------------------------------------------------------------------
// API
// ----------------------------------------------------------------------------

static void W3dArbolCambiadoHook();

// ----------------------------------------------------------------------------
// Import OBJ y Cargar textura via el File browser COMPARTIDO (igual que PC, ver
// main/main.cpp). Symbian ya no usa dialogos nativos: el explorador es el mismo.
// ----------------------------------------------------------------------------
extern void RebindMaterialMeshPart(); // Properties.cpp

static void W3dImportObjElegido(const std::string& aPath) {
    W3dNewImportObj(aPath.c_str());
}
static void W3dImportObjMenu() {
    AbrirFileBrowser("Importar modelo", "Import Wavefront OBJ", ".obj",
                     W3dImportObjElegido);
}

// Import FBX: mismo explorador compartido, filtrando .fbx (antes "Add > Import FBX" no hacia nada en el N95
// porque el puntero LayoutImportFbx quedaba sin cablear -> ver el cableado abajo).
static void W3dImportFbxElegido(const std::string& aPath) {
    W3dNewImportFbx(aPath.c_str());
}
static void W3dImportFbxMenu() {
    AbrirFileBrowser("Importar FBX", "Import FBX", ".fbx",
                     W3dImportFbxElegido);
}

extern bool gCargarTexturaComoNormal; // Properties.cpp: el "Load Texture" del normal map lo prende (compartido 4 OS)
static Material* gSymTexMat = NULL;
static void W3dTexturaElegida(const std::string& aPath) {
    if (!gSymTexMat) return;
    GLuint id = 0;
    if (LoadTexture(aPath.c_str(), id)) {
        Texture* t = new Texture(aPath);
        t->iID = id;
        Textures.push_back(t);
        if (gCargarTexturaComoNormal) { gSymTexMat->normalTexture = t; } // MISMO browser, destino segun el flag
        else { gSymTexMat->texture = t; gSymTexMat->textureOn = true; }
        RebindMaterialMeshPart();
    }
    gSymTexMat = NULL; gCargarTexturaComoNormal = false;
}
// "Load Texture" (base Y normal map): el MISMO browser en los 4 OS. Quien lo llama ya dejo el flag en el valor
// correcto (false=textura, true=normal); el callback de arriba decide el destino.
static void W3dCargarTexturaEn(Material* aMat) {
    gSymTexMat = aMat;
    AbrirFileBrowser(gCargarTexturaComoNormal ? "Cargar normal map" : "Cargar textura",
                     gCargarTexturaComoNormal ? "Abrir normal map" : "Abrir textura",
                     ".png .jpg .jpeg .bmp .tga", W3dTexturaElegida);
}

void W3dLayoutBuild(CWhisk3D* aWhisk, TInt aWidth, TInt aHeight) {
    gWhisk = aWhisk;
    gScreenW = aWidth;
    gScreenH = aHeight;
    g_redraw = true; // cambio de layout/orientacion -> redibujar
    SetGlobalScale(1); // 240p = escala 1 (PC usa 3)
    W3dPantallaAlto = aHeight;
    MenuPantallaW = aWidth;  // para que los desplegables clampeen
    MenuPantallaH = aHeight;


    TBool horizontal = (aWidth >= aHeight);

    // LAYOUT FIJO (pedido Dante): la orientacion (horizontal/vertical) elige el layout UNA SOLA VEZ, la PRIMERA
    // (cuando gRoot todavia no existe). Despues, al ROTAR la pantalla o redimensionar, NO se rebuildea otro
    // arbol distinto: se resize SIEMPRE el MISMO -> el viewport3D y los paneles no "saltan" de lugar al girar.
    if (gRoot) {
        gRoot->Resize(aWidth, aHeight);
        return;
    }

    delete gRoot;
    gSplitterGrab = 0;
    viewPortActive = 0;

    gView3D = new Viewport3D();
    Viewport3DActive = gView3D;
    gProps = new Properties();
    // SIN outliner por defecto (Dante casi no lo usa y lo sacaba en cada arranque). El arbol arranca con SOLO
    // 3D + Propiedades. gOutliner queda en 0 (todos sus usos lo chequean); si se quiere el outliner se agrega
    // desde el menu del viewport (Split + cambiar tipo) y W3dLayoutRecolectar lo recupera.
    gOutliner = 0;

    if (horizontal) {
        // pantalla horizontal: 3D a la izquierda | Propiedades a la derecha
        gRoot = new ViewportRow(gView3D, gProps, 0.7f);
        gRaizEsRow = ETrue;
    } else {
        // pantalla vertical (default N95): COLUMNA -> 3D arriba / Propiedades abajo (pedido Dante)
        gRoot = new ViewportColumn(gView3D, gProps, 0.7f);
        gRaizEsRow = EFalse;
    }
    gContHijo = 0; // arbol de 2 hojas: UN solo divisor (gContRaiz=gRoot). gContHijo se recalcula si se hace Split.
    gContRaiz = gRoot;
    gHorizontal = horizontal;

    // el ruteo COMPARTIDO (LayoutInput) opera sobre rootViewport
    rootViewport = gRoot;
    // sin mouse, SIEMPRE hay un viewport activo (borde verde) por defecto:
    // el 3D. La tecla verde de llamada lo cicla (W3dLayoutCiclarViewport).
    if (!viewPortActive) viewPortActive = gView3D;
    LayoutWarpMouse = W3dMouseWarp;
    LayoutArbolCambiado = W3dArbolCambiadoHook;
    LayoutImportObj = W3dImportObjMenu;        // Add > Import OBJ: el browser compartido
    LayoutImportFbx = W3dImportFbxMenu;        // Add > Import FBX: idem (antes NULL -> el item no hacia nada)
    DialogoCargarTextura = W3dCargarTexturaEn; // cargar textura: idem


    gRoot->x = 0;
    gRoot->y = 0;
    gRoot->Resize(aWidth, aHeight);

    w3dLogf("layout: %dx%d horizontal=%d (3D %dx%d, props %dx%d)",
        aWidth, aHeight, (TInt)horizontal,
        gView3D->width, gView3D->height, gProps->width, gProps->height);
}

void W3dLayoutRender() {
    if (!gRoot) return;

    glDisable(GL_SCISSOR_TEST);
    // matriz de TEXTURA siempre identidad: si algo viejo la toca, los UV
    // de TODO el dibujado texturizado colapsan (leccion aprendida 2 veces)
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glViewport(0, 0, gScreenW, gScreenH);
    glClearColor(ListaColores[static_cast<int>(ColorID::background)][0], ListaColores[static_cast<int>(ColorID::background)][1],
                 ListaColores[static_cast<int>(ColorID::background)][2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // renderiza el rootViewport COMPARTIDO (no gRoot): "Maximize" reemplaza rootViewport por el viewport
    // activo -> dibuja SOLO ese y los demas NO se renderizan ni recalculan (clave para el rendimiento del N95).
    // Sin maximizar rootViewport==gRoot (se sincronizan en el setup/expand), asi que el caso normal no cambia.
    if (rootViewport) rootViewport->Render();
    else gRoot->Render();

    LayoutRenderMenu(gScreenW, gScreenH); // desplegable abierto (compartido)
    { extern void NotificacionesRender(int, int); NotificacionesRender(gScreenW, gScreenH); } // toasts encima (export/import)
    LayoutTickFPS(User::NTickCount());     // overlay de fps (reloj de Symbian, ~ms)

    // diagnostico: estado del GL al terminar el arbol (cada ~128 frames)
    {
        static TInt diag = 0;
        if ((++diag & 0x7F) == 1) {
            w3dLogf("post-arbol: err=%x luz=%d l0=%d cm=%d tex2d=%d fog=%d sci=%d alto=%d outl=%d,%d %dx%d",
                glGetError(),
                (TInt)glIsEnabled(GL_LIGHTING), (TInt)glIsEnabled(GL_LIGHT0),
                (TInt)glIsEnabled(GL_COLOR_MATERIAL), (TInt)glIsEnabled(GL_TEXTURE_2D),
                (TInt)glIsEnabled(GL_FOG), (TInt)glIsEnabled(GL_SCISSOR_TEST),
                W3dPantallaAlto,
                gOutliner ? gOutliner->x : -1, gOutliner ? gOutliner->y : -1,
                gOutliner ? gOutliner->width : -1, gOutliner ? gOutliner->height : -1);
        }
    }
}

// hover: la hoja bajo el cursor lleva el borde accent (como viewPortActive
// en PC); con mas viewports 3D aca tambien se intercambiarian las camaras
void W3dLayoutMouseMoved(TInt aX, TInt aY) {
    if (!gRoot) return;
    g_redraw = true; // se movio el cursor -> redibujar (hover, etc.)
    ViewportBase* under = FindViewportUnderMouse(gRoot, aX, aY);
    if (under && under->isLeaf() && under != viewPortActive) {
        viewPortActive = under;
    }
}

TBool W3dLayoutMouseSobre3D(TInt aX, TInt aY) {
    if (!gRoot) return ETrue;
    // CUALQUIER viewport 3D cuenta (puede haber mas de uno)
    ViewportBase* under = FindViewportUnderMouse(gRoot, aX, aY);
    return under && under->ViewportKind() == 1;
}

// click sobre el panel de propiedades (el real de PC navega con botones;
// el click solo lo consume para no disparar seleccion 3D detras)
TBool W3dLayoutClickProps(TInt aX, TInt aY) {
    if (!gProps || !gProps->Contains(aX, aY)) return EFalse;
    gProps->ClickEn(aX, aY); // plegar/desplegar grupos (compartido con PC)
    return ETrue;
}

// click izquierdo sobre el outliner (seleccion de objeto del arbol)
TBool W3dLayoutClickOutliner(TInt aX, TInt aY) {
    if (!gOutliner || !gOutliner->Contains(aX, aY)) return EFalse;
    gOutliner->ClickSeleccionar(aX, aY); // metodo COMPARTIDO (tambien PC)
    return ETrue;
}

// modo scroll-arrastre con el derecho (toggle, limitacion del HID)
static ViewportBase* gScrollGrab2 = 0;

TBool W3dLayoutScrollGrab(TInt aMx, TInt aMy) {
    if (gScrollGrab2) {
        gScrollGrab2 = 0; // cualquier derecho lo suelta
        w3dLog("scroll-arrastre: soltado");
        return ETrue;
    }
    if (!gRoot) return EFalse;
    ViewportBase* under = FindViewportUnderMouse(gRoot, aMx, aMy);
    if (under && under->isLeaf() &&
        (under->ViewportKind() == 2 || under->ViewportKind() == 3)) {
        gScrollGrab2 = under;
        w3dLog("scroll-arrastre: agarrado (derecho de nuevo para soltar)");
        return ETrue;
    }
    return EFalse;
}

// movimiento -> el panel bajo el mouse (drag de scrollbar de PC + hover)
void W3dLayoutMouseMotion(TInt aMx, TInt aMy) {
    if (!gRoot) return;
    // menu abierto / hover de barras / viewport 3D activo: compartido
    if (LayoutMotionUI(aMx, aMy)) {
        return;
    }
    if (Viewport3DActive) {
        gView3D = (Viewport3D*)Viewport3DActive; // siguio al hover
    }
    if (gScrollGrab2) {
        // modo scroll XY: el movimiento scrollea como el boton del medio
        // de PC (middleMouseDown alimenta a Scrollable::ScrollX/Y)
        middleMouseDown = true;
        gScrollGrab2->event_mouse_motion(aMx, aMy);
        middleMouseDown = false;
        return;
    }
    ViewportBase* under = FindViewportUnderMouse(gRoot, aMx, aMy);
    if (under && under->isLeaf()) {
        under->event_mouse_motion(aMx, aMy); // el de PC (Scrollable/hover)
    }
}

// ---- el menu de tipo, expand/split, barras y paneles viven en
// main/ViewPorts/LayoutInput.cpp (COMPARTIDO con PC/Android) ----

// recolecta los punteros que el shell Symbian cachea (gView3D/etc)
static void W3dRecolectarPaneles(ViewportBase* aNodo) {
    if (!aNodo) return;
    if (aNodo->isLeaf()) {
        switch (aNodo->ViewportKind()) {
            case 1: if (!gView3D) gView3D = (Viewport3D*)aNodo; break;
            case 2: if (!gOutliner) gOutliner = (Outliner*)aNodo; break;
            case 3: if (!gProps) gProps = (Properties*)aNodo; break;
        }
        return;
    }
    if (aNodo->ContainerKind() == 1) {
        W3dRecolectarPaneles(((ViewportRow*)aNodo)->childA);
        W3dRecolectarPaneles(((ViewportRow*)aNodo)->childB);
    } else if (aNodo->ContainerKind() == 2) {
        W3dRecolectarPaneles(((ViewportColumn*)aNodo)->childA);
        W3dRecolectarPaneles(((ViewportColumn*)aNodo)->childB);
    }
}

static ViewportBase* W3dPrimerContenedor(ViewportBase* aNodo) {
    if (!aNodo || aNodo->isLeaf()) return 0;
    ViewportBase* a = 0;
    ViewportBase* b = 0;
    if (aNodo->ContainerKind() == 1) {
        a = ((ViewportRow*)aNodo)->childA;
        b = ((ViewportRow*)aNodo)->childB;
    } else {
        a = ((ViewportColumn*)aNodo)->childA;
        b = ((ViewportColumn*)aNodo)->childB;
    }
    if (a && !a->isLeaf()) return a;
    if (b && !b->isLeaf()) return b;
    return 0;
}

// hook de LayoutInput: el arbol cambio (cambio de tipo / expand / split)
static void W3dArbolCambiadoHook() {
    gRoot = rootViewport; // expand puede cambiar el root
    gScrollGrab2 = 0;
    gSplitterGrab = 0;
    gView3D = 0;
    gOutliner = 0;
    gProps = 0;
    W3dRecolectarPaneles(gRoot);
    gContRaiz = gRoot->isLeaf() ? 0 : gRoot;
    gContHijo = W3dPrimerContenedor(gRoot);
    if (gContRaiz) gRaizEsRow = (gRoot->ContainerKind() == 1);
    w3dLog("layout: arbol de viewports cambiado (LayoutInput)");
}

TBool W3dLayoutMenuAbierto() {
    return LayoutMenuAbierto() ? ETrue : EFalse;
}

// hay un popup modal abierto (File browser / color picker)? El container lo
// usa para mandarle el teclado (flechas/OK/soft) aunque no haya mouse BT.
TBool W3dLayoutPopupActivo() {
    return PopUpActive ? ETrue : EFalse;
}

// tecla verde de llamada: cicla el viewport activo (borde verde) sin mouse.
void W3dLayoutCiclarViewport(TInt aDir) {
    LayoutCiclarViewportActivo(aDir);
}

// soft-izquierda: abre/cierra la barra de menu del viewport activo.
void W3dLayoutToggleBarra() {
    LayoutToggleBarraViewportActivo();
}

// verde + flechas: redimensiona el viewport activo en un eje (paso chico)
void W3dLayoutRedimensionarViewport(TInt aDx, TInt aDy) {
    LayoutRedimensionarViewportActivo(aDx, aDy, 0.01f);
}

// el viewport ACTIVO (borde verde) es un viewport 3D? (para que las flechas
// orbiten ahi y no en el outliner/propiedades)
TBool W3dLayout3DActivo() {
    return (viewPortActive && viewPortActive->isLeaf() &&
            viewPortActive->ViewportKind() == 1) ? ETrue : EFalse;
}

// ctrl+P / alt+P: menus de emparentar (compartidos)
void W3dLayoutMenuParent(TBool aClear, TInt aX, TInt aY) {
    LayoutMenuParent(aClear ? true : false, aX, aY);
}

// shift+S: menu de snap (cursor/seleccion), compartido estilo Blender
void W3dLayoutMenuSnap(TInt aX, TInt aY) {
    LayoutMenuSnap(aX, aY);
}

// boton izquierdo SOLTADO: scroll agarrado + drop del drag del outliner
void W3dLayoutSoltar(TInt aX, TInt aY) {
    LayoutSoltar(aX, aY);
}

// estas "en medio de algo" (scroll agarrado / divisor): el cursor del
// mouse se pinta VERDE para que se note
TBool W3dLayoutOcupado() {
    if (LayoutEnArrastre()) return ETrue;
    if (gScrollGrab2) return ETrue;
    if (gSplitterGrab) return ETrue;
    return EFalse;
}

// mantiene el mouse ADENTRO del panel mientras se arrastra con el
// izquierdo: sale por el lado contrario con un margen (como PC)
TBool W3dLayoutWrapMouse(TInt& aX, TInt& aY) {
    ViewportBase* v = viewPortActive;
    if (!v || !v->isLeaf()) return EFalse;
    if (v->ViewportKind() != 2 && v->ViewportKind() != 3) return EFalse;
    TInt margen = borderGS;
    TBool warped = EFalse;
    if (aX <= v->x + margen) {
        aX = v->x + v->width - margen - 1;
        warped = ETrue;
    } else if (aX >= v->x + v->width - margen) {
        aX = v->x + margen + 1;
        warped = ETrue;
    }
    if (aY <= v->y + margen) {
        aY = v->y + v->height - margen - 1;
        warped = ETrue;
    } else if (aY >= v->y + v->height - margen) {
        aY = v->y + margen + 1;
        warped = ETrue;
    }
    return warped;
}

// el popup (color picker) esta arrastrando un valor? (cursor VIOLETA)
TBool W3dLayoutArrastrePopup() {
    return LayoutPopupArrastrando() ? ETrue : EFalse;
}

// toda la UI compartida (menu abierto > barras > paneles) en un solo punto
TBool W3dLayoutClickUI(TInt aX, TInt aY) {
    // el IZQUIERDO tambien confirma el modo scroll del click derecho
    if (gScrollGrab2) {
        gScrollGrab2 = 0;
        return ETrue;
    }
    if (LayoutClickUI(aX, aY)) {
        return ETrue;
    }
    return EFalse;
}

// teclado -> el viewport BAJO EL MOUSE (como PC: el hover decide quien
// recibe el teclado). Todos los paneles, no solo propiedades.
TBool W3dLayoutTecla(TInt aScan, TInt aMx, TInt aMy) {
    // traduccion Symbian -> teclas abstractas del ruteo compartido
    int k = -1;
    switch (aScan) {
        case EStdKeyUpArrow:    k = LayoutKey::Up; break;
        case EStdKeyDownArrow:  k = LayoutKey::Down; break;
        case EStdKeyLeftArrow:  k = LayoutKey::Left; break;
        case EStdKeyRightArrow: k = LayoutKey::Right; break;
        case EStdKeyDevice3: // OK del telefono
        case EStdKeyEnter:      k = LayoutKey::Enter; break;
        case EStdKeyBackspace: // C del telefono
        case EStdKeyEscape:     k = LayoutKey::Cancel; break;
        // soft keys del telefono: con el CBA vacio (whisk3D.rss) llegan como
        // teclas crudas 164 (IZQ) / 165 (DER), igual que en Half-Life.
        case 164:               k = LayoutKey::Cancel; break; // soft IZQ = Cancelar/volver
        case 165:               k = LayoutKey::Accept; break; // soft DER = Import/abrir
    }
    if (k < 0) return EFalse;
    if (LayoutTeclaUI(k, aMx, aMy)) {
        return ETrue;
    }
    // OK/Enter sobre el viewport con una malla seleccionada = TAB de PC: entra o sale de
    // Edit Mode. LayoutToggleEditMode ya guarda contra transforms en curso y objetos no-malla.
    if (k == LayoutKey::Enter && LayoutToggleEditMode()) {
        return ETrue;
    }
    return EFalse;
}

// EDICION NUMERICA por texto (un PropFloat): el contenedor rutea los digitos del keypad al campo enfocado.
// W3dNumEditActivo dice si hay una edicion numerica en curso; W3dTextFieldChar mete un caracter (0-9, '.', 8=borrar).
extern bool NumEditActivo();        // WhiskUI/PropFloat.cpp
extern bool TextFieldInputChar(int c); // main/ViewPorts/LayoutInput.cpp (declarada en TextField.h)
TBool W3dNumEditActivo() { return NumEditActivo() ? ETrue : EFalse; }
void  W3dTextFieldChar(TInt c) { TextFieldInputChar((int)c); }

// flecha MANTENIDA (frame-based) al popup activo: SOLO ajusta valores (color picker: R/G/B/A o circulo/
// value). El container la llama CADA frame segun gHeld* -> subir/bajar un color manteniendo es fluido.
// NO navega (la navegacion entre elementos sigue 1-por-tap via W3dLayoutTecla en el key-down).
TBool W3dLayoutTeclaRepeat(TInt aScan) {
    int k = -1;
    switch (aScan) {
        case EStdKeyUpArrow:    k = LayoutKey::Up; break;
        case EStdKeyDownArrow:  k = LayoutKey::Down; break;
        case EStdKeyLeftArrow:  k = LayoutKey::Left; break;
        case EStdKeyRightArrow: k = LayoutKey::Right; break;
    }
    if (k < 0) return EFalse;
    return LayoutPopupRepeat(k) ? ETrue : EFalse;
}

// editor UV activo: paneo CONSTANTE por flecha mantenida (o zoom si aZoom=0-mantenido). ETrue si lo manejo.
TBool W3dLayoutUVNav(TInt aDx, TInt aDy, TBool aZoom) {
    return LayoutUVNavFrame(aDx, aDy, aZoom ? true : false) ? ETrue : EFalse;
}
// query: el viewport activo es el editor UV? (LayoutUVNavFrame con 0,0 no panea, solo devuelve si es UV)
TBool W3dLayoutUVActivo() { return LayoutUVNavFrame(0, 0, false) ? ETrue : EFalse; }

// Timeline activo: flecha MANTENIDA = scrub (izq/der); 0-mantenido + arriba/abajo = zoom; * -mantenido + flechas
// = paneo. ETrue si lo manejo (es el Timeline).
TBool W3dLayoutTimelineNav(TInt aDx, TInt aDy, TBool aZoom, TBool aPan) {
    return LayoutTimelineNavFrame(aDx, aDy, aZoom ? true : false, aPan ? true : false) ? ETrue : EFalse;
}
// query: el viewport activo es el Timeline? (con 0,0 no hace nada, solo devuelve si es Timeline)
TBool W3dLayoutTimelineActivo() { return LayoutTimelineNavFrame(0, 0, false, false) ? ETrue : EFalse; }

// keypad SIN mouse: rutea la flecha/OK al viewport ACTIVO (propiedades/outliner)
TBool W3dLayoutTeclaPanel(TInt aScan) {
    int k = -1;
    switch (aScan) {
        case EStdKeyUpArrow:    k = LayoutKey::Up; break;
        case EStdKeyDownArrow:  k = LayoutKey::Down; break;
        case EStdKeyLeftArrow:  k = LayoutKey::Left; break;
        case EStdKeyRightArrow: k = LayoutKey::Right; break;
        case EStdKeyDevice3:
        case EStdKeyEnter:      k = LayoutKey::Enter; break;
        case EStdKeyBackspace: // C del telefono
        case EStdKeyEscape:     k = LayoutKey::Cancel; break; // cancela el modo mover del outliner
    }
    if (k < 0) return EFalse;
    return LayoutTeclaPanelActivo(k) ? ETrue : EFalse;
}

// el viewport ACTIVO es el outliner? (para que "1" entre al modo mover ahi, sin mouse)
TBool W3dOutlinerActivo() {
    return (viewPortActive && viewPortActive->isLeaf() &&
            viewPortActive->ViewportKind() == 2) ? ETrue : EFalse;
}

// "1" sobre el outliner: entra al modo MOVER; si ya esta moviendo, confirma (toggle).
void W3dOutlinerMoverToggle() {
    if (!W3dOutlinerActivo()) return;
    Outliner* out = (Outliner*)viewPortActive;
    if (out->ModoMover()) out->MoverConfirmar();
    else                  out->MoverIniciar();
}

// rueda sobre el outliner: scroll REAL de PC
TBool W3dLayoutWheelOutliner(TInt aX, TInt aY, TInt aDelta) {
    if (!gOutliner || !gOutliner->Contains(aX, aY)) return EFalse;
    MouseWheel = true;
    gOutliner->ScrollY(aDelta * 12 * GlobalScale); // x2: iba muy lento
    MouseWheel = false;
    return ETrue;
}

// rueda sobre el panel: ajusta la fila activa; consume el evento aunque no
// haya fila activa (la rueda sobre el panel nunca hace zoom de camara)
TBool W3dLayoutWheelProps(TInt aX, TInt aY, TInt aDelta) {
    if (!gProps || !gProps->Contains(aX, aY)) return EFalse;
    MouseWheel = true;
    gProps->ScrollY(aDelta * 6 * GlobalScale);
    MouseWheel = false;
    return ETrue;
}

// click: agarra el divisor si el mouse esta sobre el (o lo suelta si ya
// habia uno agarrado). Devuelve ETrue si el click fue consumido.
TBool W3dLayoutToggleSplitter(TInt aX, TInt aY) {
    if (!gRoot) return EFalse;

    if (gSplitterGrab) {
        gSplitterGrab = 0; // soltar
        w3dLog("layout: divisor soltado");
        return ETrue;
    }

    ViewportBase* under = FindViewportUnderMouse(gRoot, aX, aY);
    if (under && !under->isLeaf()) {
        gSplitterGrab = under;
        w3dLog("layout: divisor agarrado (otro click para soltar)");
        return ETrue;
    }
    return EFalse;
}

TBool W3dLayoutDragging() {
    return gSplitterGrab != 0;
}

void W3dLayoutDragMove(TInt aDx, TInt aDy) {
    if (!gSplitterGrab) return;
    // el tipo del contenedor se conoce por puntero (sin RTTI en RVCT)
    TBool esRow = (gSplitterGrab == gContRaiz) ? gRaizEsRow : !gRaizEsRow;
    if (esRow) {
        ((ViewportRow*)gSplitterGrab)->SetSizeChildrens(aDx);
    } else {
        // (SetSizeChildrens ahora es arriba-izquierda en los 4 OS)
        ((ViewportColumn*)gSplitterGrab)->SetSizeChildrens(aDy);
    }
}

// rect actual del viewport 3D (para el picking del mundo nuevo)
void W3dLayoutGet3DRect(TInt& aX, TInt& aY, TInt& aW, TInt& aH, TInt& aScreenH) {
    if (Viewport3DActive) {
        aX = Viewport3DActive->x; aY = Viewport3DActive->y;
        aW = Viewport3DActive->width; aH = Viewport3DActive->height;
    } else {
        aX = 0; aY = 0; aW = gScreenW; aH = gScreenH;
    }
    aScreenH = gScreenH;
}

// libera el arbol de viewports COMPLETO. OJO: los dtors de ViewportRow/Column borran childB pero NO childA
// (a proposito, para evitar doble-free) -> un `delete gRoot` a secas FUGA toda la cadena de childA (el 3D, etc.).
// Bajamos a mano y NULL-eamos los hijos antes del delete (mismo patron que LayoutBorrarSubarbol en LayoutInput.cpp).
static void W3dBorrarArbol(ViewportBase* aNodo) {
    if (!aNodo) return;
    if (!aNodo->isLeaf()) {
        if (aNodo->ContainerKind() == 1) {
            ViewportRow* r = (ViewportRow*)aNodo;
            W3dBorrarArbol(r->childA); W3dBorrarArbol(r->childB);
            r->childA = NULL; r->childB = NULL;
        } else {
            ViewportColumn* c = (ViewportColumn*)aNodo;
            W3dBorrarArbol(c->childA); W3dBorrarArbol(c->childB);
            c->childA = NULL; c->childB = NULL;
        }
    }
    delete aNodo;
}

void W3dLayoutDestroy() {
    W3dBorrarArbol(gRoot); // libera TODO el arbol (sin fugar el childA chain). gOutliner ya esta dentro si se agrego.
    gRoot = 0;
    gView3D = 0;
    gProps = 0;
    gOutliner = 0;
    gContRaiz = 0;
    gContHijo = 0;
    viewPortActive = 0;
    gSplitterGrab = 0;
    gWhisk = 0;
}
