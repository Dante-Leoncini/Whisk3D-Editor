# Escribe el header con la version = fecha del BUILD (AA.MM.DD).
# Corre como custom target en CADA compilacion, no al configurar: el string(TIMESTAMP) del
# CMakeLists se evalua una sola vez (al configurar) y el binario quedaba con una fecha vieja.
# Solo reescribe el archivo si el contenido CAMBIO, asi no fuerza un recompile por build:
# a lo sumo uno por dia, cuando cambia la fecha.
string(TIMESTAMP _w3d_date "%y.%m.%d")
set(_w3d_nuevo "#define W3D_VERSION \"${_w3d_date}\"\n")

set(_w3d_viejo "")
if(EXISTS "${OUT}")
    file(READ "${OUT}" _w3d_viejo)
endif()

if(NOT _w3d_viejo STREQUAL _w3d_nuevo)
    file(WRITE "${OUT}" "${_w3d_nuevo}")
endif()
