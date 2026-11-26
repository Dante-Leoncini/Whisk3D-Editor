#!/usr/bin/env python3
# Uso: simbolos.py muestras.txt binario [top]
# Resume las muestras de muestras.c (ver ahi): por biblioteca, PROPIO (la funcion que corria), INCLUSIVO (estaba en
# la pila) y por la primera funcion del programa (lo que cae en el driver o en libc se le cuenta a quien lo llamo).
import collections
import subprocess
import sys

archivo, binario = sys.argv[1], sys.argv[2]
top = int(sys.argv[3]) if len(sys.argv) > 3 else 40
nombre_bin = binario.rsplit('/', 1)[-1]

muestras = [l.split() for l in open(archivo) if l.strip()]
direcciones = sorted({f.split('+')[1] for m in muestras for f in m if f.startswith(nombre_bin + '+')})
simbolo = {}
if direcciones:
    salida = subprocess.run(['addr2line', '-f', '-C', '-e', binario] + ['0x' + a for a in direcciones],
                            capture_output=True, text=True).stdout.split('\n')
    for i, a in enumerate(direcciones):
        nombre = salida[2 * i] if 2 * i < len(salida) else '?'
        simbolo[a] = nombre.split('(')[0][:90]


def nombre_de(f):
    lib, off = f.split('+', 1)
    if lib == nombre_bin:
        return simbolo.get(off, '?')
    return '[' + lib + ']'


propio = collections.Counter()
inclusivo = collections.Counter()
libs = collections.Counter()
for m in muestras:
    if not m:
        continue
    propio[nombre_de(m[0])] += 1
    libs[m[0].split('+')[0]] += 1
    vistos = set()
    for f in m:
        n = nombre_de(f)
        if n not in vistos:
            vistos.add(n)
            inclusivo[n] += 1

total = len(muestras)
print(f'{total} muestras')
print('--- por biblioteca')
for k, v in libs.most_common(12):
    print(f'{100.0 * v / total:6.1f}% {k}')
print('--- propio')
for k, v in propio.most_common(top):
    print(f'{100.0 * v / total:6.1f}% {k}')
print('--- inclusivo')
for k, v in inclusivo.most_common(top):
    print(f'{100.0 * v / total:6.1f}% {k}')
print('--- por la primera funcion del programa en la pila (lo que cae en bibliotecas se le cuenta a quien llamo)')
primero = collections.Counter()
for m in muestras:
    n = '(sin el programa en la pila)'
    for f in m:
        if f.startswith(nombre_bin + '+'):
            n = nombre_de(f)
            break
    primero[n] += 1
for k, v in primero.most_common(top):
    print(f'{100.0 * v / total:6.1f}% {k}')
