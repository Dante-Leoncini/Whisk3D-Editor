// ============================================================================
//  W3dWebVideo.cpp - el codificador H.264 del navegador (WebCodecs), ver el .h. Todo el JavaScript va aca (EM_JS): la
//  CSP del sitio no deja <script> en linea y esto no necesita nada del shell.
//  (OJO: el cuerpo de un EM_JS llega al JavaScript en UNA linea: cada sentencia con su ';')
// ============================================================================
#include "W3dWebVideo.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <vector>

// ---- la pregunta del arranque: un video chico en H.264 (cualquier perfil). 1 = si, 0 = no o no se sabe todavia ----
EM_JS(void, W3dWebVidTestJs, (), {
    if (globalThis.__w3dH264 !== undefined) return;
    globalThis.__w3dH264 = 0;
    if (typeof VideoEncoder === 'undefined' || typeof VideoFrame === 'undefined') return;
    var codecs = ['avc1.42001f', 'avc1.42e01f', 'avc1.4d001f', 'avc1.64001f'];
    var i = 0;
    function other() {
        if (i >= codecs.length) return;
        var c = codecs[i++];
        VideoEncoder.isConfigSupported({ codec: c, width: 640, height: 360, bitrate: 2000000, framerate: 30 })
            .then(function (r) { if (r && r.supported) globalThis.__w3dH264 = 1; else other(); }, other);
    }
    other();
});
EM_JS(int, W3dWebVidHasJs, (), { return globalThis.__w3dH264 === 1 ? 1 : 0; });

// ---- EL CODIFICADOR. Prueba de mejor a peor: High, Main, Baseline, con el NIVEL que pide el tamanio y los fps (Chrome
//      rechaza un nivel que no alcanza). Los cuadros salen en Annex B (SPS y PPS en cada cuadro clave); si el navegador
//      no sabe Annex B los da con largos de 4 bytes y la configuracion aparte (avcC), que aca se pasa a Annex B ----
EM_JS(void, W3dWebVidOpenJs, (int w, int h, int fps, int kbps), {
    var old = globalThis.__w3dVid;
    if (old && old.enc) { try { old.enc.close(); } catch (e) {} }
    var st = { state: 0, error: "", enc: null, chunks: [], sent: 0, fps: fps, w: w, h: h };
    globalThis.__w3dVid = st;
    function fault(m) { if (st.state !== -1) { st.state = -1; st.error = String(m); } }
    if (typeof VideoEncoder === 'undefined') { fault('WebCodecs is not available'); return; }
    var mbs = Math.ceil(w / 16) * Math.ceil(h / 16), bySeg = mbs * fps;
    // [nivel, macrobloques por cuadro, macrobloques por segundo] (H.264, tabla A-1)
    var levels = [[0x1E, 1620, 40500], [0x1F, 3600, 108000], [0x20, 5120, 216000], [0x28, 8192, 245760],
                   [0x2A, 8704, 522240], [0x32, 22080, 589824], [0x33, 36864, 983040], [0x34, 36864, 2073600],
                   [0x3C, 139264, 4177920], [0x3D, 139264, 8355840], [0x3E, 139264, 16711680]];
    var level = 0x3E;
    for (var k = 0; k < levels.length; k++) if (mbs <= levels[k][1] && bySeg <= levels[k][2]) { level = levels[k][0]; break; }
    var hx = (level < 16 ? "0" : "") + level.toString(16);
    var profiles = ['6400', '4d00', '4200', '42e0'];
    var i = 0;
    function chunk(c, meta) {
        // la configuracion aparte (avcC, cuando no hay Annex B): sus SPS y PPS con codigo de inicio
        if (meta && meta.decoderConfig && meta.decoderConfig.description && !st.withConfig) {
            st.withConfig = true;
            var d = meta.decoderConfig.description;
            var a = d instanceof ArrayBuffer ? new Uint8Array(d) : new Uint8Array(d.buffer, d.byteOffset, d.byteLength);
            var out = [], p = 5, n, l, j;
            if (a.length > 6) {
                n = a[p++] & 0x1F;
                for (j = 0; j < n && p + 2 <= a.length; j++) { l = (a[p] << 8) | a[p + 1]; p += 2; out.push(0, 0, 0, 1); for (var q = 0; q < l; q++) out.push(a[p + q]); p += l; }
                n = p < a.length ? a[p++] : 0;
                for (j = 0; j < n && p + 2 <= a.length; j++) { l = (a[p] << 8) | a[p + 1]; p += 2; out.push(0, 0, 0, 1); for (var q2 = 0; q2 < l; q2++) out.push(a[p + q2]); p += l; }
                if (out.length) st.chunks.push({ d: new Uint8Array(out), config: 1 });
            }
        }
        var b = new Uint8Array(c.byteLength);
        c.copyTo(b);
        st.chunks.push({ d: b, config: 0 });
    }
    function test() {
        if (st !== globalThis.__w3dVid) return;
        if (i >= profiles.length) { fault('This browser cannot encode H.264 at ' + w + 'x' + h + ' ' + fps + ' fps'); return; }
        var cfg = { codec: 'avc1.' + profiles[i++] + hx, width: w, height: h, bitrate: kbps * 1000, framerate: fps,
                    latencyMode: 'quality', avc: { format: 'annexb' } };
        VideoEncoder.isConfigSupported(cfg).then(function (r) {
            if (st !== globalThis.__w3dVid) return;
            if (!r || !r.supported) { test(); return; }
            try {
                st.enc = new VideoEncoder({ output: chunk, error: function (e) { fault(e && e.message ? e.message : e); } });
                st.enc.configure(cfg);
                st.codec = cfg.codec;
                st.state = 1;
                console.log('[video] WebCodecs: ' + cfg.codec + ' ' + w + 'x' + h + ' ' + fps + ' fps ' + kbps + ' kbps');
            } catch (e) { st.enc = null; test(); }
        }, function () { test(); });
    }
    test();
});
EM_JS(int, W3dWebVidStateJs, (), { var st = globalThis.__w3dVid; return st ? st.state : -1; });
EM_JS(int, W3dWebVidQueueJs, (), { var st = globalThis.__w3dVid; return (st && st.enc) ? st.enc.encodeQueueSize : 0; });
// el cuadro I420 del heap a un VideoFrame (lo copia: el buffer de C++ se puede reusar). BT.709 desde 720 lineas,
// BT.601 abajo (lo que supone cualquier reproductor de un video sin etiquetar; aca va etiquetado igual)
EM_JS(int, W3dWebVidFrameJs, (const unsigned char* yuv, int w, int h, int index, int key), {
    var st = globalThis.__w3dVid;
    if (!st || !st.enc || st.state !== 1) return 0;
    var hd = h >= 720, cs = hd ? 'bt709' : 'smpte170m';
    try {
        var fr = new VideoFrame(HEAPU8.subarray(yuv, yuv + w * h * 3 / 2), {
            format: 'I420', codedWidth: w, codedHeight: h,
            timestamp: Math.round(index * 1000000 / st.fps), duration: Math.round(1000000 / st.fps),
            colorSpace: { primaries: hd ? 'bt709' : 'smpte170m', transfer: cs, matrix: cs, fullRange: false } });
        st.enc.encode(fr, { keyFrame: key !== 0 });
        fr.close();
        st.sent++;
        return 1;
    } catch (e) { st.state = -1; st.error = String(e && e.message ? e.message : e); return 0; }
});
EM_JS(int, W3dWebVidRemoveJs, (unsigned char* dst, int cap, int* config), {
    var st = globalThis.__w3dVid;
    if (!st || !st.chunks.length) return 0;
    var t = st.chunks[0];
    if (t.d.length > cap) return -t.d.length;
    st.chunks.shift();
    HEAPU8.set(t.d, dst);
    HEAP32[config >> 2] = t.config;
    return t.d.length;
});
EM_JS(void, W3dWebVidFinishJs, (), {
    var st = globalThis.__w3dVid;
    if (!st || !st.enc || st.state !== 1) return;
    st.state = 2;
    st.enc.flush().then(function () { if (st.state === 2) st.state = 3; },
                        function (e) { st.state = -1; st.error = String(e && e.message ? e.message : e); });
});
EM_JS(void, W3dWebVidCloseJs, (), {
    var st = globalThis.__w3dVid;
    if (st && st.enc) { try { st.enc.close(); } catch (e) {} }
    globalThis.__w3dVid = null;
});
// el motivo de la falla (en ingles: va al log), copiado al buffer de C++
EM_JS(int, W3dWebVidErrorJs, (char* dst, int cap), {
    var st = globalThis.__w3dVid;
    var b = new TextEncoder().encode(st ? st.error : 'no encoder');
    var n = Math.min(b.length, cap - 1);
    HEAPU8.set(b.subarray(0, n), dst);
    HEAPU8[dst + n] = 0;
    return n;
});

void W3dWebVideoTest() { W3dWebVidTestJs(); }
bool W3dWebVideoHas() { return W3dWebVidHasJs() != 0; }
void W3dWebVideoOpen(int w, int h, int fps, int kbps) { W3dWebVidOpenJs(w, h, fps > 0 ? fps : 24, kbps > 0 ? kbps : 4000); }
int W3dWebVideoState() { return W3dWebVidStateJs(); }
std::string W3dWebVideoError() { char b[512]; W3dWebVidErrorJs(b, (int)sizeof(b)); return std::string(b); }
int W3dWebVideoQueue() { return W3dWebVidQueueJs(); }
bool W3dWebVideoFrame(const unsigned char* yuv, int w, int h, int index, bool key) {
    return yuv && W3dWebVidFrameJs(yuv, w, h, index, key ? 1 : 0) != 0;
}
int W3dWebVideoRemove(unsigned char* dst, int cap, bool& config) {
    int c = 0;
    const int n = W3dWebVidRemoveJs(dst, cap, &c);
    config = c != 0;
    return n;
}
void W3dWebVideoFinish() { W3dWebVidFinishJs(); }
void W3dWebVideoClose() { W3dWebVidCloseJs(); }
#endif
