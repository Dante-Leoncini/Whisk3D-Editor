-- ==========================================================================
--  Lo que un script del editor NO puede hacer (libs/Whisk3DCore/script/W3dScript.cpp: OpenLibs y kLoadMode).
--  Un proyecto bajado de internet corre sus scripts al apretar Play (y al elegir el objeto, para leer sus
--  propiedades): no tiene que poder leer archivos de la maquina ni cargar bytecode, que no tiene verificador y con
--  uno armado a mano se ejecuta codigo nativo. Cada propiedad vale 1 si la regla se cumple.
-- ==========================================================================
-- (un bytecode, aunque sea de mentira: lo que importa es POR QUE lo rechaza -por el modo, no por estar mal armado-)
local bytecode, why = load("\27LuaT\0\25\147\13\10\26\10", "x", "b")
local text = load("return 2 + 3", "x", "bt")
properties = {
  noLoadfile = (loadfile == nil) and 1 or 0,
  noDofile = (dofile == nil) and 1 or 0,
  noDump = (string.dump == nil) and 1 or 0,
  noBytecode = (bytecode == nil and string.find(why or "", "mode is 't'", 1, true)) and 1 or 0,
  textLoads = text and text() or 0,
  noIo = (io == nil and os == nil and package == nil and debug == nil and require == nil) and 1 or 0,
}
