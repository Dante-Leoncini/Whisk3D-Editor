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
