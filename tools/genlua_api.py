#!/usr/bin/env python3
# ============================================================================================================
#  genlua_api.py -- la AYUDA DE LA API DE LUA para el IDE (el autocompletado y "Ir a la definicion"): la firma y la
#  descripcion de cada funcion, sacadas de las tablas del manual (docs/manual/<idioma>/lua/*.md), en
#  res/languages/lua-<idioma>.txt, una por idioma del editor.
#
#    python3 tools/genlua_api.py
#
#  Cada renglon de una tabla del manual que arranca con una firma entre comillas invertidas,
#      | `setRotationZ(obj, g)` | Cambia un solo eje... |
#  (o varias: `getRotationX(obj)`, `getRotationY(obj)` ...) da una linea  nombre<TAB>firma<TAB>descripcion.
#  El editor carga solo el del idioma elegido, y recien cuando el IDE lo necesita. Correrlo de nuevo cada vez que
#  cambie la API en el manual.
# ============================================================================================================
import os, re, sys

RAIZ = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
MANUAL = os.path.join(RAIZ, 'docs', 'manual')
SALIDA = os.path.join(RAIZ, 'res', 'languages')
IDIOMAS = ['en', 'es', 'pt', 'fr', 'it', 'tr', 'de', 'pl', 'ru']   # los del editor (libs/Whisk3DCore/config/W3dLang.cpp)

FIRMA = re.compile(r'`([A-Za-z_][A-Za-z0-9_.]*)\(([^`]*)`')


def celdas(renglon):
    # las celdas de un renglon de tabla, respetando los \| escapados
    partes, actual, i = [], '', 0
    r = renglon.strip()
    if r.startswith('|'): r = r[1:]
    if r.endswith('|'): r = r[:-1]
    while i < len(r):
        if r[i] == '\\' and i + 1 < len(r) and r[i + 1] == '|':
            actual += '|'; i += 2; continue
        if r[i] == '|':
            partes.append(actual.strip()); actual = ''; i += 1; continue
        actual += r[i]; i += 1
    partes.append(actual.strip())
    return partes


def texto_plano(md):
    md = re.sub(r'<kbd>(.*?)</kbd>', r'\1', md)
    md = re.sub(r'<[^>]+>', '', md)
    md = re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', md)     # [texto](link) -> texto
    md = md.replace('**', '').replace('`', '')
    md = re.sub(r'(?<![A-Za-z0-9])_([^_]+)_(?![A-Za-z0-9])', r'\1', md)
    # (la fuente del editor no tiene flechas, ni puntos suspensivos, ni rayas: van en ASCII)
    for a, b in (('→', '->'), ('←', '<-'), ('…', '...'), ('—', '-'), ('–', '-'), ('“', '"'), ('”', '"'), ('’', "'"),
                 ('×', 'x'), ('≤', '<='), ('≥', '>='), ('≠', '~=')):
        md = md.replace(a, b)
    return ' '.join(md.split())


def leer_idioma(cod):
    carpeta = os.path.join(MANUAL, cod, 'lua')
    if not os.path.isdir(carpeta):
        return None
    datos = {}   # nombre -> (firma, descripcion): la primera vez que aparece
    for nombre in sorted(os.listdir(carpeta)):
        if not nombre.endswith('.md'):
            continue
        for renglon in open(os.path.join(carpeta, nombre), encoding='utf-8'):
            if not renglon.startswith('|'):
                continue
            cs = celdas(renglon)
            if len(cs) < 2 or not cs[0].startswith('`'):
                continue
            desc = texto_plano(cs[1])
            for m in FIRMA.finditer(cs[0]):
                n = m.group(1)
                firma = (n + '(' + m.group(2)).strip()
                if n not in datos:
                    datos[n] = (texto_plano(firma), desc)
    return datos


def main():
    total = 0
    for cod in IDIOMAS:
        datos = leer_idioma(cod)
        if datos is None:
            print('sin manual de Lua en', cod, '(se saltea)')
            continue
        destino = os.path.join(SALIDA, 'lua-%s.txt' % cod)
        with open(destino, 'w', encoding='utf-8', newline='\n') as f:
            f.write('# Whisk3D: la ayuda de la API de Lua para el IDE. La genera tools/genlua_api.py desde '
                    'docs/manual/%s/lua: no editar a mano\n' % cod)
            for n in sorted(datos, key=str.lower):
                firma, desc = datos[n]
                f.write('%s\t%s\t%s\n' % (n, firma.replace('\t', ' '), desc.replace('\t', ' ')))
        total += len(datos)
        print('%s: %d funciones -> %s' % (cod, len(datos), os.path.relpath(destino, RAIZ)))
    if not total:
        sys.exit('no se genero nada')


if __name__ == '__main__':
    main()
