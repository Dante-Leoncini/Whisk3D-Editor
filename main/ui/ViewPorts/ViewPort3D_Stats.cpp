// ===================================================================================================
//  OVERLAY de estadisticas del Viewport3D (arriba a la derecha): vertices, caras, modgen, ms, fps.
//  Extraido de ViewPort3D.cpp (Fase 2 del reorg). El metodo ya esta declarado en Viewport3D (ViewPort3D.h).
//  Solo LEE globales ya expuestos (g_profShow del profiler, g_genMeshCount/g_currentFps de OpcionesRender).
// ===================================================================================================
#include "w3dGraphics.h"                 // w3dEngine::Color4f/PushMatrix/Translatef/PopMatrix
#include "ViewPorts/ViewPort3D.h"        // Viewport3D + members; arrastra variables.h (SceneCollection)
#include "WhiskUI/text/bitmapText.h"     // RenderBitmapText + textAlign
#include "WhiskUI/theme/colors.h"        // ColorList / ColorID
#include "WhiskUI/core/UI.h"             // gapGS, LetterHeightGS
#include "W3dProfile.h"                  // g_profShow (ms por categoria del frame)
#include "render/RenderOptions.h"        // g_genMeshCount, g_currentFps
#include "render/RayTracer.h"            // g_rt / RTImagesPerSecond (las imagenes trazadas por segundo)
#include "objects/Mesh.h"                // Mesh (contar vertices/caras de la escena)
#include "objects/Routine.h"             // W3dRoutineCount (lo que dibujan las rutinas)
#include <cstdio>                        // sprintf
#include "W3dLang.h"                      // T(): las lineas de vertices / caras en el idioma del sistema

// suma recursiva de las estadisticas de malla de toda la escena (la usa tambien el comando de prueba "carastotal")
void W3dCountMeshes(Object* o, int& vAdd, int& vReal, int& fLog, int& fTri){
    if (!o) return;
    if (o->getType() == ObjectType::mesh){
        Mesh* m = (Mesh*)o;
        vAdd  += (m->groupedVerts > 0) ? m->groupedVerts : m->vertexSize;
        vReal += m->vertexSize;
        // caras LOGICAS: las de la edicion; si esta pendiente (la malla de un recurso compartido,
        // o una .w3db leida como el juego), las que dice el sello del archivo (sin materializar)
        fLog  += !m->faces3d.empty() ? (int)m->faces3d.size()
               : (!m->pendingEdit.empty() && m->w3dmForeign.nFaces > 0) ? m->w3dmForeign.nFaces
               : (m->facesSize / 3);
        fTri  += m->facesSize / 3;
    } else if (o->getType() == ObjectType::routine){
        // lo que dibujan las rutinas (un escenario unido, instancias): sin esto "dibujadas" pasaba al total
        int v = 0, t = 0;
        W3dRoutineCount((Routine*)o, v, t);
        vAdd += v; vReal += v; fLog += t; fTri += t;
    }
    for (size_t i = 0; i < o->Childrens.size(); i++)
        W3dCountMeshes(o->Childrens[i], vAdd, vReal, fLog, fTri);
}

// dibuja una linea del overlay alineada a la derecha, en la fila actual, y avanza a la siguiente
static void StatLine(const char* buf, int width, int margin, int& ly, int lineH){
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)(width - margin), (GLfloat)ly, 0);
    RenderBitmapText(buf, textAlign::right, width);
    w3dEngine::PopMatrix();
    ly += lineH;
}

// texto blanco arriba a la derecha: vertices agrupados/reales, caras logicas/
// triangulos y fps. Se llama dentro del pase 2D de RenderUI (ortho + fuente ya
// seteados). Los contadores por malla estan precalculados (no se cuenta por frame).
void Viewport3D::RenderStats(){
    // Las stats (faces/gl/lua/times/fps) son DEBUG y se controlan por SUS PROPIOS flags (View menu / --stats),
    // NO por "Show Overlays" (que es para los gizmos del editor: grilla, ejes...). Sin esto no se veian jugando,
    // porque el pase de camara del juego apaga showOverlays (ViewPort3D.cpp) y arrastraba a las stats con el.
    if (!OverlayStatVertices && !OverlayStatFaces && !OverlayStatModgen && !OverlayStatTimes && !OverlayFps && !OverlayStatGL) return;
    SetColorID(ColorID::white);
    const int margin = gapGS * 2;
    const int lineH = LetterHeightGS + gapGS;
    int ly = (barDown ? 0 : BarHeight()) + gapGS; // debajo de la barra si esta arriba
    char buf[64];
    if (OverlayStatVertices || OverlayStatFaces){
        int vAdd=0, vReal=0, fLog=0, fTri=0;
        W3dCountMeshes(SceneCollection, vAdd, vReal, fLog, fTri);
        if (OverlayStatVertices){ snprintf(buf, sizeof buf, T("vertex: %d/%d"), vAdd, vReal); StatLine(buf, width, margin, ly, lineH); }
        // "faces: DIBUJADAS/total": lo que el pase de escena de ESTE frame emitio de
        // verdad (indices/3 de los contadores del Core, ya con frustum + LOD + Culling
        // + celdas por triangulo aplicados; los mide Render(), misma fuente que el
        // bench) contra el total de triangulos de la escena. Pedido del dueno:
        // "cuantos poligonos con el culling se estan dibujando y cuantas hay en
        // total. asi sabemos cuanto ahorramos". Antes mostraba logicas/triangulos
        // (total/total): no reflejaba el culling.
        if (OverlayStatFaces){    snprintf(buf, sizeof buf, T("faces: %d/%d"), statTrisFrame, fTri); StatLine(buf, width, margin, ly, lineH); }
    }
    if (OverlayStatGL){
        // TODAS las llamadas al driver del cuadro anterior (cada gl* = 1; los ahorros del cache de estado no llegan):
        // la escena (con sus particulas), la interfaz del JUEGO y la del editor, y de la escena los draws, binds y
        // cambios de estado. Antes era solo la escena y sin clear/viewport/color/niebla/lineas: no contaba la UI
        const w3dEngine::FrameStats& c = w3dEngine::g_statFrame;
        sprintf(buf, "gl: %d", c.total); StatLine(buf, width, margin, ly, lineH);
        sprintf(buf, " esc %d ui %d ed %d", c.calls[w3dEngine::StatCatScene] + c.calls[w3dEngine::StatCatParticles],
                c.calls[w3dEngine::StatCatUI], c.calls[w3dEngine::StatCatEditor]);
        StatLine(buf, width, margin, ly, lineH);
        sprintf(buf, " escena: dw %d tx %d st %d", statDrawsFrame, statBindsFrame, statStatesFrame);
        StatLine(buf, width, margin, ly, lineH);
    }
    if (OverlayStatModgen){
        // DIAGNOSTICO de performance: regeneraciones de la malla de modificadores. Al ROTAR la camara NO debe subir
        // (la subdivision/screw se cachea en genValid). Si sube al rotar -> se esta recalculando de mas.
        sprintf(buf, "modgen: %ld", g_genMeshCount); StatLine(buf, width, margin, ly, lineH);
    }
    if (OverlayStatTimes){
        // PROFILER: ms por categoria del frame (para saber a que atacar). scene=modelo+skinning; 3d=overhead del
        // viewport (grilla/overlays/huesos); ui=paneles (outliner/props/timeline); log=input+anim; swap=espera vsync.
        // Formato en DECIMAS sin %f (el printf de Symbian no formatea floats; misma regla que W3dFmtF del 3D).
        long ms[6];
        { double vs[6] = { g_profShow.scene, g_profShow.viewport3d - g_profShow.scene,
                           g_profShow.render - g_profShow.viewport3d, g_profShow.logic,
                           g_profShow.swap, g_profShow.logic + g_profShow.render + g_profShow.swap };
          for (int i = 0; i < 6; i++) { double v = vs[i]; if (v < 0) v = 0; ms[i] = (long)(v * 10.0 + 0.5); } }
        sprintf(buf, "ms scn:%ld.%ld 3d:%ld.%ld",  ms[0]/10, ms[0]%10, ms[1]/10, ms[1]%10); StatLine(buf, width, margin, ly, lineH);
        sprintf(buf, "ms ui:%ld.%ld log:%ld.%ld",  ms[2]/10, ms[2]%10, ms[3]/10, ms[3]%10); StatLine(buf, width, margin, ly, lineH);
        sprintf(buf, "ms swap:%ld.%ld tot:%ld.%ld", ms[4]/10, ms[4]%10, ms[5]/10, ms[5]%10); StatLine(buf, width, margin, ly, lineH);
        // SCRIPTS lua: ms del loop + activos/total. "activos" = los que corrieron (visibles/escena activa); los
        // que estan fuera de camara NO cuentan (el "solo calcular lo cercano"). caras/draws ya salen arriba (faces:/gl:).
        { extern int g_luaActiveScripts, g_luaScriptsTotal; extern double g_luaTickMs;
          long lm = (long)(g_luaTickMs * 10.0 + 0.5); if (lm < 0) lm = 0;
          sprintf(buf, "lua:%ld.%ld ms scr:%d/%d", lm/10, lm%10, g_luaActiveScripts, g_luaScriptsTotal);
          StatLine(buf, width, margin, ly, lineH); }
    }
    if (OverlayFps){
        sprintf(buf, "fps: %d", (int)(g_currentFps + 0.5f)); StatLine(buf, width, margin, ly, lineH);
        // con el trazado: las imagenes trazadas terminadas por segundo (el editor puede dibujar mas cuadros que eso)
        if (view == RenderType::Rendered && g_rt.on) {
            sprintf(buf, "rt: %.1f fps", RTImagesPerSecond()); StatLine(buf, width, margin, ly, lineH);
            const double rps = RTRaysPerSecond();
            if (rps > 0.0) { sprintf(buf, "rt: %.0f Mrayos/s", rps / 1e6); StatLine(buf, width, margin, ly, lineH); }
        }
    }
}
