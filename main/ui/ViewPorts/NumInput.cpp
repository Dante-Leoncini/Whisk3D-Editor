// ===================================================================================================
//  ENTRADA NUMERICA / FORMULAS durante un transform (compartida 4 OS). Extraido de LayoutInput (Fase 2).
// ===================================================================================================
#include "input/W3dClipboard.h"   // el portapapeles del sistema (Ctrl+C / Ctrl+V de las cajas de texto)
#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "Undo.h" // Ctrl+Z: capturar modo / seleccion
#include "ViewPorts/PopUp/ConfirmPopup.h" // OpenConfirmDelete (popup de confirmar borrado)
#include "ViewPorts/LayoutInput.h"
#include "ViewPorts/Pick3D.h"   // InsetActive / BevelActive: tambien escriben su valor exacto
#include "ViewPorts/PoseTransform.h" // Pose Mode transform (extraido a su propio archivo)
#include "ViewPorts/Notifications.h" // toasts (extraido a su propio archivo)
#include "ViewPorts/ViewPort3D.h"
#include "ViewPorts/Outliner.h"
#include "ViewPorts/Properties.h"
#include "ViewPorts/UVEditor.h"
#include "ViewPorts/Timeline.h"
#include "WhiskUI/draw/glesdraw.h"
#include "WhiskUI/draw/rectangle.h" // el velo del modo foco
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/Materials.h" // Material (mat->texture) para el dropdown "Texture" del UV editor
#include "objects/Textures.h"  // Texture (path) para las etiquetas del dropdown
#include "objects/EditMesh.h"
#include "objects/Light.h"
#include "objects/Camera.h"
#include "objects/Empty.h"
#include "objects/Armature.h"
#include "animation/SkeletalAnimation.h" // InsertKeyframeSkeleton (Pose Mode: Insert Keyframe)
#include "objects/Instance.h"
#include "objects/Collection.h"
#include "objects/ObjectMode.h"
#include "edit/Modifier.h" // ModifierType::Mirror + target (regen de mirrors al mover objetos)
#include "objects/Primitives.h"
#include "variables.h"
#include "render/RenderOptions.h" // g_currentFps
#include "ViewPorts/PopUp/PopUpBase.h"
#include "ViewPorts/PopUp/RedoMeshPanel.h"
#include "WhiskUI/widgets/card.h"        // tarjeta de las notificaciones
#include "WhiskUI/text/bitmapText.h"  // texto de las notificaciones
#include "WhiskUI/draw/icons.h"       // iconos notifOk / notifError
#include "WhiskUI/theme/colors.h"     // ColorID
#include "w3dlog.h"         // las notificaciones tambien van al log
#include "ViewPorts/NumInput.h"
#include <cstdio>          // sprintf (NumInputSet)

// ====================================================================
//  ENTRADA NUMERICA / FORMULAS durante un transform (COMPARTIDA 4 OS)
// ====================================================================
// Mientras se mueve/rota/escala, el usuario puede TIPEAR un valor exacto en vez de
// hacerlo a ojo con el mouse: numeros, punto decimal, parentesis y * / + (formulas
// tipo "(3+10)/2"). El '-' alterna el signo (no es resta). Es PLATFORM-AGNOSTIC: el
// nucleo (buffer + evaluador + apply) vive aca; cada SO alimenta los caracteres por
// NumInputChar (PC desde SDL_TEXTINPUT; Symbian desde su teclado/keypad).
static std::string gNumBuf;       // la expresion tipeada (sin el signo)
static bool        gNumActive = false;
static bool        gNumNegate = false; // el toggle del '-'
static int         gNumCaret  = 0;     // posicion del caret en gNumBuf (para editar en el medio; teclado tactil ← →)

// --- evaluador de expresiones (+ * / parentesis, numeros con punto). Parse de
//     numero MANUAL (sin strtod) para no depender del locale (',' vs '.'). ---
struct ExprP { const char* s; bool ok; };
static float ExprExpr(ExprP& p);
static void ExprSkip(ExprP& p){ while(*p.s==' ') p.s++; }
static float ExprNum(ExprP& p){
    ExprSkip(p);
    if (*p.s=='('){ p.s++; float v=ExprExpr(p); ExprSkip(p); if(*p.s==')') p.s++; else p.ok=false; return v; }
    if (*p.s=='+'){ p.s++; return ExprNum(p); }
    if (*p.s=='-'){ p.s++; return -ExprNum(p); }
    bool any=false; float ip=0;
    while(*p.s>='0'&&*p.s<='9'){ ip=ip*10.0f+(float)(*p.s-'0'); p.s++; any=true; }
    if (*p.s=='.'){ p.s++; float sc=1.0f; while(*p.s>='0'&&*p.s<='9'){ sc*=0.1f; ip+=(float)(*p.s-'0')*sc; p.s++; any=true; } }
    if (!any){ p.ok=false; return 0.0f; }
    return ip;
}
static float ExprTerm(ExprP& p){
    float v=ExprNum(p);
    for(;;){ ExprSkip(p); char c=*p.s;
        if(c=='*'){ p.s++; v*=ExprNum(p); }
        else if(c=='/'){ p.s++; float d=ExprNum(p); v = (d!=0.0f)? v/d : 0.0f; }
        else break; }
    return v;
}
static float ExprExpr(ExprP& p){
    float v=ExprTerm(p);
    for(;;){ ExprSkip(p); char c=*p.s;
        if(c=='+'){ p.s++; v+=ExprTerm(p); }
        else if(c=='-'){ p.s++; v-=ExprTerm(p); }
        else break; }
    return v;
}
static bool EvalExpr(const std::string& str, float& out){
    if (str.empty()){ out=0.0f; return true; }
    ExprP p; p.s=str.c_str(); p.ok=true;
    float v=ExprExpr(p); ExprSkip(p);
    if (!p.ok || *p.s!='\0') return false; // expresion incompleta/invalida
    out=v; return true;
}
// mismo parser, expuesto: lo usa la entrada numerica del TIMELINE (mover/escalar keyframes con matematica)
bool W3dEvalExpr(const std::string& str, float& out){ return EvalExpr(str, out); }
// el Enter de un PropFloat evalua la cuenta con este mismo parser (WhiskUI no conoce al editor: va por puntero)
extern bool (*g_propFloatEval)(const std::string&, float&);
static struct HookPropFloatEval { HookPropFloatEval() { g_propFloatEval = W3dEvalExpr; } } sHookPropFloatEval;

// CAJA DE TEXTO EDITABLE: el campo enfocado + el ruteo de caracteres (compartido).
// (g_activeTextField vive en WhiskUI: text/bitmapText.cpp)
// edicion numerica por texto de un PropFloat (WhiskUI/PropFloat): Enter aplica, Cancel descarta.
extern bool NumEditActive();
extern void NumEditCommit();
extern void NumEditCancel();
// CANCELAR con Esc/C (ver TextField.h): el texto que tenia el campo enfocado ANTES de tocarlo.
// gFocusOf = de que campo es (si el foco cambia, se vuelve a anotar).
static TextField*  gFocusOf = NULL;
static std::string gFocusText;
TextField* g_restoredTextField = NULL;   // el que un Esc descarto en ESTE cuadro (ver TextField.h)
void TextFieldFocus(TextField* f){
    g_activeTextField = f;
    g_restoredTextField = NULL;
    gFocusOf = f;
    gFocusText = f ? f->text : std::string();
}
void TextFieldFrameEnd(){
    TextFieldContinueFocus();
    g_restoredTextField = NULL;          // los duenos en vivo ya escribieron el texto restaurado
}
void TextFieldContinueFocus(){
    if (g_activeTextField == gFocusOf) return;
    gFocusOf = g_activeTextField;
    gFocusText = gFocusOf ? gFocusOf->text : std::string();
}
bool TextFieldCancel(){
    if (!g_activeTextField) return false;
    TextFieldContinueFocus();                 // (enfocado a mano y sin teclas todavia: no cambio nada)
    g_activeTextField->SetText(gFocusText);
    g_restoredTextField = g_activeTextField;   // los duenos EN VIVO lo re-escriben este cuadro
    g_activeTextField = NULL;
    gFocusOf = NULL;
    g_redraw = true;
    return true;
}
// EL PORTAPAPELES del campo enfocado (el del sistema: se comparte con el navegador, las notas, el IDE)
bool TextFieldCopy(){
    if (!g_activeTextField) return false;
    if (g_activeTextField->HasSelection()) w3dEngine::W3dClipboardSet(g_activeTextField->Selected());
    return true;
}
bool TextFieldCut(){
    if (!g_activeTextField) return false;
    TextFieldContinueFocus();
    if (g_activeTextField->HasSelection()) { w3dEngine::W3dClipboardSet(g_activeTextField->Selected()); g_activeTextField->DeleteSelection(); }
    g_redraw = true;
    return true;
}
bool TextFieldPaste(){
    if (!g_activeTextField) return false;
    TextFieldContinueFocus();
    const std::string t = w3dEngine::W3dClipboardGet();
    if (!t.empty()) g_activeTextField->InsertText(t);
    g_redraw = true;
    return true;
}
bool TextFieldSelectAll(){
    if (!g_activeTextField) return false;
    g_activeTextField->SelectAll();
    g_redraw = true;
    return true;
}
bool TextFieldInputChar(int c){
    if (!g_activeTextField) return false;
    TextFieldContinueFocus();                 // antes de la PRIMERA tecla: el texto de antes
    if (c == 8)       g_activeTextField->Backspace();
    else if (c == 127)g_activeTextField->DelForward();
    else if (c >= 32 && c < 127) {   // ASCII imprimible
        if (g_activeTextField->numeric && !TextField::CharNumeric(c)) return true;   // un numero: sin letras (se traga)
        g_activeTextField->InsertChar(c);
    }
    else return false;
    g_redraw = true;
    return true;
}

bool NumInputActive(){ return gNumActive; }
void NumInputReset(){ gNumBuf.clear(); gNumActive=false; gNumNegate=false; gNumCaret=0; }
const std::string& NumInputBuffer(){ return gNumBuf; }
int  NumInputCaret(){ return gNumCaret; }                 // posicion del caret (para dibujarlo en la barra)
void NumInputLeft(){  if (gNumCaret>0) { gNumCaret--; g_redraw=true; } }
void NumInputRight(){ if (gNumCaret<(int)gNumBuf.size()) { gNumCaret++; g_redraw=true; } }
bool NumInputNegated(){ return gNumNegate; }
// valor actual (false si la expresion esta incompleta -> no aplicar todavia)
bool NumInputValue(float& out){
    float v; if (!EvalExpr(gNumBuf, v)) return false;
    out = gNumNegate ? -v : v; return true;
}

// el viewport ACTIVO con un G/R/S MODAL propio (UV editor / Editor 2D), o NULL. Es el
// dispatch GENERICO del TransformUI compartido: si un editor 2D esta transformando, la
// entrada numerica va a SUS hooks (XformNumValue/Confirmar/Cancelar); si no, a los caminos
// del 3D de siempre (que no cambian: el 3D es la referencia).
static ViewportBase* NumInputVpXform(){
    return (viewPortActive && viewPortActive->XformInProgress()) ? viewPortActive : NULL;
}

// aplica el valor exacto al transform en curso (malla, objeto, o el editor 2D activo)
static void NumInputApply(){
    if (!gNumActive) return;
    // INSERTAR CARAS / BEVEL: el valor es el grosor / el ancho (lo leen de NumInputValue)
    if (InsetActive()) { InsetNumber(); return; }
    if (BevelActive()) { BevelNumber(); return; }
    float v; if (!NumInputValue(v)) return; // incompleta: espero mas caracteres
    { ViewportBase* vpx = NumInputVpXform();
      if (vpx){ vpx->XformNumValue(v); return; } } // UV editor / Editor 2D
    if (InteractionMode == EditMode && EditXformActive()) EditXformNumValue(v);
    else if (InteractionMode == EditMode){ // Edit de ARMATURE: el valor exacto va al transform de huesos
        extern int BoneXformMode(); extern void BoneXformNumValue(float);
        if (BoneXformMode()) BoneXformNumValue(v);
    }
    else if (InteractionMode == PoseMode){ extern void PoseXformNumValue(float); PoseXformNumValue(v); }
    else                                                  SetNumericTransform(v);
}

// COMPARTIDA: cada plataforma alimenta los caracteres tipeados aca. Devuelve true si
// lo consumio (hay un transform activo y el caracter es relevante). c==8 = backspace.
bool NumInputChar(int c){
    // solo durante un transform: el del 3D (estado global) o el MODAL propio del
    // viewport activo (UV editor / Editor 2D, hooks del TransformUI compartido)
    if (state == editNavigation && !NumInputVpXform() && !InsetActive() && !BevelActive()) return false;
    if (c == 8){ // backspace: borra el char ANTES del caret (o el signo si esta vacio)
        if (gNumCaret > 0 && !gNumBuf.empty()) { gNumBuf.erase(gNumCaret-1, 1); gNumCaret--; }
        else if (gNumBuf.empty()) gNumNegate = false;
    } else if (c == '-'){
        gNumNegate = !gNumNegate; gNumActive = true;
    } else if ((c>='0'&&c<='9') || c=='.' || c=='(' || c==')' || c=='*' || c=='/' || c=='+'){
        if (gNumCaret < 0) gNumCaret = 0; if (gNumCaret > (int)gNumBuf.size()) gNumCaret = (int)gNumBuf.size();
        gNumBuf.insert(gNumBuf.begin()+gNumCaret, (char)c); gNumCaret++; gNumActive = true; // inserta EN el caret
    } else {
        return false; // no es un caracter numerico
    }
    if (gNumBuf.empty() && !gNumNegate) gNumActive = false; // se vacio: vuelve el mouse
    NumInputApply();
    g_redraw = true;
    return true;
}

// El teclado tactil (NumPad en modo transform) llama a estas para editar el transform
// que se muestra ARRIBA (sin popup/textinput propio), igual que un teclado fisico en PC.
// el valor de una, como si se hubiera tipeado (con un decimal: es lo que se ve en la barra). Lo usa el telefono al
// ROTAR INCLINANDOLO: el angulo girado desde que se apreto el lapiz es el angulo del objeto
void NumInputSet(float v){
    char buf[32];
    const bool neg = v < 0.0f;
    float a = neg ? -v : v;
    const int ent = (int)a, dec = (int)((a - (float)ent) * 10.0f + 0.5f);
    if (dec >= 10) sprintf(buf, "%d", ent + 1);
    else if (dec > 0) sprintf(buf, "%d.%d", ent, dec);
    else sprintf(buf, "%d", ent);
    gNumBuf = buf; gNumCaret = (int)gNumBuf.size(); gNumNegate = neg; gNumActive = true;
    NumInputApply();
    g_redraw = true;
}

void NumInputBegin(){ // marca la entrada activa asi la barra muestra "[|]" apenas se abre el teclado
    if (!gNumActive){ gNumBuf.clear(); gNumCaret=0; gNumNegate=false; gNumActive=true; }
    g_redraw = true;
}
void NumInputConfirm(){ // Enter del teclado: confirma el transform (Aceptar ya hace NumInputReset)
    if (InsetActive()) { InsetConfirm(); g_redraw = true; return; }
    if (BevelActive()) { BevelConfirm(); g_redraw = true; return; }
    { ViewportBase* vpx = NumInputVpXform();
      if (vpx){ vpx->XformConfirm(); g_redraw = true; return; } } // UV / 2D (su confirmar resetea)
    if (Viewport3DActive) Viewport3DActive->Accept();
    g_redraw = true;
}
void NumInputCancel(){ // X del teclado: descarta el transform (mismo camino que la cruz de la barra)
    if (InsetActive()) { InsetCancel(); g_redraw = true; return; }
    if (BevelActive()) { BevelCancel(); g_redraw = true; return; }
    { ViewportBase* vpx = NumInputVpXform();
      if (vpx){ vpx->XformCancel(); NumInputReset(); g_redraw = true; return; } } // UV / 2D
    if (InteractionMode == EditMode && EditXformActive()) EditXformCancel();
    else Cancel(); // objeto: descarta el transform (free function de ObjectMode)
    NumInputReset();
    g_redraw = true;
}
// hay un transform en curso? (mismo criterio que la barra de estado que lo muestra). El teclado
// tactil en modo transform se cierra solo si esto deja de ser cierto (se confirmo/cancelo).
bool NumInputTransformInProgress(){
    if (NumInputVpXform()) return true; // G/R/S modal del UV editor / Editor 2D
    if (InsetActive() || BevelActive()) return true;   // insertar caras / bevel: su grosor / ancho
    if (!Viewport3DActive) return false;
    if (!(state == translation || state == rotation || state == EditScale)) return false;
    if (InteractionMode == ObjectMode) return true;
    if (InteractionMode == EditMode && EditXformActive()) return true;
    { extern int BoneXformMode();
      if (InteractionMode == EditMode && BoneXformMode()) return true; } // G/R/S de huesos
    if (InteractionMode == PoseMode && PoseHeaderMode()) return true;
    return false;
}
