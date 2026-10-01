#ifndef PROPIMAGEN_H
#define PROPIMAGEN_H

#include "WhiskUI/Propieties/PropertieBase.h"
#include "WhiskUI/draw/glesdraw.h"
#include "objects/Textures.h"
#include "w3dGraphics.h"

// ============================================================================
//  VISTA PREVIA DE UNA TEXTURA en una tarjeta de Properties (la del recurso activo
//  del outliner por recursos): la imagen a lo ancho de la tarjeta, con su proporcion,
//  en 'filas' renglones de alto. No se selecciona ni se edita. Con tex == NULL (o
//  oculto) no ocupa lugar. Header-only (no toca el MMP de Symbian).
// ============================================================================
class PropImagen : public PropertieBase {
    public:
        Texture* tex;   // la textura a mostrar (la pone el panel cada cuadro; NULL = nada)
        int filas;      // alto maximo, en renglones
        bool oculto;

        PropImagen(const std::string& Name) : PropertieBase(Name), tex(NULL), filas(8), oculto(false) {}

        bool Seleccionable() { return false; }
        // la textura sigue viva? (el panel la re-bindea cada cuadro, pero un Resize puede llegar en
        // el medio: el cierre de un proyecto libera las texturas antes del proximo bindeo)
        bool Viva() const {
            if (!tex) return false;
            for (size_t i = 0; i < Textures.size(); i++) if (Textures[i] == tex) return true;
            return false;
        }
        bool Visible() const { return !oculto && Viva() && tex->iID; }
        int AltoFila() const { return filas * (RenglonHeightGS + gapGS); }
        int Resize(int w) { width = w; return Visible() ? AltoFila() : 0; }

        void RenderPropertiBox(Card* propertiBox) {
            (void)propertiBox;
            if (Visible()) w3dEngine::Translatef(0, (GLfloat)AltoFila(), 0);
        }
        void RenderPropertiValue(Card* propertiBox) {
            (void)propertiBox;
            if (Visible()) w3dEngine::Translatef(0, (GLfloat)AltoFila(), 0);
        }
        void RenderPropertiLabel(Card* propertiBox) {
            (void)propertiBox;
            if (!Visible()) return;
            // el pase de labels esta parado en el borde derecho de la columna de etiquetas:
            // volver al margen izquierdo (como PropLabel) y usar el ancho entero de la tarjeta
            int w = width - gapGS * 4;
            int h = AltoFila() - gapGS;
            if (w < 1 || h < 1) { w3dEngine::Translatef(0, (GLfloat)AltoFila(), 0); return; }
            // PROPORCION de la imagen (la que entre, centrada a lo ancho)
            if (tex->ancho > 0 && tex->alto > 0) {
                const long wProp = (long)h * tex->ancho / tex->alto;
                if (wProp <= w) w = (int)wProp;
                else h = (int)((long)w * tex->alto / tex->ancho);
            }
            const int x0 = ((width - gapGS * 4) - w) / 2;
            GLshort v[8] = { 0, 0, (GLshort)w, 0, 0, (GLshort)h, (GLshort)w, (GLshort)h };
            GLfloat uv[8] = { 0, 0, 1, 0, 0, 1, 1, 1 };
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(-PropColEtiqueta + gapGS * 2 + x0), 0, 0);
            w3dEngine::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
            w3dEngine::BindTexture(tex->iID);
            W3dDrawStrip4(v, uv);
            w3dEngine::BindTexture(Textures[0]->iID);   // el atlas de la UI (iconos y letras)
            w3dEngine::PopMatrix();
            w3dEngine::Translatef(0, (GLfloat)AltoFila(), 0);
        }
};

#endif // PROPIMAGEN_H
