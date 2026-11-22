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
    private static final long WAIT_US = 10000;   // (10 ms por vuelta al pedir buffers al codificador)

    private MediaCodec enc;
    private MediaMuxer mux;
    private int track = -1;
    private boolean muxOn, ok;
    private long frames;
    private int w, h, fps;
    private String path, videoPath;
    private final MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();

    // el video de w x h (se redondea a par: H.264 en 4:2:0) a 'fps', con 'kbps' de bitrate. false si el telefono no
    // tiene codificador para ese tamano
    public boolean open(String path, int w, int h, int fps, int kbps) {
        free();
        try {
            this.path = path;
            this.w = (w + 1) & ~1; this.h = (h + 1) & ~1; this.fps = fps > 0 ? fps : 24;
            videoPath = path + ".video.tmp.mp4";
            MediaFormat f = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, this.w, this.h);
            f.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible);
            f.setInteger(MediaFormat.KEY_BIT_RATE, kbps * 1000);
            f.setInteger(MediaFormat.KEY_FRAME_RATE, this.fps);
            f.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1);
            enc = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC);
            enc.configure(f, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            enc.start();
            mux = new MediaMuxer(videoPath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            frames = 0; track = -1; muxOn = false; ok = true;
            return true;
        } catch (Exception e) {
            Log.e(TAG, "abrir: " + e);
            free();
            return false;
        }
    }

    // un cuadro: sw x sh pixeles RGBA de arriba hacia abajo
    public boolean frame(byte[] rgba, int sw, int sh) {
        if (!ok || enc == null) return false;
        try {
            int i = enc.dequeueInputBuffer(WAIT_US);
            for (int turns = 0; i < 0 && turns < 1000; turns++) { drain(false); i = enc.dequeueInputBuffer(WAIT_US); }
            if (i < 0) { ok = false; return false; }
            Image img = enc.getInputImage(i);
            if (img == null) { ok = false; return false; }
            fill(img, rgba, sw, sh);
            enc.queueInputBuffer(i, 0, w * h * 3 / 2, frames * 1000000L / fps, 0);
            frames++;
            drain(false);
            return true;
        } catch (Exception e) {
            Log.e(TAG, "cuadro: " + e);
            ok = false;
            return false;
        }
    }

    // termina el video; con 'wav' (el audio de la escena ya renderizado) le agrega el sonido en AAC de 'kbpsAudio'. El
    // MP4 queda en 'ruta'
    public boolean close(String wav, int kbpsAudio) {
        boolean good = ok && enc != null;
        try {
            if (good) {
                int i = enc.dequeueInputBuffer(WAIT_US);
                for (int turns = 0; i < 0 && turns < 1000; turns++) { drain(false); i = enc.dequeueInputBuffer(WAIT_US); }
                if (i >= 0) enc.queueInputBuffer(i, 0, 0, frames * 1000000L / fps, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                drain(true);
            }
        } catch (Exception e) {
            Log.e(TAG, "cerrar: " + e);
            good = false;
        }
        free();
        good = good && muxOn;
        File tmpVideo = new File(videoPath);
        if (good) {
            if (wav != null && wav.length() > 0) {
                String tmpAudio = path + ".audio.tmp.m4a";
                good = aacOfWav(wav, tmpAudio, kbpsAudio) && collect(videoPath, tmpAudio, path);
                new File(tmpAudio).delete();
            } else {
                File end = new File(path);
                end.delete();
                good = tmpVideo.renameTo(end);
            }
        }
        tmpVideo.delete();
        return good;
    }

    // RGBA -> YUV 4:2:0 (BT.601, rango limitado) en los planos que pide el codificador (cualquier distribucion: la del
    // Image, con sus pasos de fila y de pixel). Lo que sobra al redondear a par repite el borde
    private void fill(Image img, byte[] rgba, int sw, int sh) {
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
    private void drain(boolean end) {
        for (int turns = 0; turns < 100000; turns++) {
            int o = enc.dequeueOutputBuffer(info, end ? WAIT_US : 0);
            if (o == MediaCodec.INFO_TRY_AGAIN_LATER) { if (!end) return; continue; }
            if (o == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                track = mux.addTrack(enc.getOutputFormat());
                mux.start(); muxOn = true;
                continue;
            }
            if (o < 0) continue;
            ByteBuffer b = enc.getOutputBuffer(o);
            if ((info.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) info.size = 0;
            if (info.size > 0 && muxOn && b != null) {
                b.position(info.offset); b.limit(info.offset + info.size);
                mux.writeSampleData(track, b, info);
            }
            enc.releaseOutputBuffer(o, false);
            if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) return;
        }
    }

    private void free() {
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
    public static boolean aacOfWav(String wav, String output, int kbps) {
        MediaCodec ac = null; MediaMuxer am = null; boolean muxOk = false;
        FileInputStream in = null;
        try {
            in = new FileInputStream(wav);
            byte[] cab = new byte[44];
            if (in.read(cab) != 44) return false;
            int channels = (cab[22] & 255) | ((cab[23] & 255) << 8);
            int rate = (cab[24] & 255) | ((cab[25] & 255) << 8) | ((cab[26] & 255) << 16) | ((cab[27] & 255) << 24);
            if (channels < 1 || rate <= 0) return false;
            MediaFormat f = MediaFormat.createAudioFormat(MediaFormat.MIMETYPE_AUDIO_AAC, rate, channels);
            f.setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectLC);
            f.setInteger(MediaFormat.KEY_BIT_RATE, kbps * 1000);
            f.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, 16384);
            ac = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_AUDIO_AAC);
            ac.configure(f, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            ac.start();
            am = new MediaMuxer(output, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            MediaCodec.BufferInfo bi = new MediaCodec.BufferInfo();
            int trackA = -1;
            long samples = 0;
            final int bytesBySample = 2 * channels;
            byte[] segment = new byte[8192];
            boolean entryEnd = false, outputEnd = false;
            while (!outputEnd) {
                if (!entryEnd) {
                    int i = ac.dequeueInputBuffer(WAIT_US);
                    if (i >= 0) {
                        ByteBuffer b = ac.getInputBuffer(i);
                        b.clear();
                        int want = Math.min(segment.length, b.capacity());
                        want -= want % bytesBySample;
                        int n = in.read(segment, 0, want);
                        long pts = samples * 1000000L / rate;
                        if (n <= 0) {
                            ac.queueInputBuffer(i, 0, 0, pts, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                            entryEnd = true;
                        } else {
                            b.put(segment, 0, n);
                            ac.queueInputBuffer(i, 0, n, pts, 0);
                            samples += n / bytesBySample;
                        }
                    }
                }
                int o = ac.dequeueOutputBuffer(bi, WAIT_US);
                if (o == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) { trackA = am.addTrack(ac.getOutputFormat()); am.start(); muxOk = true; }
                else if (o >= 0) {
                    ByteBuffer b = ac.getOutputBuffer(o);
                    if ((bi.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) bi.size = 0;
                    if (bi.size > 0 && muxOk && b != null) { b.position(bi.offset); b.limit(bi.offset + bi.size); am.writeSampleData(trackA, b, bi); }
                    ac.releaseOutputBuffer(o, false);
                    if ((bi.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) outputEnd = true;
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
    private static boolean collect(String video, String audio, String output) {
        MediaMuxer m = null; MediaExtractor ev = null, ea = null; boolean on = false;
        try {
            new File(output).delete();
            m = new MediaMuxer(output, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            ev = new MediaExtractor(); ev.setDataSource(video); ev.selectTrack(0);
            ea = new MediaExtractor(); ea.setDataSource(audio); ea.selectTrack(0);
            int tv = m.addTrack(ev.getTrackFormat(0)), ta = m.addTrack(ea.getTrackFormat(0));
            m.start(); on = true;
            // INTERCALADAS por tiempo (el MP4 las quiere asi): siempre la muestra mas temprana de las dos pistas
            ByteBuffer b = ByteBuffer.allocate(4 * 1024 * 1024);
            MediaCodec.BufferInfo bi = new MediaCodec.BufferInfo();
            boolean hasV = true, hasA = true;
            while (hasV || hasA) {
                long t_v = hasV ? ev.getSampleTime() : Long.MAX_VALUE, t_a = hasA ? ea.getSampleTime() : Long.MAX_VALUE;
                if (t_v < 0) { hasV = false; continue; }
                if (t_a < 0) { hasA = false; continue; }
                boolean fromVideo = t_v <= t_a;
                MediaExtractor ex = fromVideo ? ev : ea;
                int n = ex.readSampleData(b, 0);
                if (n < 0) { if (fromVideo) hasV = false; else hasA = false; continue; }
                bi.offset = 0; bi.size = n; bi.presentationTimeUs = ex.getSampleTime();
                bi.flags = (ex.getSampleFlags() & MediaExtractor.SAMPLE_FLAG_SYNC) != 0 ? MediaCodec.BUFFER_FLAG_KEY_FRAME : 0;
                m.writeSampleData(fromVideo ? tv : ta, b, bi);
                if (!ex.advance()) { if (fromVideo) hasV = false; else hasA = false; }
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
