#ifndef W3D_AAC_H
#define W3D_AAC_H
// ============================================================================
//  W3dAac - UN CODIFICADOR AAC-LC PROPIO, sin librerias: el sonido de los MP4 donde la maquina no trae uno que sirva
//  (el N95: su conversor de audio no pasa un WAV a AAC; solo graba del microfono). Funciona igual en todas las
//  plataformas (C++03, float).
//
//  Lo justo de la norma (ISO/IEC 14496-3) para que cualquier reproductor lo entienda: ventanas largas (seno), MDCT de
//  2048 por FFT, un solo scalefactor por canal y cuadro (el que entra en el bitrate pedido, buscado por biseccion),
//  el libro Huffman mas barato por banda, sin TNS ni estereo conjunto. Sin psicoacustica: el ruido de cuantizar queda
//  parejo en todo el espectro; a 96-192 kbps se oye bien.
//
//  Cada cuadro son 1024 muestras por canal. El primero sale con 1024 muestras de retardo (el solapamiento de la MDCT):
//  el MP4 lo saltea con su lista de edicion (W3dMp4Writer::AudioAac, 'retardo').
// ============================================================================
#include <string>
#include <vector>

class W3dAacEncoder {
public:
    W3dAacEncoder();
    // frecuencias de la norma (8000 .. 96000); 1 o 2 canales. false = no se puede
    bool Open(int frequency, int channels, int kbps);
    // muestras de 16 bits intercaladas (L R L R ...), 'cuadros' por canal
    void Samples(const short* pcm, int frames);
    // lo que quedo a medias, y el ultimo solapamiento
    void Finish();

    std::vector<unsigned char> asc;     // AudioSpecificConfig (2 bytes: va al esds del MP4)
    std::vector<unsigned char> data;    // los cuadros AAC crudos, uno detras del otro
    std::vector<unsigned int> sizes;    // cuanto mide cada uno
    long samples;                       // las muestras por canal que entraron (sin el relleno del final)
    int delay;                          // 1024: lo que el reproductor tiene que saltear al principio
    std::string error;

private:
    int freq, can, kbps, idxFreq, nBands, maxSfb;
    const unsigned short* swb;
    std::vector<float> previous[2], current[2];   // el cuadro anterior y el que se esta juntando
    int full;
    std::vector<float> window, cosR, sinR, cosT, sinT, cosF, sinF;   // seno; los giros de la MDCT y de la FFT
    // (lo de trabajar de cada cuadro: aca y no en la pila, que en el telefono es chica)
    std::vector<float> tz, tX, txp, tu, tre, tim;
    std::vector<int> tq[2], tqs;
    void Frame();
    void Mdct(const float* entry, float* output);
};

// un WAV entero (PCM de 16 bits, 1 o 2 canales) a AAC: la configuracion, los cuadros, sus tamanios, la frecuencia, los
// canales, las muestras y el retardo. false (y el motivo) si no se pudo
bool W3dAacFromWav(const std::vector<unsigned char>& wav, int kbps, W3dAacEncoder& cod, int& frequency, int& channels);

#endif
