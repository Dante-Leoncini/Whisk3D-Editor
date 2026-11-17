#!/usr/bin/env python3
# ============================================================================
#  android_jni_check.py - que R8 no haya roto las llamadas JNI del AAB de Google Play
# ----------------------------------------------------------------------------
#  El release de Android (bundleRelease) pasa por R8, que borra y renombra el Java que nadie usa... pero no ve las
#  llamadas desde C/C++ (GetMethodID / GetStaticMethodID con el nombre en un string) ni los metodos native que el C
#  registra por nombre. Las reglas para guardarlos estan en platform/android/app/proguard-rules.pro.
#
#  Este script lo comprueba contra el .dex REAL del AAB:
#    1) junta del codigo nativo (SDL + Whisk3D) cada GetMethodID / GetStaticMethodID("nombre", "firma") de un
#       metodo que esta en el Java de la app (org.libsdl.app / com.whisk3d), no del framework de Android
#    2) junta del Java cada metodo native
#    3) se fija que cada uno este en el .dex, en su clase, con el mismo nombre y la misma firma
#  Si falta alguno, la app se caeria al usarlo: hay que agregarlo a proguard-rules.pro.
#
#  Uso (desde la raiz del repo, despues de ./gradlew bundleRelease):
#    python3 tools/android_jni_check.py [ruta.aab]
# ============================================================================
import glob, os, re, subprocess, sys, tempfile, zipfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ANDROID = os.path.join(RAIZ, "platform", "android")
AAB = os.path.join(ANDROID, "app/distribution/android/app/outputs/bundle/release/app-release.aab")
JAVA = os.path.join(ANDROID, "app/src/main/java")
NATIVO = [os.path.join(RAIZ, "thirdparty/SDL2/src/core/android"), os.path.join(RAIZ, "thirdparty/SDL2/src/hidapi/android"),
          os.path.join(RAIZ, "main"), os.path.join(RAIZ, "libs")]


def dexdump():
    sdk = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT") or os.path.expanduser("~/Android/Sdk")
    hay = sorted(glob.glob(os.path.join(sdk, "build-tools", "*", "dexdump")))
    if not hay:
        sys.exit("No encuentro dexdump en %s/build-tools" % sdk)
    return hay[-1]


def metodos_dex(aab):
    """{ 'Lorg/libsdl/app/SDLActivity;': {('nombre', '(firma)'), ...} } de todos los .dex del modulo base."""
    clases = {}
    with tempfile.TemporaryDirectory() as tmp, zipfile.ZipFile(aab) as z:
        # el AAB los trae en base/dex/; un APK (assembleRelease), en la raiz
        dexs = [n for n in z.namelist() if re.match(r"(base/dex/)?classes\d*\.dex$", n)]
        for n in dexs:
            ruta = z.extract(n, tmp)
            salida = subprocess.run([dexdump(), ruta], capture_output=True, text=True, check=True).stdout
            clase = None
            for linea in salida.splitlines():
                m = re.match(r"\s*Class descriptor\s*:\s*'([^']+)'", linea)
                if m:
                    clase = m.group(1); clases.setdefault(clase, set()); nombre = None; continue
                m = re.match(r"\s*name\s*:\s*'([^']+)'", linea)
                if m:
                    nombre = m.group(1); continue
                m = re.match(r"\s*type\s*:\s*'(\([^']*)'", linea)
                if m and clase and nombre:
                    clases[clase].add((nombre, m.group(1))); nombre = None
    return clases


def java_de_la_app():
    """Los metodos declarados en el Java de la app: nombre -> [clase], y los native: [(clase, nombre)]."""
    declarados, nativos = {}, []
    for ruta in glob.glob(os.path.join(JAVA, "**", "*.java"), recursive=True):
        paquete = "L" + os.path.dirname(os.path.relpath(ruta, JAVA)) + "/"
        texto = sin_comentarios_ni_strings(open(ruta, encoding="utf-8", errors="replace").read())
        # un .java trae varias clases: SDLActivity.java define SDLInputConnection al lado y SDLCommandHandler ADENTRO
        # (en el .dex: SDLActivity$SDLCommandHandler). Cada metodo va con la clase cuyas llaves lo encierran.
        clases = []                                   # (desde, hasta, nombre en el .dex)
        for m in re.finditer(r"\bclass\s+(\w+)[^{;]*\{", texto):
            desde, hasta = m.end() - 1, llave_que_cierra(texto, m.end() - 1)
            afuera = [c for c in clases if c[0] < desde < c[1]]
            nombre = (max(afuera)[2] + "$" if afuera else "") + m.group(1)
            clases.append((desde, hasta, nombre))
        for m in re.finditer(r"^\s*(?:(?:public|private|protected|static|final|synchronized|native)\s+)+"
                             r"[\w\[\]<>.,? ]+?\s+(\w+)\s*\(", texto, re.M):
            adentro = [c for c in clases if c[0] < m.start() < c[1]]
            if not adentro:
                continue
            clase = paquete + max(adentro)[2] + ";"   # la que empieza mas tarde = la mas interna
            declarados.setdefault(m.group(1), []).append(clase)
            if re.search(r"\bnative\b", m.group(0)):
                nativos.append((clase, m.group(1)))
    return declarados, nativos


def sin_comentarios_ni_strings(texto):
    """Los comentarios y los strings/chars quedan en blanco (mismas posiciones) para contar llaves sin errores."""
    def blanco(m):
        return re.sub(r"[^\n]", " ", m.group(0))
    return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', blanco, texto, flags=re.S)


def llave_que_cierra(texto, abre):
    nivel = 0
    for i in range(abre, len(texto)):
        if texto[i] == "{":
            nivel += 1
        elif texto[i] == "}":
            nivel -= 1
            if nivel == 0:
                return i
    return len(texto)


def llamadas_nativas():
    """(nombre, firma, archivo) de cada GetMethodID / GetStaticMethodID del codigo nativo."""
    hay = set()
    for base in NATIVO:
        for ruta in glob.glob(os.path.join(base, "**", "*.c*"), recursive=True):
            if not ruta.endswith((".c", ".cpp")):
                continue
            texto = open(ruta, encoding="utf-8", errors="replace").read()
            for m in re.finditer(r'Get(?:Static)?MethodID\s*\([^;]*?"(\w+)"\s*,\s*"(\([^"]*)"', texto):
                hay.add((m.group(1), m.group(2), os.path.relpath(ruta, RAIZ)))
            # la forma de Properties.cpp: j.M("videoAbrir", "(Ljava/lang/String;IIII)Z")
            for m in re.finditer(r'\bM\(\s*"(\w+)"\s*,\s*"(\([^"]*)"\s*\)', texto):
                hay.add((m.group(1), m.group(2), os.path.relpath(ruta, RAIZ)))
    return hay


def main():
    aab = sys.argv[1] if len(sys.argv) > 1 else AAB
    if not os.path.exists(aab):
        sys.exit("No existe %s (correr ./gradlew bundleRelease en platform/android)" % aab)
    dex = metodos_dex(aab)
    declarados, nativos = java_de_la_app()
    faltan, revisados = [], 0

    for nombre, firma, archivo in sorted(llamadas_nativas()):
        if nombre not in declarados:
            continue                     # metodo del framework de Android (getAssets, registerReceiver...)
        revisados += 1
        clases = declarados[nombre]
        if not any((nombre, firma) in dex.get(c, ()) for c in clases):
            faltan.append("C->Java  %s%s  (%s; declarado en %s)" % (nombre, firma, archivo, ", ".join(clases)))

    for clase, nombre in nativos:
        revisados += 1
        if not any(n == nombre for n, _ in dex.get(clase, ())):
            faltan.append("native   %s.%s  (la clase o el metodo no esta en el .dex con ese nombre)" % (clase, nombre))

    print("%s: %d clases en el .dex, %d metodos JNI revisados" % (os.path.basename(aab), len(dex), revisados))
    if faltan:
        print("FALTAN %d (agregarlos a platform/android/app/proguard-rules.pro):" % len(faltan))
        for f in faltan:
            print("  " + f)
        sys.exit(1)
    print("OK: todo lo que el C/C++ llama por JNI sigue en el .dex con su nombre y su firma")


if __name__ == "__main__":
    main()
