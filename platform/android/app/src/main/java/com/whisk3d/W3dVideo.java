package com.whisk3d;

import android.media.Image;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.media.MediaMuxer;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;

// ================================================================================================================
//  EL VIDEO DEL RENDER EN ANDROID (tarjeta Render, formato MP4): H.264 con el codificador del TELEFONO (MediaCodec: el
//  fabricante ya pago sus patentes; Whisk3D no trae ningun codificador) y el sonido de la escena en AAC. El video y el
//  audio se graban aparte y al final se juntan en el MP4 (MediaExtractor + MediaMuxer). Lo llama el codigo nativo por
//  la actividad (Whisk3DActivity.video*): main/ui/ViewPorts/Properties.cpp, W3D_VIDEO_ANDROID
// ================================================================================================================
public class W3dVideo {
    private static final String TAG = "W3dVideo";
    private static final long ESPERA_US = 10000;   // (10 ms por vuelta al pedir buffers al codificador)

    private MediaCodec enc;
    private MediaMuxer mux;
    private int pista = -1;
    private boolean muxOn, ok;
    private long cuadros;
    private int w, h, fps;
    private String ruta, rutaVideo;
    private final MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();

    // el video de w x h (se redondea a par: H.264 en 4:2:0) a 'fps', con 'kbps' de bitrate. false si el telefono no
    // tiene codificador para ese tamano
    public boolean abrir(String ruta, int w, int h, int fps, int kbps) {
        liberar();
        try {
            this.ruta = ruta;
            this.w = (w + 1) & ~1; this.h = (h + 1) & ~1; this.fps = fps > 0 ? fps : 24;
            rutaVideo = ruta + ".video.tmp.mp4";
            MediaFormat f = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, this.w, this.h);
            f.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible);
            f.setInteger(MediaFormat.KEY_BIT_RATE, kbps * 1000);
            f.setInteger(MediaFormat.KEY_FRAME_RATE, this.fps);
            f.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1);
            enc = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC);
            enc.configure(f, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            enc.start();
            mux = new MediaMuxer(rutaVideo, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            cuadros = 0; pista = -1; muxOn = false; ok = true;
            return true;
        } catch (Exception e) {
            Log.e(TAG, "abrir: " + e);
            liberar();
            return false;
        }
    }

    // un cuadro: sw x sh pixeles RGBA de arriba hacia abajo
    public boolean cuadro(byte[] rgba, int sw, int sh) {
        if (!ok || enc == null) return false;
        try {
            int i = enc.dequeueInputBuffer(ESPERA_US);
            for (int vueltas = 0; i < 0 && vueltas < 1000; vueltas++) { drenar(false); i = enc.dequeueInputBuffer(ESPERA_US); }
            if (i < 0) { ok = false; return false; }
            Image img = enc.getInputImage(i);
            if (img == null) { ok = false; return false; }
            llenar(img, rgba, sw, sh);
            enc.queueInputBuffer(i, 0, w * h * 3 / 2, cuadros * 1000000L / fps, 0);
            cuadros++;
            drenar(false);
            return true;
        } catch (Exception e) {
            Log.e(TAG, "cuadro: " + e);
            ok = false;
            return false;
        }
    }

    // termina el video; con 'wav' (el audio de la escena ya renderizado) le agrega el sonido en AAC de 'kbpsAudio'. El
    // MP4 queda en 'ruta'
    public boolean cerrar(String wav, int kbpsAudio) {
        boolean bien = ok && enc != null;
        try {
            if (bien) {
                int i = enc.dequeueInputBuffer(ESPERA_US);
                for (int vueltas = 0; i < 0 && vueltas < 1000; vueltas++) { drenar(false); i = enc.dequeueInputBuffer(ESPERA_US); }
                if (i >= 0) enc.queueInputBuffer(i, 0, 0, cuadros * 1000000L / fps, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                drenar(true);
            }
        } catch (Exception e) {
            Log.e(TAG, "cerrar: " + e);
            bien = false;
        }
        liberar();
        bien = bien && muxOn;
        File tmpVideo = new File(rutaVideo);
        if (bien) {
            if (wav != null && wav.length() > 0) {
                String tmpAudio = ruta + ".audio.tmp.m4a";
                bien = aacDeWav(wav, tmpAudio, kbpsAudio) && juntar(rutaVideo, tmpAudio, ruta);
                new File(tmpAudio).delete();
            } else {
                File fin = new File(ruta);
                fin.delete();
                bien = tmpVideo.renameTo(fin);
            }
        }
        tmpVideo.delete();
        return bien;
    }

    // RGBA -> YUV 4:2:0 (BT.601, rango limitado) en los planos que pide el codificador (cualquier distribucion: la del
    // Image, con sus pasos de fila y de pixel). Lo que sobra al redondear a par repite el borde
    private void llenar(Image img, byte[] rgba, int sw, int sh) {
        Image.Plane[] p = img.getPlanes();
        ByteBuffer yb = p[0].getBuffer(), ub = p[1].getBuffer(), vb = p[2].getBuffer();
        int yRow = p[0].getRowStride(), yPix = p[0].getPixelStride();
        int uRow = p[1].getRowStride(), uPix = p[1].getPixelStride();
        int vRow = p[2].getRowStride(), vPix = p[2].getPixelStride();
        for (int y = 0; y < h; y++) {
            int sy = y < sh ? y : sh - 1;
            for (int x = 0; x < w; x++) {
                int sx = x < sw ? x : sw - 1;
                int o = (sy * sw + sx) * 4;
                int r = rgba[o] & 255, g = rgba[o + 1] & 255, b = rgba[o + 2] & 255;
                yb.put(y * yRow + x * yPix, (byte) (((66 * r + 129 * g + 25 * b + 128) >> 8) + 16));
                if ((x & 1) == 0 && (y & 1) == 0) {
                    int u = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
                    int v = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
                    ub.put((y / 2) * uRow + (x / 2) * uPix, (byte) u);
                    vb.put((y / 2) * vRow + (x / 2) * vPix, (byte) v);
                }
            }
        }
    }

    // lo que el codificador ya codifico, al MP4. 'fin' = hasta el final del stream
    private void drenar(boolean fin) {
        for (int vueltas = 0; vueltas < 100000; vueltas++) {
            int o = enc.dequeueOutputBuffer(info, fin ? ESPERA_US : 0);
            if (o == MediaCodec.INFO_TRY_AGAIN_LATER) { if (!fin) return; continue; }
            if (o == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                pista = mux.addTrack(enc.getOutputFormat());
                mux.start(); muxOn = true;
                continue;
            }
            if (o < 0) continue;
            ByteBuffer b = enc.getOutputBuffer(o);
            if ((info.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) info.size = 0;
            if (info.size > 0 && muxOn && b != null) {
                b.position(info.offset); b.limit(info.offset + info.size);
                mux.writeSampleData(pista, b, info);
            }
            enc.releaseOutputBuffer(o, false);
            if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) return;
        }
    }

    private void liberar() {
        try { if (enc != null) { enc.stop(); } } catch (Exception e) { }
        try { if (enc != null) { enc.release(); } } catch (Exception e) { }
        enc = null;
        try { if (mux != null && muxOn) mux.stop(); } catch (Exception e) { }
        try { if (mux != null) mux.release(); } catch (Exception e) { }
        mux = null;
        ok = false;
    }

    // ---- EL AUDIO: un WAV (PCM de 16 bits) a AAC en un .m4a, con el codificador del telefono. Tambien es el
    //      "Renderizar audio" en AAC de Android ----
    public static boolean aacDeWav(String wav, String salida, int kbps) {
        MediaCodec ac = null; MediaMuxer am = null; boolean muxOk = false;
        FileInputStream in = null;
        try {
            in = new FileInputStream(wav);
            byte[] cab = new byte[44];
            if (in.read(cab) != 44) return false;
            int canales = (cab[22] & 255) | ((cab[23] & 255) << 8);
            int rate = (cab[24] & 255) | ((cab[25] & 255) << 8) | ((cab[26] & 255) << 16) | ((cab[27] & 255) << 24);
            if (canales < 1 || rate <= 0) return false;
            MediaFormat f = MediaFormat.createAudioFormat(MediaFormat.MIMETYPE_AUDIO_AAC, rate, canales);
            f.setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectLC);
            f.setInteger(MediaFormat.KEY_BIT_RATE, kbps * 1000);
            f.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, 16384);
            ac = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_AUDIO_AAC);
            ac.configure(f, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            ac.start();
            am = new MediaMuxer(salida, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            MediaCodec.BufferInfo bi = new MediaCodec.BufferInfo();
            int pistaA = -1;
            long muestras = 0;
            final int bytesPorMuestra = 2 * canales;
            byte[] tramo = new byte[8192];
            boolean entradaFin = false, salidaFin = false;
            while (!salidaFin) {
                if (!entradaFin) {
                    int i = ac.dequeueInputBuffer(ESPERA_US);
                    if (i >= 0) {
                        ByteBuffer b = ac.getInputBuffer(i);
                        b.clear();
                        int quiero = Math.min(tramo.length, b.capacity());
                        quiero -= quiero % bytesPorMuestra;
                        int n = in.read(tramo, 0, quiero);
                        long pts = muestras * 1000000L / rate;
                        if (n <= 0) {
                            ac.queueInputBuffer(i, 0, 0, pts, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                            entradaFin = true;
                        } else {
                            b.put(tramo, 0, n);
                            ac.queueInputBuffer(i, 0, n, pts, 0);
                            muestras += n / bytesPorMuestra;
                        }
                    }
                }
                int o = ac.dequeueOutputBuffer(bi, ESPERA_US);
                if (o == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) { pistaA = am.addTrack(ac.getOutputFormat()); am.start(); muxOk = true; }
                else if (o >= 0) {
                    ByteBuffer b = ac.getOutputBuffer(o);
                    if ((bi.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) bi.size = 0;
                    if (bi.size > 0 && muxOk && b != null) { b.position(bi.offset); b.limit(bi.offset + bi.size); am.writeSampleData(pistaA, b, bi); }
                    ac.releaseOutputBuffer(o, false);
                    if ((bi.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) salidaFin = true;
                }
            }
            return muxOk;
        } catch (Exception e) {
            Log.e(TAG, "aacDeWav: " + e);
            return false;
        } finally {
            try { if (in != null) in.close(); } catch (IOException e) { }
            try { if (ac != null) { ac.stop(); ac.release(); } } catch (Exception e) { }
            try { if (am != null) { if (muxOk) am.stop(); am.release(); } } catch (Exception e) { }
        }
    }

    // junta el video y el audio (cada uno con una pista) en un MP4
    private static boolean juntar(String video, String audio, String salida) {
        MediaMuxer m = null; MediaExtractor ev = null, ea = null; boolean on = false;
        try {
            new File(salida).delete();
            m = new MediaMuxer(salida, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            ev = new MediaExtractor(); ev.setDataSource(video); ev.selectTrack(0);
            ea = new MediaExtractor(); ea.setDataSource(audio); ea.selectTrack(0);
            int tv = m.addTrack(ev.getTrackFormat(0)), ta = m.addTrack(ea.getTrackFormat(0));
            m.start(); on = true;
            // INTERCALADAS por tiempo (el MP4 las quiere asi): siempre la muestra mas temprana de las dos pistas
            ByteBuffer b = ByteBuffer.allocate(4 * 1024 * 1024);
            MediaCodec.BufferInfo bi = new MediaCodec.BufferInfo();
            boolean hayV = true, hayA = true;
            while (hayV || hayA) {
                long t_v = hayV ? ev.getSampleTime() : Long.MAX_VALUE, t_a = hayA ? ea.getSampleTime() : Long.MAX_VALUE;
                if (t_v < 0) { hayV = false; continue; }
                if (t_a < 0) { hayA = false; continue; }
                boolean deVideo = t_v <= t_a;
                MediaExtractor ex = deVideo ? ev : ea;
                int n = ex.readSampleData(b, 0);
                if (n < 0) { if (deVideo) hayV = false; else hayA = false; continue; }
                bi.offset = 0; bi.size = n; bi.presentationTimeUs = ex.getSampleTime();
                bi.flags = (ex.getSampleFlags() & MediaExtractor.SAMPLE_FLAG_SYNC) != 0 ? MediaCodec.BUFFER_FLAG_KEY_FRAME : 0;
                m.writeSampleData(deVideo ? tv : ta, b, bi);
                if (!ex.advance()) { if (deVideo) hayV = false; else hayA = false; }
            }
            return true;
        } catch (Exception e) {
            Log.e(TAG, "juntar: " + e);
            return false;
        } finally {
            try { if (m != null) { if (on) m.stop(); m.release(); } } catch (Exception e) { }
            if (ev != null) ev.release();
            if (ea != null) ea.release();
        }
    }
}
