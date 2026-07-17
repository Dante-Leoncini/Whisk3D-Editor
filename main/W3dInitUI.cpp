// ============================================================================
//  Init de la UI COMPARTIDO (los 4 OS). Ver W3dInitUI.h.
// ============================================================================
#include "W3dInitUI.h"
#include "W3dLang.h"                       // idioma del sistema (los textos salen traducidos desde el primer frame)

#include "objects/Textures.h"          // Textures (vector global)
#include "w3dTexture.h"                // w3dEngine::LoadTexture
#include "w3dGraphics.h"               // w3dEngine: estado de graficos
#include "WhiskUI/font.h"                   // Font, WhiskFont
#include "WhiskUI/icons.h"                  // CrearIconos, SetIconScale
#include "WhiskUI/card.h"                   // CalcCardUV
#include "WhiskUI/UI.h"                     // GlobalScale
#include "ViewPorts/WithBorder.h"      // CalcBorderUV
#include "ViewPorts/ScrollBar.h"       // CalcScrollUV

void W3dInitUI(const std::string& skinDir) {
    // El IDIOMA va primero: la UI que se arma abajo ya pide sus textos con T(), asi que tiene que estar resuelto
    // antes. Aca y no en cada plataforma: este init lo comparten las cinco, y el detector ya sabe preguntarle a
    // cada SO por su lado.
    W3dIdiomaDetectar();

    // mismo ORDEN que esperan el resto de los modulos (constructor.cpp de PC):
    // 0=font (atlas), 1=origen, 2=cursor3d, 3=relationshipLine, 4=lampara
    static const char* archivos[5] = {
        "font.png", "origen.png", "cursor3d.png", "relationshipLine.png", "lamp.png"
    };

    int atlasW = 128, atlasH = 128; // tamano del atlas (font.png); fallback 128
    for (int i = 0; i < 5; i++) {
        Texture* t = new Texture(archivos[i]);
        // skinDir ya trae el separador final (cada OS pone el suyo: '/' o '\')
        std::string path = skinDir + archivos[i];
        unsigned int id = 0;
        int w = 0, h = 0;
        if (w3dEngine::LoadTexture(path.c_str(), id, &w, &h)) {
            t->iID = id;
            if (i == 0) { atlasW = w; atlasH = h; } // del atlas salen los UV
        }
        Textures.push_back(t);
    }

    // UI armada sobre el atlas (font.png): UVs del 9-patch, scrollbar, tarjetas,
    // iconos y la fuente real
    CalcBorderUV(atlasW, atlasH);
    CalcScrollUV(atlasW, atlasH);
    CalcCardUV(atlasW, atlasH);
    CrearIconos(atlasW, atlasH);
    SetIconScale(GlobalScale);

    WhiskFont = new Font(atlasW, atlasH, Textures[0]->iID);
    WhiskFont->SetScale(GlobalScale);
}

void W3dInitGraphics() {
    // baseline del pipeline fijo, COMUN a los 4 OS. El render por-frame ajusta
    // depth/textura/luz; esto es solo el estado inicial.
    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::CullFace);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Lighting);
    w3dEngine::Enable(w3dEngine::Normalize);   // renormalizar tras escalar
    w3dEngine::SmoothShading(true);
    w3dEngine::FastPerspective();

    // matriz de textura en identidad (los UV ya vienen en [0,1])
    w3dEngine::MatrixMode(w3dEngine::TextureMatrix);
    w3dEngine::LoadIdentity();
    w3dEngine::MatrixMode(w3dEngine::ModelView);

    // arrays del pipeline fijo
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::EnableArray(w3dEngine::NormalArray);
}
