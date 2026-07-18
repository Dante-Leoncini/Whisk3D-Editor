// ===================================================================================================
//  OVERLAY de estadisticas del Viewport3D (arriba a la derecha): vertices, caras, modgen, ms, fps.
//  Extraido de ViewPort3D.cpp (Fase 2 del reorg). El metodo ya esta declarado en Viewport3D (ViewPort3D.h).
//  Solo LEE globales ya expuestos (g_profShow del profiler, g_genMallaCount/g_fpsActual de OpcionesRender).
// ===================================================================================================
#include "w3dGraphics.h"                 // w3dEngine::Color4f/PushMatrix/Translatef/PopMatrix
#include "ViewPorts/ViewPort3D.h"        // Viewport3D + members; arrastra variables.h (SceneCollection)
#include "WhiskUI/text/bitmapText.h"     // RenderBitmapText + textAlign
#include "WhiskUI/theme/colores.h"       // ListaColores / ColorID
#include "WhiskUI/core/UI.h"             // gapGS, LetterHeightGS
#include "W3dProfile.h"                  // g_profShow (ms por categoria del frame)
#include "render/OpcionesRender.h"       // g_genMallaCount, g_fpsActual
#include "objects/Mesh.h"               // Mesh (contar vertices/caras de la escena)
#include <cstdio>                        // sprintf

// suma recursiva de las estadisticas de malla de toda la escena
static void W3dContarMallas(Object* o, int& vAgr, int& vReal, int& fLog, int& fTri){
    if (!o) return;
    if (o->getType() == ObjectType::mesh){
        Mesh* m = (Mesh*)o;
        vAgr  += (m->vertsAgrupados > 0) ? m->vertsAgrupados : m->vertexSize;
        vReal += m->vertexSize;
        fLog  += !m->faces3d.empty() ? (int)m->faces3d.size() : (m->facesSize / 3);
        fTri  += m->facesSize / 3;
    }
    for (size_t i = 0; i < o->Childrens.size(); i++)
        W3dContarMallas(o->Childrens[i], vAgr, vReal, fLog, fTri);
}

// dibuja una linea del overlay alineada a la derecha, en la fila actual, y avanza a la siguiente
static void StatLinea(const char* buf, int width, int margen, int& ly, int lineH){
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)(width - margen), (GLfloat)ly, 0);
    RenderBitmapText(buf, textAlign::right, width);
    w3dEngine::PopMatrix();
    ly += lineH;
}

// texto blanco arriba a la derecha: vertices agrupados/reales, caras logicas/
// triangulos y fps. Se llama dentro del pase 2D de RenderUI (ortho + fuente ya
// seteados). Los contadores por malla estan precalculados (no se cuenta por frame).
void Viewport3D::RenderEstadisticas(){
    if (!showOverlays) return;
    if (!OverlayStatVertices && !OverlayStatFaces && !OverlayStatModgen && !OverlayStatTimes && !OverlayFps) return;
    SetColorID(ColorID::blanco);
    const int margen = gapGS * 2;
    const int lineH = LetterHeightGS + gapGS;
    int ly = (barAbajo ? 0 : BarHeight()) + gapGS; // debajo de la barra si esta arriba
    char buf[64];
    if (OverlayStatVertices || OverlayStatFaces){
        int vAgr=0, vReal=0, fLog=0, fTri=0;
        W3dContarMallas(SceneCollection, vAgr, vReal, fLog, fTri);
        if (OverlayStatVertices){ sprintf(buf, "vertex: %d/%d", vAgr, vReal); StatLinea(buf, width, margen, ly, lineH); }
        if (OverlayStatFaces){    sprintf(buf, "faces: %d/%d", fLog, fTri);   StatLinea(buf, width, margen, ly, lineH); }
    }
    if (OverlayStatModgen){
        // DIAGNOSTICO de performance: regeneraciones de la malla de modificadores. Al ROTAR la camara NO debe subir
        // (la subdivision/screw se cachea en genValido). Si sube al rotar -> se esta recalculando de mas.
        sprintf(buf, "modgen: %ld", g_genMallaCount); StatLinea(buf, width, margen, ly, lineH);
    }
    if (OverlayStatTimes){
        // PROFILER: ms por categoria del frame (para saber a que atacar). scene=modelo+skinning; 3d=overhead del
        // viewport (grilla/overlays/huesos); ui=paneles (outliner/props/timeline); log=input+anim; swap=espera vsync.
        sprintf(buf, "ms scn:%.1f 3d:%.1f", g_profShow.scene, g_profShow.viewport3d - g_profShow.scene);                StatLinea(buf, width, margen, ly, lineH);
        sprintf(buf, "ms ui:%.1f log:%.1f", g_profShow.render - g_profShow.viewport3d, g_profShow.logic);               StatLinea(buf, width, margen, ly, lineH);
        sprintf(buf, "ms swap:%.1f tot:%.1f", g_profShow.swap, g_profShow.logic + g_profShow.render + g_profShow.swap); StatLinea(buf, width, margen, ly, lineH);
    }
    if (OverlayFps){
        sprintf(buf, "fps: %d", (int)(g_fpsActual + 0.5f)); StatLinea(buf, width, margen, ly, lineH);
    }
}
