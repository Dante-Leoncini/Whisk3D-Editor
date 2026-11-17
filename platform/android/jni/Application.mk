APP_ABI := arm64-v8a armeabi-v7a x86_64
APP_PLATFORM := android-21
APP_STL := c++_shared
# PAGINAS DE MEMORIA DE 16 KB (Android 15+ en telefonos nuevos): las .so se enlazan alineadas a 16 KB
# (-Wl,-z,max-page-size=16384). Sin esto Google Play avisa que la app puede no instalarse o no arrancar en esos
# telefonos. Lo entiende el NDK r27 (con r28 o mas nuevo ya es lo de fabrica).
APP_SUPPORT_FLEXIBLE_PAGE_SIZES := true
