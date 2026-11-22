#ifndef W3D_MP4_H
#define W3D_MP4_H
// ============================================================================
//  W3dMp4 - EL CONTENEDOR MP4 (ISO BMFF) de un video ya codificado, sin librerias. Lo usan los codificadores que
//  entregan los cuadros comprimidos sueltos (el del N95 por DevVideoRecord; el del navegador por WebCodecs): ellos
//  codifican, esto los ordena en un .mp4 que reproduce cualquiera (RealPlayer del telefono, el navegador, la PC).
//
//  Una pista de video:
//    - H.264 (avc1 + avcC): cada cuadro como lo da el codificador, en "Annex B" (00 00 01 / 00 00 00 01 antes de
//      cada NAL). El SPS y el PPS se toman del primero que los traiga y van al avcC (no a los cuadros).
//    - MPEG-4 parte 2 (mp4v + esds): cada cuadro un VOP (00 00 01 B6); lo que viene antes del primero (VOS / VO / VOL)
//      es la configuracion del decodificador (el esds).
//  El INDICE (moov) va al PRINCIPIO del archivo: se reproduce apenas empieza a leerse. Todo se arma en memoria y se
//  escribe de una al cerrar (un video del telefono, 240p a unos cientos de kbps, son pocos MB).
//  C++03 (Symbian).
// ============================================================================
#include <string>
#include <vector>

enum { W3D_MP4_H264 = 0, W3D_MP4_MPEG4 = 1, W3D_MP4_ONLY_AUDIO = 2 };   // (SOLO_AUDIO: un .m4a, sin video)

class W3dMp4Writer {
public:
    W3dMp4Writer();
    // 'fps' entero (los del render); el ancho y el alto, los del video
    bool Open(const std::string& path, int codec, int w, int h, int fps);
    // UN CUADRO comprimido (un access unit). false = no se entendio (sin el SPS/PPS o el VOL todavia, datos vacios)
    bool Frame(const unsigned char* data, size_t length);
    // la configuracion que el codificador da APARTE (algunos la entregan antes del primer cuadro): SPS+PPS en Annex B,
    // o el VOS/VOL del MPEG-4
    void Config(const unsigned char* data, size_t length);
    // arma el archivo y lo escribe. 'escribir' NULL = stdio; Symbian pasa el suyo (RFile)
    bool Close(bool (*write)(const char* path, const unsigned char* data, size_t length) = 0);
    // EL AUDIO (AAC): los cuadros crudos (sin cabecera ADTS), su configuracion (AudioSpecificConfig) y cuanto mide cada
    // uno. Cada cuadro AAC son 1024 muestras. Se llama antes de Cerrar; sin esto el video va sin sonido. 'retardo' y
    // 'muestras': lo que el reproductor saltea al principio (el del codificador) y cuanto dura de verdad (lista de
    // edicion); 0 = todo
    void AudioAac(const std::vector<unsigned char>& asc, const std::vector<unsigned char>& data,
                  const std::vector<unsigned int>& sizes, int frequency, int channels, int kbps,
                  int delay = 0, long samples = 0);
    int Frames() const { return (int)size.size(); }
    bool IsOpen() const { return open; }
    std::string error;

private:
    bool open;
    std::string path;
    int codec, w, h, fps;
    std::vector<unsigned char> data;        // los cuadros, uno detras del otro (lo que va en el mdat)
    std::vector<unsigned int> size;         // el tamanio de cada uno
    std::vector<unsigned int> keys;         // los cuadros clave (desde 1)
    std::vector<unsigned char> sps, pps;    // H.264
    std::vector<unsigned char> dsi;         // MPEG-4: VOS / VO / VOL
    std::vector<unsigned char> aAsc, aData; // el AUDIO: la configuracion y los cuadros AAC
    std::vector<unsigned int> aSize;
    int aFreq, aCan, aKbps, aDelay;
    long aSamples;
    bool FrameH264(const unsigned char* d, size_t n);
    bool FrameMpeg4(const unsigned char* d, size_t n);
};

// LA PISTA AAC de un .mp4 / .m4a / .3gp ya escrito (el que deja el conversor de audio del telefono, o ffmpeg): la
// configuracion, los cuadros y sus tamanios, la frecuencia y los canales. false (y el motivo) si no tiene una
bool W3dMp4ReadAac(const std::vector<unsigned char>& file, std::vector<unsigned char>& asc, std::vector<unsigned char>& data,
                   std::vector<unsigned int>& sizes, int& frequency, int& channels, std::string* error);
// lo mismo de un .aac suelto (ADTS: cada cuadro con su cabecera de 7 o 9 bytes)
bool W3dAdtsReadAac(const std::vector<unsigned char>& file, std::vector<unsigned char>& asc, std::vector<unsigned char>& data,
                    std::vector<unsigned int>& sizes, int& frequency, int& channels, std::string* error);

#endif
