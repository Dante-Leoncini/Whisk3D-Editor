# Whisk3D en Android

Backend GLES1 + SDL2, empaquetado como APK con Gradle.

## Requisitos

- **Android NDK** (define `ANDROID_NDK` o dejalo en `/opt/android-ndk`).
- **JDK 17** + el `gradlew` que ya viene en `platform/android/`. Gradle necesita `JAVA_HOME` apuntando al JDK
  (si no, falla con *"JAVA_HOME is not set"*), por ejemplo:
  `export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64`

## Compilar (lo más fácil)

Desde la raíz del repo:

```bash
platform/android/build_android.sh
```

El script:
1. Copia `res/` a los assets del APK.
2. Compila las librerías nativas con `ndk-build` para las ABIs (por defecto `arm64`, `amd64`, `arm32`).
3. Empaqueta el APK con `./gradlew assembleDebug` (firmado con el keystore de debug).

Para una sola ABI: `platform/android/build_android.sh arm64`.

## A mano (solo el APK, si ya están las `.so`)

```bash
cd platform/android && ./gradlew assembleDebug
```

## Salida

El APK queda en `platform/android/app/distribution/android/app/outputs/apk/debug/whisk3d-*-Android.apk`.
