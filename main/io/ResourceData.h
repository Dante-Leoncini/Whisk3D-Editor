#ifndef RESOURCE_DATA_H
#define RESOURCE_DATA_H
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

struct W3dImageData {
    std::string format;   // "PNG", "JPEG", "GIF", "BMP", "TGA", "WEBP"... ("" = no se reconoce)
    int  width, height;
    int  channels;          // del ARCHIVO (1 gris, 2 gris+alfa, 3 RGB, 4 RGBA); 0 = no se sabe
    bool alpha;             // tiene canal alfa (o transparencia por tRNS)
    int  alphaType;         // -1 = no se sabe (sin decodificar), 0 = opaco (todo 255), 1 = binario, 2 = con transparencias
    W3dImageData() : width(0), height(0), channels(0), alpha(false), alphaType(-1) {}
};
// false = no es una imagen que se reconozca
bool W3dImageInfo(const unsigned char* b, size_t n, W3dImageData& out);

struct W3dSoundData {
    std::string format;   // "WAV", "OGG", "MP3" ("" = no se reconoce)
    int  frequency;       // muestras por segundo (0 = no se sabe)
    int  channels;
    int  bits;             // bits por muestra (0 = no aplica: comprimido)
    float duration;        // segundos (-1 = no se sabe)
    bool loop;             // trae un bucle ('smpl' de un WAV)
    W3dSoundData() : frequency(0), channels(0), bits(0), duration(-1.0f), loop(false) {}
};
bool W3dSoundInfo(const unsigned char* b, size_t n, W3dSoundData& out);

// "512 B", "12.3 KB", "4.7 MB"
std::string W3dBytesText(double bytes);
// "1:05.3" / "0.52 s"
std::string W3dDurationText(float seconds);

#endif
