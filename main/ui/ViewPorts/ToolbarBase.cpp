// ===================================================================================================
//  TOOLBAR COMPARTIDA de los viewports (barra de HERRAMIENTAS de abajo). Mecanismo UNICO subido
//  desde ViewPort3D_Toolbar.cpp (donde nacio) a ViewportBase, para que el UV editor y el Editor 2D
//  tengan la misma barra sin duplicar nada:
//    - ToolButtons por ROL (btn->rol; reordenar no rompe el dispatch)
//    - layout con scroll horizontal clampeado (ToolbarActualizar)
//    - hit TOLERANTE al click (tap desviado agarra el boton mas cercano en X)
//    - render (fondo translucido + botones, mismo patron que RenderBar)
//    - MRU (ToolbarMRU): historial de acciones por editor, sin repetir, max 8
//  La parte CONTEXTUAL (que botones se ven y que hacen) es de cada editor, en dos virtuales:
//  ToolbarSincronizar() (estado puro, sin GL -> testeable headless) y ToolbarAccionRol(rol).
//  El Viewport3D la usa IGUAL que antes (cero regresion): sus overrides viven en ViewPort3D_Toolbar.cpp.
// ===================================================================================================
#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"     // T(): los textos salen en el idioma del sistema
#include "ViewPorts/ViewPorts.h"
#include "variables.h"   // enum TBMove..TBDelete (ids del historial de acciones)

namespace gfx = w3dEngine;
extern bool g_redraw;

// ---- MRU compartido: 'id' pasa al frente, sin repetidos, hasta 8 entradas ----
void ToolbarMRU(std::vector<int>& hist, int id){
    for (size_t i = 0; i < hist.size(); i++)
        if (hist[i] == id){ hist.erase(hist.begin() + i); break; } // sin repetir
    hist.insert(hist.begin(), id);                                 // la ultima usada, primera
    if (hist.size() > 8) hist.pop_back();                          // hasta 8
    g_redraw = true;
}

// ---- texto de una accion del historial (los ids TB* de variables.h), traducido ----
const char* ToolbarAccionLabel(int id){
    switch (id){
        case TBMove:    return T("Move");
        case TBRotate:  return T("Rotate");
        case TBScale:   return T("Scale");
        case TBExtrude: return T("Extrude");
        case TBLoopCut: return T("Loop Cut");
        case TBDelete:  return T("Delete");
    }
    return "?";
}

// default: la toolbar existe si el editor puso botones. Cada editor la restringe por contexto
// (el 3D: cfg.nuevoUsuario en Symbian; el UV: solo en edicion; el 2D: no en vista de juego).
bool ViewportBase::ToolbarVisible() const { return !ToolButtons.empty(); }

int ViewportBase::ToolbarHeight() const { return BarHeight(); }

bool ViewportBase::OnToolbar(int px, int py) const {
    if (ToolButtons.empty() || !ToolbarVisible()) return false;
    int barH = ToolbarHeight();
    int yBar = y + height - barH; // pegada abajo (la barra de menu esta arriba)
    return px >= x && px < x + width && py >= yBar && py < yBar + barH;
}

void ViewportBase::ToolbarScrollBy(int delta){
    toolScroll -= delta;
    if (toolScroll < 0) toolScroll = 0;
    ToolbarActualizar(); // re-clampea contra el ancho total actual
    g_redraw = true;
}

// visibilidad CONTEXTUAL (virtual de cada editor) + layout comun: anchos, clamp del scroll y
// los sx/sy absolutos con el scroll aplicado. Mismo patron que la barra de arriba, en dos
// pasadas (primero el ancho total para clampear, despues las posiciones).
void ViewportBase::ToolbarActualizar(){
    ToolbarSincronizar(); // que se ve / que dice cada boton: lo decide el editor

    int barH = ToolbarHeight();
    int yBar = y + height - barH;
    int btnGap = gapGS / 2 + 1;
    int total = gapGS;
    for (size_t i = 0; i < ToolButtons.size(); i++){
        Button* btn = ToolButtons[i];
        if (!btn->visible) continue;
        btn->Resize(width - gapGS * 2); // (btn->cuadrado fuerza ancho = alto, ej: X/Y/Z, Undo/Redo)
        total += btn->width + btnGap;
    }
    int maxS = total - width; if (maxS < 0) maxS = 0;
    if (toolScroll > maxS) toolScroll = maxS;
    int bx = gapGS - toolScroll;
    for (size_t i = 0; i < ToolButtons.size(); i++){
        Button* btn = ToolButtons[i];
        if (!btn->visible){ btn->sx = -10000; btn->sy = -10000; btn->hover = false; continue; }
        btn->sx = x + bx;
        btn->sy = yBar + (barH - btn->height) / 2;
        btn->hover = btn->Contains(lastMouseX, lastMouseY);
        bx += btn->width + btnGap;
    }
}

bool ViewportBase::ToolbarClick(int mx, int my){
    if (!OnToolbar(mx, my)) return false;
    ToolbarActualizar(); // sx/sy frescos
    // Buscar el boton: match EXACTO, o si el toque cayo al lado (dedo gordo sobre botones chicos) el MAS
    // CERCANO en X dentro de una tolerancia. Asi un tap un poco desviado igual pulsa el boton que se queria
    // en vez de caer en el gap y no hacer nada. La barra es una franja horizontal -> decide el X.
    Button* target = NULL;
    for (size_t i = 0; i < ToolButtons.size(); i++){       // 1) match exacto
        Button* btn = ToolButtons[i];
        if (btn->visible && btn->Contains(mx, my)) { target = btn; break; }
    }
    if (!target){                                          // 2) sin exacto: el mas cercano en X
        int mejorD = 1 << 30;
        for (size_t i = 0; i < ToolButtons.size(); i++){
            Button* b = ToolButtons[i];
            if (!b->visible) continue;
            int cx = b->sx + b->width / 2;
            int d = (mx > cx) ? (mx - cx) : (cx - mx);
            if (d < mejorD) { mejorD = d; target = b; }
        }
        if (target && mejorD > target->width) target = NULL; // gap real (lejos): sin accion
    }
    if (target){
        ToolbarAccionRol(target->rol); // la accion es del editor (virtual)
        g_redraw = true;
    }
    return true; // dentro de la barra: consumir igual (no pasa al contenido)
}

void ViewportBase::RenderToolbar(){
    if (ToolButtons.empty() || !ToolbarVisible() || !barCard) return;
    ToolbarActualizar();
    int barH = ToolbarHeight();

    gfx::PushMatrix();
    gfx::Translatef(0, (GLfloat)(height - barH), 0);
    // fondo translucido como la barra de arriba (barAlpha del viewport)
    const float* gris = ListaColores[static_cast<int>(ColorID::gris)];
    gfx::Color4f(gris[0], gris[1], gris[2], barAlpha);
    barCard->Resize(width, barH);
    barCard->RenderObject(false);
    // botones en sus sx/sy (ya con scroll). Local = sx-x, sy-y-(height-barH). Mismo patron que RenderBar.
    for (size_t i = 0; i < ToolButtons.size(); i++){
        Button* btn = ToolButtons[i];
        if (!btn->visible) continue;
        btn->alpha = barAlpha;   // misma translucidez que el fondo de la barra
        gfx::PushMatrix();
        gfx::Translatef((GLfloat)(btn->sx - x), (GLfloat)(btn->sy - y - (height - barH)), 0);
        btn->Render();
        gfx::PopMatrix();
    }
    gfx::PopMatrix();
}
