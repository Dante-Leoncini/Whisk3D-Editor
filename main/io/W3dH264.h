#ifndef W3D_H264_H
#define W3D_H264_H
// ============================================================================
//  W3dH264 - UN CODIFICADOR H.264 PROPIO (Baseline, CAVLC), sin librerias: el video a CUALQUIER resolucion donde el
//  codificador de la maquina no llega (el N95: sus codificadores son MPEG-4 / H.263 hasta 720x576; con este, 720p,
//  1080p o 4K -lento, pero sale-). C++03, solo enteros (la transformada de la norma es exacta).
//
//  Lo justo de la norma (ISO/IEC 14496-10) para que cualquier reproductor lo entienda:
//    - cuadros clave (IDR) con macrobloques Intra 16x16 (prediccion vertical, horizontal o DC) y el croma en DC;
//    - el resto, cuadros P de 16x16 con busqueda de movimiento de pixel entero (y P_Skip donde no cambia nada);
//    - un slice por cuadro, sin filtro de bloques (deblocking apagado), QP por cuadro segun el bitrate pedido.
//  Lo que sale son NAL en "Annex B" (00 00 00 01 + NAL): van directo a W3dMp4Escritor::Cuadro.
//  Lo que el decodificador reconstruye es BIT A BIT lo que el codificador reconstruye (Reconstruido()): asi se prueba.
// ============================================================================
#include <vector>

class W3dH264Codificador {
public:
    W3dH264Codificador();
    // ancho y alto (pares; se rellena hasta multiplos de 16 y se recorta en el SPS), cuadros por segundo, bitrate
    // pedido (kbps) y cada cuantos cuadros va uno clave (IDR)
    bool Abrir(int w, int h, int fps, int kbps, int gop);
    // UN CUADRO RGBA (de arriba hacia abajo, 'w' x 'h' del Abrir): lo deja codificado en 'salida' (Annex B; el
    // primero lleva SPS y PPS adelante)
    bool Cuadro(const unsigned char* rgba, std::vector<unsigned char>& salida);
    // el ultimo cuadro reconstruido (YUV 4:2:0 planar del tamanio relleno: Y de anchoMb*16 x altoMb*16, U y V a la
    // mitad), para comparar con lo que decodifica un reproductor
    const std::vector<unsigned char>& Reconstruido() const { return ref; }   // (despues de Cuadro, la referencia)
    int AnchoRelleno() const { return mbW * 16; }
    int AltoRelleno() const { return mbH * 16; }
    int Qp() const { return qp; }

private:
    int w, h, wSrc, fps, kbps, gop, mbW, mbH, qp, nCuadro, idrId;
    long bytesMeta, bytesHechos;
    std::vector<unsigned char> cur, rec, ref;           // YUV 4:2:0 planar (relleno): el cuadro, su reconstruccion, la anterior
    std::vector<signed char> nzL, nzU, nzV;              // los coeficientes no nulos de cada bloque 4x4 (para el nC)
    std::vector<short> mvx, mvy;                         // el movimiento de cada macrobloque (en cuartos de pixel)
    std::vector<unsigned char> esSalto;
    bool Cerrar();
    void ConvertirRgba(const unsigned char* rgba);
    void CodificarCuadro(bool idr, std::vector<unsigned char>& nal);
};

#endif
