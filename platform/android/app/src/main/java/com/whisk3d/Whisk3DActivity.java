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
                        String ruta = Environment.getExternalStorageDirectory()
                                      + "/" + p.substring("primary:".length());
                        if (new java.io.File(ruta).canRead()) return new String[]{ ruta };
                    }
                    // 2) MediaStore y varios gestores: la columna _data ES la ruta
                    try {
                        android.database.Cursor c = getContentResolver().query(
                                d, new String[]{ "_data" }, null, null, null);
                        if (c != null) {
                            String ruta = null;
                            if (c.moveToFirst()) {
                                int col = c.getColumnIndex("_data");
                                if (col >= 0) ruta = c.getString(col);
                            }
                            c.close();
                            if (ruta != null && new java.io.File(ruta).canRead())
                                return new String[]{ ruta };
                        }
                    } catch (Exception e) { /* el proveedor no expone _data */ }
                    // 3) proveedores que EMBEBEN la ruta en la URI (Files de Google:
                    //    .../2/storage/emulated/0/Download/x.w3d, o .../sdcard/...)
                    String up = d.getPath();
                    if (up != null) {
                        int i2 = up.indexOf("/storage/");
                        if (i2 < 0) { i2 = up.indexOf("/sdcard/"); }
                        if (i2 >= 0) {
                            String ruta = up.substring(i2);
                            if (new java.io.File(ruta).canRead()) return new String[]{ ruta };
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
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
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
}
