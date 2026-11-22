package com.whisk3d;

import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;

import org.libsdl.app.SDLActivity;

// Actividad principal de Whisk3D. Extiende SDLActivity (SDL2). Antes de arrancar,
// SDL carga las librerias nativas que devuelve getLibraries(): libSDL2.so + libmain.so.
public class Whisk3DActivity extends SDLActivity {
    // EL VIDEO DEL RENDER (W3dVideo: el codificador del telefono). Lo llama el codigo nativo por JNI (Properties.cpp,
    // W3D_VIDEO_ANDROID): por la actividad, para que las clases se busquen con el class loader de la app
    private final W3dVideo video = new W3dVideo();
    public boolean videoOpen(String path, int w, int h, int fps, int kbps) { return video.open(path, w, h, fps, kbps); }
    public boolean videoFrame(byte[] rgba, int w, int h) { return video.frame(rgba, w, h); }
    public boolean videoClose(String wav, int kbpsAudio) { return video.close(wav, kbpsAudio); }
    public boolean audioAac(String wav, String output, int kbps) { return W3dVideo.aacOfWav(wav, output, kbps); }

    @Override
    protected String[] getLibraries() {
        return new String[]{
                "SDL2",
                "main",
        };
    }

    // La RUTA del proyecto .w3d a abrir, como argv[1] del main nativo (el mismo
    // flujo que el doble click en PC: constructor.cpp -> w3dPath -> OpenW3D).
    // Llega por el extra "w3d" (adb / otras apps) o por un VIEW del gestor de
    // archivos (file:// directo; content:// de DocumentsUI se traduce a ruta real,
    // legible gracias al permiso All Files Access de abajo).
    @Override
    protected String[] getArguments() {
        Intent it = getIntent();
        if (it != null) {
            String extra = it.getStringExtra("w3d");
            if (extra != null && !extra.isEmpty()) return new String[]{ extra };
            Uri d = it.getData();
            if (d != null) {
                if ("file".equals(d.getScheme()) && d.getPath() != null)
                    return new String[]{ d.getPath() };
                if ("content".equals(d.getScheme())) {
                    // Lo IMPORTANTE es conseguir la RUTA REAL del archivo: un .w3d
                    // de texto referencia archivos HERMANOS (w3dui, lua, obj) y esos
                    // solo estan al lado del ORIGINAL. La copia a cache es el ultimo
                    // recurso (con el v2 empaquetado alcanza; con el texto viejo no).
                    android.util.Log.i("Whisk3D", "VIEW uri: " + d);
                    // 1) DocumentsUI: document/primary:Download/x.w3d
                    String p = d.getLastPathSegment(); // ya viene decodificado
                    if (p != null && p.startsWith("primary:")) {
                        String path = Environment.getExternalStorageDirectory()
                                      + "/" + p.substring("primary:".length());
                        if (new java.io.File(path).canRead()) return new String[]{ path };
                    }
                    // 2) MediaStore y varios gestores: la columna _data ES la ruta
                    try {
                        android.database.Cursor c = getContentResolver().query(
                                d, new String[]{ "_data" }, null, null, null);
                        if (c != null) {
                            String path = null;
                            if (c.moveToFirst()) {
                                int col = c.getColumnIndex("_data");
                                if (col >= 0) path = c.getString(col);
                            }
                            c.close();
                            if (path != null && new java.io.File(path).canRead())
                                return new String[]{ path };
                        }
                    } catch (Exception e) { /* el proveedor no expone _data */ }
                    // 3) proveedores que EMBEBEN la ruta en la URI (Files de Google:
                    //    .../2/storage/emulated/0/Download/x.w3d, o .../sdcard/...)
                    String up = d.getPath();
                    if (up != null) {
                        int i2 = up.indexOf("/storage/");
                        if (i2 < 0) { i2 = up.indexOf("/sdcard/"); }
                        if (i2 >= 0) {
                            String path = up.substring(i2);
                            if (new java.io.File(path).canRead()) return new String[]{ path };
                        }
                    }
                    // 4) ultimo recurso: COPIAR el contenido a la cache. Solo viaja
                    //    ESTE archivo: un w3d v2 EMPAQUETADO abre completo; un texto
                    //    viejo pierde sus archivos hermanos.
                    try {
                        java.io.InputStream in = getContentResolver().openInputStream(d);
                        if (in != null) {
                            java.io.File f = new java.io.File(getCacheDir(), "abierto.w3d");
                            java.io.OutputStream out = new java.io.FileOutputStream(f);
                            byte[] buf = new byte[65536];
                            int n;
                            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
                            out.close();
                            in.close();
                            return new String[]{ f.getAbsolutePath() };
                        }
                    } catch (Exception e) { /* sin permiso o stream roto: escena default */ }
                }
            }
        }
        return new String[0];
    }

    // Android 11+ (API 30): el almacenamiento es "scoped". Los permisos de media solo dan imagenes/videos, asi
    // que el file browser interno (opendir/fopen) NO podia leer los modelos .obj/.w3d ni sus texturas. "All files
    // access" (MANAGE_EXTERNAL_STORAGE) habilita leer/escribir CUALQUIER archivo, como en Android <=10, sin tener
    // que reescribir el browser con SAF. Se pide una sola vez: si no esta otorgado, se abre la pantalla de Settings.
    // la PANTALLA tiene recorte (notch / camara)? Lo pregunta el codigo nativo (W3dKeyboard.cpp) al arrancar: sin notch
    // la app va en pantalla completa (la barra de arriba ocupa espacio valioso); con notch se deja la barra, que es
    // donde esta la camara. Antes de Android 9 no hay API de recortes -> pantalla completa.
    public boolean hasNotch() {
        try {
            if (Build.VERSION.SDK_INT >= 29) {
                android.view.DisplayCutout c = getWindowManager().getDefaultDisplay().getCutout();
                return c != null && !c.getBoundingRects().isEmpty();
            }
            if (Build.VERSION.SDK_INT >= 28) {
                android.view.WindowInsets wi = getWindow().getDecorView().getRootWindowInsets();
                if (wi == null) return true;   // sin datos todavia: no arriesgar a tapar la camara
                android.view.DisplayCutout c = wi.getDisplayCutout();
                return c != null && !c.getBoundingRects().isEmpty();
            }
        } catch (Throwable t) {
            return true;
        }
        return false;
    }

    // CERRAR DE VERDAD. Cuando main() termina (el usuario eligio "Cerrar" despues de Atras, o el sistema destruyo la
    // actividad) SDL cierra la actividad pero el PROCESO sigue vivo con los globales de C++ del .so: al volver a abrir,
    // main() arrancaria otra vez con la escena de antes cargada (se duplicaba). Asi que, cerrada la actividad, se
    // termina el proceso (despues de que Android termine de destruirla)
    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (isFinishing()) {
            new android.os.Handler(android.os.Looper.getMainLooper()).post(new Runnable() {
                @Override
                public void run() { System.exit(0); }
            });
        }
    }

    // EL TECLADO DEL SISTEMA: cuanto de la superficie de SDL tapa, en pixeles (W3dKeyboard.cpp). En pantalla completa
    // Android no achica la ventana con el teclado: sin esto el editor no se enteraba y el campo quedaba debajo. Con la
    // ventana achicada da 0 (ya no tapa nada). Se avisa solo cuando cambia
    private static native void nativeKeyboardCovered(int px);
    private int coveredNotified = -1;
    private void watchKeyboard() {
        final android.view.View decor = getWindow().getDecorView();
        decor.getViewTreeObserver().addOnGlobalLayoutListener(new android.view.ViewTreeObserver.OnGlobalLayoutListener() {
            @Override
            public void onGlobalLayout() {
                if (mSurface == null) return;
                android.graphics.Rect r = new android.graphics.Rect();
                decor.getWindowVisibleDisplayFrame(r);
                int[] p = new int[2];
                mSurface.getLocationOnScreen(p);
                int h = mSurface.getHeight();
                int covered = p[1] + h - r.bottom;
                if (covered < h * 15 / 100) covered = 0;   // (la barra de navegacion no es el teclado)
                if (covered == coveredNotified) return;
                try { nativeKeyboardCovered(covered); coveredNotified = covered; } catch (Throwable t) { /* (sin libmain todavia) */ }
            }
        });
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        watchKeyboard();
        // Android 10+: sin la capa semitransparente de "contraste" que el sistema pone sobre la barra de navegacion
        // (con el tema claro salia blanquecina). El fondo de las barras es el negro de la ventana (styles.xml). La de
        // estado no lleva esa capa por defecto (y su setStatusBarContrastEnforced quedo obsoleto en Android 15).
        if (Build.VERSION.SDK_INT >= 29) {
            getWindow().setNavigationBarContrastEnforced(false);
        }
        // ANDROID 15+ con targetSdk 35/36: la ventana va SIEMPRE de borde a borde. Sin esto, con la barra de estado a
        // la vista (pantalla con notch) el editor se dibujaba DEBAJO de la barra y de la camara, y el teclado ya no
        // achicaba la ventana (adjustResize ahora solo avisa): W3dKeyboard.cpp lo detecta porque la ventana se achica.
        // Se le da al contenido el margen de las barras del sistema, del recorte y del teclado. En pantalla completa
        // las barras estan ocultas y su margen es 0.
        if (Build.VERSION.SDK_INT >= 35) {
            android.view.View content = getWindow().getDecorView().findViewById(android.R.id.content);
            if (content != null) {
                content.setOnApplyWindowInsetsListener(new android.view.View.OnApplyWindowInsetsListener() {
                    @Override
                    public android.view.WindowInsets onApplyWindowInsets(android.view.View v, android.view.WindowInsets wi) {
                        android.graphics.Insets m = wi.getInsets(android.view.WindowInsets.Type.systemBars()
                                | android.view.WindowInsets.Type.displayCutout() | android.view.WindowInsets.Type.ime());
                        v.setPadding(m.left, m.top, m.right, m.bottom);
                        return wi;
                    }
                });
            }
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                try {
                    Intent i = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                                          Uri.parse("package:" + getPackageName()));
                    startActivity(i);
                } catch (Exception e) {
                    // fallback: la lista general de "acceso a todos los archivos" (algunos OEM no tienen la per-app)
                    try { startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION)); }
                    catch (Exception e2) { /* sin la pantalla de settings: el usuario lo activa a mano */ }
                }
            }
        }
    }

    // la VIBRACION (input/W3dVibrate.cpp la llama por JNI): el agarre de la biblioteca (pulsacion larga) avisa asi
    // que ya se puede arrastrar
    public void vibrate(int ms) {
        try {
            android.os.Vibrator v = (android.os.Vibrator) getSystemService(android.content.Context.VIBRATOR_SERVICE);
            if (v != null) v.vibrate(ms);
        } catch (Exception e) {}
    }
}
