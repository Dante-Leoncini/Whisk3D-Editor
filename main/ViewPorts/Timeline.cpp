#include "ViewPorts/Timeline.h"
#include "w3dGraphics.h"
#include "WhiskUI/glesdraw.h"          // W3dPantallaAlto
#include "WhiskUI/colores.h"           // ListaColores / ColorID
#include "WhiskUI/rectangle.h"         // Rec2D
#include "WhiskUI/bitmapText.h"        // RenderBitmapText / textAlign
#include "WhiskUI/UI.h"                // GlobalScale, RenglonHeightGS, CharacterWidthGS, LetterHeightGS, ...
#include "objects/Objects.h"           // ObjActivo
#include "objects/Armature.h"
#include "objects/Textures.h"          // Textures[0] = atlas (RenderBar/bordes)
#include "animation/Animation.h"       // StartFrame/EndFrame/CurrentFrame/PlayAnimation/AnimPlayDir + AnimProperty
#include "animation/SkeletalAnimation.h"
#include "render/OpcionesRender.h"     // g_redraw
#include "PopUp/PopUpBase.h"           // PopUpActive
#include "WhiskUI/card.h"              // Card (boton verde redondeado del frame actual)
#include "WhiskUI/PopupMenu.h"         // dropdown de seleccion de animacion
#include <math.h>
#include <string>
#include <vector>
#include <algorithm>

namespace gfx = w3dEngine;

extern bool leftMouseDown, middleMouseDown, ViewPortClickDown;

// roles de los botones de la barra (para el dispatch del click; no chocan con los BR_* del 3D)
enum { TL_ROL_T0 = 300, TL_ROL_START = 320, TL_ROL_END = 321, TL_ROL_ANIM = 322 };

// dropdown de animacion: menu JERARQUICO (Scenes + un submenu por armadura) construido por Properties.cpp; la
// seleccion (escena o clip) se aplica con AnimSelPorId (compartida con la tarjeta Animation).
extern void ConstruirMenuAnim(PopupMenu* menu); // Properties.cpp
extern void AnimSelPorId(int id);               // Properties.cpp
static PopupMenu* g_tlMenuAnim = NULL;
static void TL_menuAnimAction(int id){
    AnimSelPorId(id);
    if (MenuAbierto == g_tlMenuAnim) g_tlMenuAnim->Cerrar();
    g_redraw = true;
}

// verdes: fondo de botones de play activos / boton del frame actual, y la linea del playhead
static float TL_VERDE_BTN[4]  = { 0.12f, 0.45f, 0.18f, 1.0f };
static float TL_VERDE_LINEA[4]= { 0.20f, 0.80f, 0.32f, 1.0f };

// ------------------------------------------------------------------ helpers de dibujo (coords LOCALES, Y abajo)
static void FillRect(int px, int py, int w, int h){
    if (w <= 0 || h <= 0) return;
    static Rec2D* r = NULL; if (!r) r = new Rec2D();
    r->SetSize((GLshort)px, (GLshort)py, (GLshort)w, (GLshort)h);
    r->RenderObject(false); // usa el Color4f actual
}
static void FillTri(float ax, float ay, float bx, float by, float cx, float cy){
    static float v[6]; v[0]=ax; v[1]=ay; v[2]=bx; v[3]=by; v[4]=cx; v[5]=cy;
    gfx::VertexPointer2f(0, v); gfx::DrawTrianglesArray(3);
}
static void TriRight(float cx, float cy, float r){ FillTri(cx-r, cy-r, cx-r, cy+r, cx+r, cy); }
static void TriLeft (float cx, float cy, float r){ FillTri(cx+r, cy-r, cx+r, cy+r, cx-r, cy); }
static void VBar(float cx, float cy, float halfH, float w){ FillRect((int)(cx-w*0.5f), (int)(cy-halfH), (int)w, (int)(halfH*2)); }
static void Diamond(float cx, float cy, float r){
    static float v[12];
    v[0]=cx; v[1]=cy-r; v[2]=cx+r; v[3]=cy; v[4]=cx; v[5]=cy+r;
    v[6]=cx; v[7]=cy-r; v[8]=cx; v[9]=cy+r; v[10]=cx-r; v[11]=cy;
    gfx::VertexPointer2f(0, v); gfx::DrawTrianglesArray(6);
}
static void SetCol(ColorID::Enum c, float a = 1.0f){
    gfx::Color4f(ListaColores[(int)c][0], ListaColores[(int)c][1], ListaColores[(int)c][2], a);
}
// texto centrado horizontalmente en cx, con su top en py (necesita textura del atlas activa)
static void TextCentrado(float cx, int py, const std::string& s){
    int tw = (int)s.size() * CharacterWidthGS;
    gfx::PushMatrix();
    gfx::Translatef((float)((int)cx - tw/2), (float)py, 0);
    RenderBitmapText(s, textAlign::left, tw + GlobalScale*2);
    gfx::PopMatrix();
}
// glyph vectorial de transporte centrado en un rect (gx,gy,gw,gh)
static void DrawGlyph(int gx, int gy, int gw, int gh, int tipo, bool pausa){
    float cx = gx + gw*0.5f, cy = gy + gh*0.5f, r = gh*0.20f; // r chico -> deja padding en el boton cuadrado
    if (pausa){ VBar(cx - r*0.55f, cy, r, GlobalScale*1.6f); VBar(cx + r*0.55f, cy, r, GlobalScale*1.6f); return; }
    switch (tipo){
        case 0: VBar(cx - r*1.1f, cy, r, GlobalScale*1.6f); TriLeft(cx + r*0.4f, cy, r); break;       // |< inicio
        case 1: TriLeft(cx - r*0.4f, cy, r*0.9f); TriLeft(cx + r*0.9f, cy, r*0.9f); break;            // << kf ant
        case 2: TriLeft(cx + r*0.4f, cy, r*0.9f); VBar(cx - r*0.7f, cy, r*0.5f, GlobalScale); break;  // <| frame ant
        case 3: TriLeft(cx, cy, r); break;                                                            // <  play rev
        case 4: TriRight(cx, cy, r); break;                                                           // >  play
        case 5: TriRight(cx - r*0.4f, cy, r*0.9f); VBar(cx + r*0.7f, cy, r*0.5f, GlobalScale); break; // |> frame sig
        case 6: TriRight(cx + r*0.4f, cy, r*0.9f); TriRight(cx - r*0.9f, cy, r*0.9f); break;          // >> kf sig
        case 7: TriRight(cx - r*0.4f, cy, r); VBar(cx + r*1.1f, cy, r, GlobalScale*1.6f); break;      // >| final
    }
}

// keyframes (frames) de la ANIMACION ACTIVA (la que estamos viendo), NO del objeto seleccionado
static void CollectKeyframes(std::vector<int>& out){
    out.clear();
    if (ActiveAnimKind == 1 && ActiveAnimArm &&
        ActiveAnimArm->animActiva >= 0 && ActiveAnimArm->animActiva < (int)ActiveAnimArm->animations.size()){
        // clip de armature ACTIVO: sus keyframes (de todos los huesos)
        SkeletalAnimation* an = ActiveAnimArm->animations[ActiveAnimArm->animActiva];
        for (size_t t=0;t<an->tracks.size();t++)
            for (size_t p=0;p<an->tracks[t].Propertys.size();p++)
                for (size_t k=0;k<an->tracks[t].Propertys[p].keyframes.size();k++)
                    out.push_back(an->tracks[t].Propertys[p].keyframes[k].frame);
    } else {
        // animacion de ESCENA activa: keyframes de TODOS los objetos de la escena (AnimationObjects = escena activa)
        for (size_t i=0;i<AnimationObjects.size();i++)
            for (size_t p=0;p<AnimationObjects[i].Propertys.size();p++)
                for (size_t k=0;k<AnimationObjects[i].Propertys[p].keyframes.size();k++)
                    out.push_back(AnimationObjects[i].Propertys[p].keyframes[k].frame);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
}

// clip de animacion ACTIVO segun la seleccion APP-WIDE (no depende del objeto seleccionado) o NULL si la animacion
// activa es una ESCENA. Asi clickear un armature NO cambia el rango/seleccion del timeline.
static SkeletalAnimation* ClipActivo(){
    if (ActiveAnimKind == 1 && ActiveAnimArm &&
        ActiveAnimArm->animActiva >= 0 && ActiveAnimArm->animActiva < (int)ActiveAnimArm->animations.size())
        return ActiveAnimArm->animations[ActiveAnimArm->animActiva];
    return NULL;
}

// ------------------------------------------------------------------ onChange -> timeline activo
static Timeline* g_tlActivo = NULL;
static void TL_onStart(){ if (g_tlActivo) g_tlActivo->ApplyStart(); }
static void TL_onEnd()  { if (g_tlActivo) g_tlActivo->ApplyEnd();   }
static void TL_onCur()  { if (g_tlActivo) g_tlActivo->ApplyCur();   }

Timeline::Timeline(){
    pxPerFrame = (float)GlobalScale * 7.0f;
    viewStartF = -2.0f;                 // arranca mostrando un par de frames negativos (frame 0 prolijo)
    scrubbing = false; panning = false; lastMx = lastMy = 0; pressMx = pressMy = 0;
    stripY = numY = barH2 = 0; curBtnX = curBtnW = 0;
    fStart=(float)StartFrame; fEnd=(float)EndFrame; fCur=(float)CurrentFrame;

    pfStart = new PropFloat("Start"); pfStart->value=&fStart; pfStart->entero=true; pfStart->onChange=TL_onStart;
    pfEnd   = new PropFloat("End");   pfEnd->value=&fEnd;     pfEnd->entero=true;   pfEnd->onChange=TL_onEnd;
    pfCur   = new PropFloat("Frame"); pfCur->value=&fCur;     pfCur->entero=true;   pfCur->onChange=TL_onCur;

    BarCrear(); // [0] = icono/menu de tipo
    for (int i=0;i<8;i++){
        Button* b = new Button("");          // vacio + cuadrado: el glyph vectorial va encima
        b->rol = TL_ROL_T0 + i;
        b->cuadrado = true;                  // ancho = alto (con el padding de UIBotonAltura)
        BarButtons.push_back(b); btnT[i] = b;
    }
    btnStart = new Button("Start:1");   btnStart->rol = TL_ROL_START; BarButtons.push_back(btnStart);
    btnEnd   = new Button("End:250");   btnEnd->rol   = TL_ROL_END;   BarButtons.push_back(btnEnd);
    btnAnim  = new Button("Scene", (int)IconType::camera); btnAnim->rol = TL_ROL_ANIM; btnAnim->desplegable = true;
    btnAnim->caretMenu = true; // aca SI conviene la flechita (dropdown de animacion)
    // SIEMPRE visible (Scene por defecto); SyncFields actualiza texto/icono segun la animacion activa
    BarButtons.push_back(btnAnim);
#ifdef W3D_SYMBIAN
    // N95 (240px): la barra completa (8 transportes + Start/End + anim) es demasiada para navegar sin mouse.
    // Dejamos SOLO lo esencial: inicio(0), play(4), final(7) + Start/End + dropdown de animacion. Los demas se
    // OCULTAN (no se borran) -> siguen existiendo para PC/Android, donde la barra entra entera.
    btnT[1]->visible = false; // kf anterior
    btnT[2]->visible = false; // frame anterior
    btnT[3]->visible = false; // play reversa
    btnT[5]->visible = false; // frame siguiente
    btnT[6]->visible = false; // kf siguiente
#endif
    g_tlActivo = this;
}
Timeline::~Timeline(){
    if (g_tlActivo == this) g_tlActivo = NULL;
    delete pfStart; delete pfEnd; delete pfCur;
    // los Button de BarButtons los libera ~ViewportBase
}

void Timeline::Resize(int newW, int newH){ ViewportBase::Resize(newW, newH); ResizeBorder(newW, newH); }

float Timeline::FrameToX(float f) const { return (f - viewStartF) * pxPerFrame; }
float Timeline::XToFrame(float lx) const { return viewStartF + lx / (pxPerFrame>0.01f?pxPerFrame:0.01f); }
int Timeline::TickStep() const {
    float minPx = (float)GlobalScale * 26.0f;
    const int steps[] = {1,2,5,10,25,50,100,250,500,1000,2500,5000};
    for (int i=0;i<12;i++) if (steps[i]*pxPerFrame >= minPx) return steps[i];
    return 5000;
}

void Timeline::SyncFields(){
    // el rango/fps de la animacion activa YA vive en los globales StartFrame/EndFrame/AnimFPS (se cargan al
    // seleccionarla con AnimCargarRangoActivo). Aca solo se refleja en los campos.
    // Start / End: si NO se editan, mostrar el valor; si se editan, el boton es el input (editField)
    if (g_propFloatEditando == pfStart){ btnStart->editField = &pfStart->field; }
    else { btnStart->editField = NULL; char b[24]; snprintf(b,sizeof b,"Start:%d",StartFrame); btnStart->text=b; fStart=(float)StartFrame; }
    if (g_propFloatEditando == pfEnd){ btnEnd->editField = &pfEnd->field; }
    else { btnEnd->editField = NULL; char b[24]; snprintf(b,sizeof b,"End:%d",EndFrame); btnEnd->text=b; fEnd=(float)EndFrame; }
    if (g_propFloatEditando != pfCur) fCur=(float)CurrentFrame;

    // play activo -> tinte verde en el boton correspondiente
    bool pf = (PlayAnimation && AnimPlayDir>0), pr = (PlayAnimation && AnimPlayDir<0);
    btnT[3]->tinte = pr ? TL_VERDE_BTN : NULL;
    btnT[4]->tinte = pf ? TL_VERDE_BTN : NULL;

    // dropdown de animacion: SIEMPRE visible (Scene por defecto). Muestra la animacion ACTIVA: icono camara = escena,
    // icono esqueleto = clip de armature. No depende del objeto seleccionado.
    btnAnim->visible = true;
    SkeletalAnimation* c = ClipActivo();
    if (c){ btnAnim->text = c->name; btnAnim->icon = (int)IconType::armature; }
    else  { btnAnim->text = NombreEscenaActiva(); btnAnim->icon = (int)IconType::camera; }
}

// ------------------------------------------------------------------ Render
void Timeline::Render(){
    gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
    gfx::MatrixMode(gfx::ModelView);  gfx::LoadIdentity();

    const int glY = W3dPantallaAlto - y - height;
    gfx::Enable(gfx::ScissorTest); gfx::Scissor(x, glY, width, height);
    // fondo NEGRO (el area fuera de Start..End queda negra; el rango se pinta gris tarjeta encima)
    gfx::ClearColor(ListaColores[(int)ColorID::negro][0], ListaColores[(int)ColorID::negro][1],
                    ListaColores[(int)ColorID::negro][2], 1.0f);
    gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);
    gfx::Disable(gfx::ScissorTest);

    gfx::Viewport(x, glY, width, height);
    gfx::Ortho(0, width, height, 0, -1, 1);
    gfx::Disable(gfx::Fog); gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::CullFace);
    gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Texture2D); gfx::Disable(gfx::Blend);
    gfx::Enable(gfx::ColorMaterial);
    gfx::EnableArray(gfx::VertexArray); gfx::DisableArray(gfx::TexCoordArray); gfx::DisableArray(gfx::NormalArray);

    // layout vertical: [menu] [barra de numeros = alto de un input] [cuerpo]
    barH2 = LetterHeightGS + GlobalScale * 4;  // barra de numeros CHICA (tipo input), no del alto del menu
    numY  = BarTopOffset();                    // debajo del menu
    stripY = numY + barH2;                     // el cuerpo arranca debajo de la barra de numeros
    if (stripY > height) stripY = height;

    SyncFields();

    int step = TickStep();
    float fRight = viewStartF + width / (pxPerFrame>0.01f?pxPerFrame:0.01f);
    int f0 = ((int)floorf(viewStartF / step)) * step;
    int fN = (int)ceilf(fRight) + step;

    // ---------------- AREA REPRODUCIBLE [Start..End]: gris tarjeta (el resto queda NEGRO) ----------------
    { float xa = FrameToX((float)StartFrame), xb = FrameToX((float)EndFrame + 1);
      int ia = (int)(xa<0?0:xa), ib = (int)(xb>width?width:xb);
      if (ib > ia){ SetCol(ColorID::gris, 1.0f); FillRect(ia, stripY, ib-ia, height-stripY); } }

    // ---------------- LINEAS verticales (van del piso hasta tocar la barra de numeros) ----------------
    // ALTERNAN color: las "oscuras" (mas oscuras que la tarjeta) llevan numero; las "claras" (color del texto) no.
    for (int f = f0; f <= fN; f += step){
        float fx = FrameToX((float)f);
        if (fx < 0 || fx > width) continue;
        bool oscura = ((f / step) & 1) == 0;
        if (oscura) gfx::Color4f(0.03f, 0.03f, 0.03f, 1.0f);          // linea oscura (mas oscuro que la tarjeta)
        else        SetCol(ColorID::grisLinea, 1.0f);                 // linea clara (gris medio 0x494949)
        FillRect((int)fx, stripY, GlobalScale, height - stripY);
    }

    // keyframes: rombos cerca de la base del cuerpo
    std::vector<int> kfs; CollectKeyframes(kfs);
    float kfY = (float)(height - GlobalScale*5);
    for (size_t i=0;i<kfs.size();i++){
        float fx = FrameToX((float)kfs[i]);
        if (fx < -GlobalScale*3 || fx > width+GlobalScale*3) continue;
        SetCol(ColorID::blanco, 1.0f); Diamond(fx, kfY, (float)GlobalScale*3.0f);
    }

    // ---------------- PLAYHEAD (linea verde): del piso hasta ARRIBA de la barra de numeros (toca el boton verde) ----------------
    float px = FrameToX((float)CurrentFrame);
    if (px >= -GlobalScale && px <= width+GlobalScale){
        gfx::Color4fv(TL_VERDE_LINEA);
        FillRect((int)(px - GlobalScale*0.5f), numY, (int)(GlobalScale*1.5f), height - numY);
    }

    // ---------------- rect del boton VERDE del frame actual (se DIBUJA al final, arriba de todo) ----------------
    std::string curTxt; { bool ec=(g_propFloatEditando==pfCur);
        if (ec) curTxt = pfCur->field.text; else { char b[16]; snprintf(b,sizeof b,"%d",CurrentFrame); curTxt=b; } }
    int curTw = (int)curTxt.size()*CharacterWidthGS;
    curBtnW = curTw + GlobalScale*8; curBtnX = (int)px - curBtnW/2;
    int curBtnY = numY - GlobalScale*2, curBtnH = barH2 + GlobalScale*2; // POR ARRIBA de los numeros (sobresale)

    // ---------------- FASE CON TEXTURA: numeros de frame (solo sobre lineas oscuras) ----------------
    gfx::Enable(gfx::Texture2D); gfx::Enable(gfx::Blend); gfx::BlendAlpha(); gfx::EnableArray(gfx::TexCoordArray);
    if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    int numTop = numY + (barH2 - LetterHeightGS)/2;
    SetCol(ColorID::grisUI, 1.0f);
    for (int f = f0; f <= fN; f += step){
        if (((f / step) & 1) != 0) continue;         // clara -> sin numero
        float fx = FrameToX((float)f);
        if (fx < -GlobalScale*8 || fx > width+GlobalScale*8) continue;
        if (fx > curBtnX - GlobalScale && fx < curBtnX + curBtnW + GlobalScale) continue; // bajo el boton verde: lo tapa
        char b[16]; snprintf(b,sizeof b,"%d",f);
        TextCentrado(fx, numTop, b);
    }

    // ---------------- barra del viewport (menu con transporte + campos) ----------------
    gfx::EnableArray(gfx::TexCoordArray);
    if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    RenderBar();

    // glyphs vectoriales ENCIMA de los botones de transporte (sin textura, centrados en el boton cuadrado)
    gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
    bool pfwd = (PlayAnimation && AnimPlayDir>0), prev = (PlayAnimation && AnimPlayDir<0);
    SetCol(ColorID::blanco, 1.0f);
    for (int i=0;i<8;i++){
        Button* b = btnT[i]; if (!b->visible || b->sx < -9000) continue;
        int gx = b->sx - x, gy = b->sy - y;
        bool pausa = (i==3 && prev) || (i==4 && pfwd);
        DrawGlyph(gx, gy, b->width, b->height, i, pausa);
    }

    // ---------------- boton VERDE del frame actual: ARRIBA de todo (no lo recorta el menu) ----------------
    // relleno VERDE + borde VERDE redondeado (9-patch): todo verde, exterior transparente (como un boton normal).
    gfx::Enable(gfx::Texture2D); gfx::Enable(gfx::Blend); gfx::BlendAlpha(); gfx::EnableArray(gfx::TexCoordArray);
    if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    { static Card* cbtn = NULL; if (!cbtn) cbtn = new Card(NULL, 10, 10);
      cbtn->Resize(curBtnW, curBtnH);
      gfx::PushMatrix(); gfx::Translatef((float)curBtnX, (float)curBtnY, 0);
      gfx::Color4fv(TL_VERDE_LINEA); cbtn->RenderObject(false); // relleno verde
      gfx::Color4fv(TL_VERDE_LINEA); cbtn->RenderBorder(false); // borde verde redondeado (mismo color -> sin anillo negro)
      gfx::PopMatrix(); }
    SetCol(ColorID::blanco, 1.0f);
    TextCentrado(px, numY + (barH2 - LetterHeightGS)/2, curTxt);

    // borde del viewport (verde si activo)
    gfx::Enable(gfx::Texture2D); gfx::EnableArray(gfx::TexCoordArray);
    if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    DibujarBordes(this);
}

// ------------------------------------------------------------------ acciones
void Timeline::TogglePlay(int dir){
    if (PlayAnimation && AnimPlayDir==dir) PlayAnimation=false; else { PlayAnimation=true; AnimPlayDir=dir; }
    g_redraw = true;
}
void Timeline::GotoStart(){ CurrentFrame = StartFrame; g_redraw=true; }
void Timeline::GotoEnd(){   CurrentFrame = EndFrame;   g_redraw=true; }
void Timeline::StepFrame(int d){
    CurrentFrame += d;
    if (CurrentFrame < StartFrame) CurrentFrame = StartFrame;
    if (CurrentFrame > EndFrame)   CurrentFrame = EndFrame;
    g_redraw=true;
}
void Timeline::StepKeyframe(int d){
    std::vector<int> kfs; CollectKeyframes(kfs);
    if (kfs.empty()){ StepFrame(d); return; }
    if (d>0){ for (size_t i=0;i<kfs.size();i++) if (kfs[i]>CurrentFrame){ CurrentFrame=kfs[i]; g_redraw=true; return; } }
    else    { for (int i=(int)kfs.size()-1;i>=0;i--) if (kfs[i]<CurrentFrame){ CurrentFrame=kfs[i]; g_redraw=true; return; } }
}
// tope de paneo hacia la izquierda: ~1 pantalla hacia el lado negativo (comodo, nunca se pierde)
float Timeline::MinView() const {
    float px = (pxPerFrame>0.01f?pxPerFrame:0.01f);
    return -((float)width / px) * 0.85f;
}
void Timeline::PanFrames(float d){ viewStartF += d; if (viewStartF < MinView()) viewStartF = MinView(); g_redraw=true; }
void Timeline::ZoomBy(float factor, int cxLocal){
    float fb = XToFrame((float)cxLocal);
    pxPerFrame *= factor;
    if (pxPerFrame < 0.3f) pxPerFrame = 0.3f;
    if (pxPerFrame > 140.0f) pxPerFrame = 140.0f;
    viewStartF = fb - (float)cxLocal / pxPerFrame;
    if (viewStartF < MinView()) viewStartF = MinView();
    g_redraw = true;
}
void Timeline::SetFrameFromX(int lx){
    int f = (int)(XToFrame((float)lx) + 0.5f);
    if (f < StartFrame) f = StartFrame;
    if (f > EndFrame)   f = EndFrame;
    CurrentFrame = f; g_redraw = true;
}
void Timeline::ApplyStart(){ int v=(int)(fStart+0.5f); if(v<0)v=0;
    AnimSetStart(v);   // editar Start = editar el Start PROPIO de la animacion activa (escena o clip)
    if(CurrentFrame<StartFrame)CurrentFrame=StartFrame; g_redraw=true; }
void Timeline::ApplyEnd(){   int v=(int)(fEnd+0.5f);   if(v<0)v=0;
    AnimSetEnd(v);     // editar End = editar el End PROPIO de la animacion activa (escena o clip)
    if(CurrentFrame>EndFrame && EndFrame>=StartFrame)CurrentFrame=EndFrame; g_redraw=true; }
void Timeline::ApplyCur(){   int v=(int)(fCur+0.5f);   if(v<0)v=0; CurrentFrame=v; g_redraw=true; }
void Timeline::EditarCampo(int i){
    g_tlActivo = this;
    PropFloat* pf = (i==0)?pfStart : (i==1)?pfEnd : pfCur;
    if (i==0) fStart=(float)StartFrame; else if (i==1) fEnd=(float)EndFrame; else fCur=(float)CurrentFrame;
    pf->IniciarEdicionTexto();
    if (i==0) btnStart->editField=&pfStart->field; else if (i==1) btnEnd->editField=&pfEnd->field;
    g_redraw = true;
}
void Timeline::TransportAction(int i){
    switch (i){
        case 0: GotoStart(); break;      case 1: StepKeyframe(-1); break;
        case 2: StepFrame(-1); break;    case 3: TogglePlay(-1); break;
        case 4: TogglePlay(+1); break;   case 5: StepFrame(+1); break;
        case 6: StepKeyframe(+1); break; case 7: GotoEnd(); break;
    }
}

// ------------------------------------------------------------------ input
bool Timeline::ClickBarButton(int mx, int my){
    for (int i=0;i<8;i++) if (btnT[i]->visible && btnT[i]->Contains(mx,my)){ TransportAction(i); return true; }
    if (btnStart->Contains(mx,my)){ EditarCampo(0); return true; }
    if (btnEnd->Contains(mx,my)){   EditarCampo(1); return true; }
    // dropdown de animacion: menu jerarquico (submenu "Scenes" + un submenu por armadura con sus clips)
    if (btnAnim->visible && btnAnim->Contains(mx,my)){
        if (!g_tlMenuAnim){ g_tlMenuAnim = new PopupMenu(); g_tlMenuAnim->action = TL_menuAnimAction; }
        ConstruirMenuAnim(g_tlMenuAnim);
        if (MenuAbierto && MenuAbierto != g_tlMenuAnim) MenuAbierto->Cerrar();
        g_tlMenuAnim->Abrir(btnAnim->sx, btnAnim->sy + btnAnim->height, MenuPantallaW, MenuPantallaH);
        MenuAbierto = g_tlMenuAnim;
        return true;
    }
    return false;
}

void Timeline::button_left(){
#ifndef W3D_SYMBIAN
    if (PopUpActive) return;
    int lx = lastMx - x, ly = lastMy - y;
    // boton VERDE del frame actual (sobresale por arriba de los numeros) -> editar por texto (teclado / pad numerico)
    if (ly >= numY - GlobalScale*2 && ly < stripY && lx >= curBtnX && lx <= curBtnX + curBtnW){ EditarCampo(2); return; }
    // CLICK IZQUIERDO en TODO el cuerpo (numeros + bandas) = SCRUB: fija el frame YA (en el DOWN) y lo sigue con el
    // arrastre. El PAN/scroll del timeline es con el boton del MEDIO (ver event_mouse_motion). Antes el cuerpo paneaba
    // con el izquierdo y el frame recien saltaba al SOLTAR -> molesto.
    if (ly >= numY - GlobalScale*2){ scrubbing = true; panning = false; SetFrameFromX(lx); }
#endif
}
void Timeline::event_mouse_motion(int mx, int my){
    if (PopUpActive){ lastMx=mx; lastMy=my; return; }
    if (scrubbing && leftMouseDown)  SetFrameFromX(mx - x);                                              // izquierdo: scrub (arrastra el frame actual)
    else if (middleMouseDown)        PanFrames(-(float)(mx - lastMx) / (pxPerFrame>0.01f?pxPerFrame:0.01f)); // MEDIO: pan/scroll del timeline
    lastMx = mx; lastMy = my;
}
bool Timeline::event_finger_scroll(int px, int py, int dx, int dy){
    PanFrames(-(float)dx / (pxPerFrame>0.01f?pxPerFrame:0.01f)); return true;
}
void Timeline::event_finger_gesture(float zoomDelta, float panDx, float panDy){
    scrubbing = false; panning = false; // 2 dedos: es zoom/paneo de VISTA -> NO tocar el frame (cancela cualquier scrub)
    if (zoomDelta > 1.0f) ZoomBy(1.1f, width/2); else if (zoomDelta < -1.0f) ZoomBy(1.0f/1.1f, width/2);
    if (panDx != 0.0f) PanFrames(-panDx / (pxPerFrame>0.01f?pxPerFrame:0.01f));
}
#ifndef W3D_SYMBIAN
void Timeline::event_mouse_wheel(SDL_Event &e){
    if (PopUpActive) return;
    { int mx,my; SDL_GetMouseState(&mx,&my); if (BarScrollHorizontal(mx,my,(int)(e.wheel.y*40))) return; }
    ZoomBy(e.wheel.y>0 ? 1.1f : 1.0f/1.1f, lastMx - x);
}
void Timeline::mouse_button_up(SDL_Event &e){
    (void)e;
    // el frame YA se fijo en el DOWN + el arrastre (scrub); al soltar solo se limpia el estado.
    scrubbing=false; panning=false; ViewPortClickDown=false; g_redraw=true;
}
void Timeline::event_key_down(SDL_Event &e){
    if (PopUpActive || g_textFieldActivo) return;
    SDL_Keycode k = e.key.keysym.sym;
    if (k==SDLK_SPACE) { TogglePlay(+1); return; }
    if (k==SDLK_LEFT)  { StepFrame(-1); return; }
    if (k==SDLK_RIGHT) { StepFrame(+1); return; }
    if (k==SDLK_UP)    { StepKeyframe(+1); return; }
    if (k==SDLK_DOWN)  { StepKeyframe(-1); return; }
    if (k==SDLK_HOME)  { GotoStart(); return; }
    if (k==SDLK_END)   { GotoEnd(); return; }
}
#endif
