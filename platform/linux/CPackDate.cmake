# CPACK_PROJECT_CONFIG_FILE: CPack lo ejecuta en CADA corrida de cpack (no al configurar).
# Recalcula la version = fecha local AA.MM.DD del momento de empaquetar, asi el .deb siempre
# lleva la fecha del dia en que se genero, sin tener que reconfigurar cmake.
# (El string(TIMESTAMP) del CMakeLists corre solo al CONFIGURAR y quedaba viejo: un build dir
# configurado el 2 de agosto seguia sacando whisk3d-26.08.02-Linux.deb el dia 4.)
# OJO: no alcanza con pisar la VERSION. CPACK_PACKAGE_FILE_NAME ya viene armado desde
# CPackConfig.cmake (se calculo al CONFIGURAR, con la fecha de ese dia), asi que hay que pisarlo
# tambien o el .deb sale con el nombre viejo aunque la version de adentro sea la de hoy.
# Es la misma linea que ya tenia el de Windows; aca faltaba.
string(TIMESTAMP _w3d_date "%y.%m.%d")
set(CPACK_PACKAGE_VERSION "${_w3d_date}")
set(CPACK_PACKAGE_FILE_NAME "whisk3d-${_w3d_date}-Linux")
