#ifndef W3D_RECURSOS_DATOS_H
#define W3D_RECURSOS_DATOS_H
// ============================================================================
//  RecursosDatos — lo que la BIBLIOTECA dice de un ARCHIVO del proyecto sin
//  cargarlo en el motor: de una imagen (formato, ancho x alto, si tiene canal
//  alfa y si ese alfa es BINARIO -solo 0 y 255, un recorte- o con transparencias
//  intermedias) y de un sonido (formato, frecuencia, canales, bits, duracion y si
//  trae un loop 'smpl'). Lee los ENCABEZADOS (PNG IHDR/tRNS, JPEG SOF, WAV RIFF,
//  OGG Vorbis); el tipo de alfa necesita los pixeles: se decodifican con stb_image
//  donde lo hay (en el N95 se informa solo lo del encabezado).
//  Solo editor. C++03. Motor generico.
// ============================================================================
#include <string>
#include <vector>

struct W3dImagenDatos {
    std::string formato;   // "PNG", "JPEG", "GIF", "BMP", "TGA", "WEBP"... ("" = no se reconoce)
    int  ancho, alto;
    int  canales;          // del ARCHIVO (1 gris, 2 gris+alfa, 3 RGB, 4 RGBA); 0 = no se sabe
    bool alfa;             // tiene canal alfa (o transparencia por tRNS)
    int  alfaTipo;         // -1 = no se sabe (sin decodificar), 0 = opaco (todo 255), 1 = binario, 2 = con transparencias
    W3dImagenDatos() : ancho(0), alto(0), canales(0), alfa(false), alfaTipo(-1) {}
};
// false = no es una imagen que se reconozca
bool W3dImagenInfo(const unsigned char* b, size_t n, W3dImagenDatos& out);

struct W3dSonidoDatos {
    std::string formato;   // "WAV", "OGG", "MP3" ("" = no se reconoce)
    int  frecuencia;       // muestras por segundo (0 = no se sabe)
    int  canales;
    int  bits;             // bits por muestra (0 = no aplica: comprimido)
    float duracion;        // segundos (-1 = no se sabe)
    bool loop;             // trae un bucle ('smpl' de un WAV)
    W3dSonidoDatos() : frecuencia(0), canales(0), bits(0), duracion(-1.0f), loop(false) {}
};
bool W3dSonidoInfo(const unsigned char* b, size_t n, W3dSonidoDatos& out);

// "512 B", "12.3 KB", "4.7 MB"
std::string W3dBytesTexto(double bytes);
// "1:05.3" / "0.52 s"
std::string W3dDuracionTexto(float segundos);

#endif
