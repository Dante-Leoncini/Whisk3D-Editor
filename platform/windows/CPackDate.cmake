# CPACK_PROJECT_CONFIG_FILE: CPack lo ejecuta en CADA corrida de cpack (no al configurar).
# Recalcula la version = fecha local AA.MM.DD del momento de empaquetar, asi el instalador
# siempre lleva la fecha del dia en que se genero, sin tener que reconfigurar cmake.
string(TIMESTAMP _w3d_date "%y.%m.%d")
set(CPACK_PACKAGE_VERSION "${_w3d_date}")
# win64 / win32 (CPACK_SYSTEM_NAME lo pone CPack segun el build: el de 32 bits es para Windows 7 / netbooks)
if(CPACK_SYSTEM_NAME)
    set(CPACK_PACKAGE_FILE_NAME "Whisk3D-${_w3d_date}-${CPACK_SYSTEM_NAME}")
else()
    set(CPACK_PACKAGE_FILE_NAME "Whisk3D-${_w3d_date}-win64")
endif()
