#ifndef MIRROR_H
#define MIRROR_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include "WhiskUI/draw/icons.h"
#include "objects/Objects.h"
#include "Target.h"
#include <GL/gl.h>
#include <string>
#include <vector>

class Mirror : public Object, public Target {
    public:
        bool mirrorX = false;
        bool mirrorY = false;
        bool mirrorZ = true;
        bool RenderChildrens = true;

        // TARGETS EXTRA (varios objetos sobre el MISMO cuerpo de agua: el personaje + los
        // ranas + la caja...). El primero sigue siendo Target::target/targetName
        // para que un .w3d viejo (propiedad "target") abra igual; estos son los
        // que agrega la propiedad opcional "targets" y viajan como refs 1..N
        // (ver RefsPropias abajo: el undo y el borrado los limpian solos).
        std::vector<Object*>     targetsExtra;
        std::vector<std::string> targetsExtraNombre;

        // REFLEJO EN AGUA (espejo de plano horizontal con z-buffer): el fondo
        // del estanque es opaco y esta apenas bajo la superficie -> con el
        // test de profundidad el reflejo (que queda mas hondo) no se ve nunca.
        // sinProfundidad dibuja el reflejo SIN test NI escritura de z (como el
        // painter's del PSX).
        bool  sinProfundidad = false;
        // RECTANGULO AL QUE SE RECORTA EL REFLEJO, expresado EN EL PLANO DEL
        // ESPEJO: limU sobre EjeU() y limV sobre EjeV(), los dos ejes
        // perpendiculares a la normal. NO son un switch de "dibujar si / dibujar
        // no" (eso era el bug: un target con medio cuerpo afuera pintaba su
        // reflejo ENTERO sobre la orilla, y uno con el origen apenas afuera
        // perdia el reflejo entero): recortan de verdad.
        //
        // Antes eran limX0/limX1/limZ0/limZ1 y se comparaban contra .x/.z de
        // mundo, con el semiespacio "detras del espejo" clavado en -Y: o sea la
        // semantica de UN caso -- una lamina de agua horizontal. Un espejo de
        // PARED (mirrorZ, que ademas es el default de la clase) no se podia
        // acotar. Con U/V el rectangulo vive en el plano que el espejo tenga.
        //
        // Para el espejo horizontal sin rotar (mirrorY) EjeU() = X y EjeV() = Z,
        // asi que los numeros son los mismos de siempre y el formato no cambia
        // un solo byte: "limites": [u0, u1, v0, v1].
        bool  usaLimites = false;
        float limU0 = 0, limU1 = 0, limV0 = 0, limV1 = 0;
        // recortar el reflejo. Con estencil disponible son DOS recortes: la
        // SILUETA del agua en pantalla (mascara) + UN PLANO DE USUARIO sobre la
        // superficie, que tira todo lo que caiga del lado de ARRIBA (es el unico
        // que corta el reflejo de un cuerpo MEDIO SUMERGIDO: su parte hundida se
        // refleja hacia arriba y sale al aire). Sin estencil se cae al fallback
        // viejo: ese plano MAS las 4 paredes del rectangulo.
        // Default = usaLimites: un espejo comun declarado sin limites se dibuja
        // como siempre, y el del agua se recorta.
        // Se puede forzar con la propiedad "recorte".
        bool  recorte = false;
        bool  recorteDeclarado = false;   // true si el archivo trae "recorte"
        bool  Recorta() const { return recorteDeclarado ? recorte : usaLimites; }

        // ---- hundimientoMaximo: DEPRECADO (se acepta, se avisa por log, SIN efecto) ----
        // Fue un parche: clavaba el reflejo a la superficie cuando el objeto
        // saltaba, "para que no desaparezca". Veredicto del dueno: un espejo es
        // un espejo -- el objeto sube y el reflejo BAJA, siempre, la distancia
        // exacta. El reflejo desaparecia al saltar porque el recorte VIEJO lo
        // cortaba con planos de MUNDO (el rectangulo + el semiespacio); el
        // recorte correcto es SOLO la silueta del agua EN PANTALLA (la mascara
        // de estencil del pase A), y con eso el reflejo profundo se ve adentro
        // del agua hasta donde la silueta llega -- como en un espejo de verdad.
        // El campo queda solo para que los .w3d que lo declaren sigan abriendo.
        float hundimientoMaximo = 0.0f;

        Mirror(Object* parent, Vector3 pos = Vector3(0,0,0), bool x = false, bool y = false, bool z = true);

        // matriz MUNDO->MUNDO del reflejo: la MISMA que arma RenderObject (mundo del
        // espejo x Scale(-ejes)). La expone para que el que mide (el harness) proyecte
        // el reflejo de un punto sin recalcular la cuenta a mano y quedar desfasado.
        void MatrizEspejo(Matrix4& out) const;
        // HOY es identica a MatrizEspejo para cualquier target (hundimientoMaximo
        // esta deprecado y no corrige nada): queda por compatibilidad con el
        // harness, que mide por aca para medir lo mismo que dibuja RenderObject.
        void MatrizEspejoTarget(Object* tg, Matrix4& out) const;
        // ---- EL PLANO DEL ESPEJO, EN MUNDO --------------------------------
        // Normal() = el eje que MatrizEspejo NIEGA, llevado a mundo y normalizado.
        // PuntoDelPlano() = un punto cualquiera del plano (el punto medio entre el
        // origen y su reflejo, que por construccion cae sobre el plano).
        // EjeU()/EjeV() = los otros dos ejes del espejo, tambien en mundo: son las
        // dos direcciones en las que se mide el rectangulo de recorte.
        int     EjeEspejado() const;   // 0=X, 1=Y, 2=Z: el eje local que niega MatrizEspejo
        Vector3 Normal() const;
        Vector3 PuntoDelPlano() const;
        Vector3 EjeU() const;
        Vector3 EjeV() const;
        // altura del plano de reflexion en MUNDO (solo tiene sentido con mirrorY):
        // el punto fijo de MatrizEspejo sobre el eje Y. Se mantiene porque es lo
        // que miden las pruebas de pixeles del caso horizontal.
        float PlanoY() const;

        // targets efectivos (el de Target + los extra que resolvieron)
        int     TargetsVivos() const;
        Object* TargetVivo(int i) const;

        // EL GATE del reflejo: true si el target puede reflejarse (esta visible y
        // no quedo ENTERO del otro lado del plano -- hundido: su imagen caeria
        // arriba del agua, y un agua no refleja lo que tiene abajo). NO descarta
        // por el rectangulo: un espejo refleja tambien lo que esta AL LADO suyo
        // (el que camina por la orilla se ve en el agua); que pixeles se pintan
        // lo decide la SILUETA del agua en pantalla (la mascara de estencil).
        // Publico para que el harness lo pueda medir
        // (espejopx <espejo> info lista la decision por target).
        bool TargetSeRefleja(Object* tg) const;

        ObjectType getType() override;

        void Reload() override;

        void RenderObject() override;

        // sus refs por puntero: el target principal (ver Objects.h) + un slot por
        // target extra. Sin esto el espejo seguia dibujando un objeto
        // detachado/borrado (puntero colgado).
        int     RefsPropias() const override { return 1 + (int)targetsExtra.size(); }
        Object* RefPropia(int i) const override {
            if (i == 0) return target;
            i--; return (i >= 0 && i < (int)targetsExtra.size()) ? targetsExtra[i] : NULL;
        }
        void    SetRefPropia(int i, Object* o) override {
            if (i == 0) { target = o; return; }
            i--; if (i >= 0 && i < (int)targetsExtra.size()) targetsExtra[i] = o;
        }
        std::string* RefPropiaNombre(int i) override {
            if (i == 0) return &targetName;
            i--; return (i >= 0 && i < (int)targetsExtraNombre.size()) ? &targetsExtraNombre[i] : NULL;
        }

        // "targets" del formato: lista separada por comas. El PRIMERO va al target
        // historico; el resto a los extra. Vacio = no toca nada.
        void SetTargetsTexto(const std::string& lista);
        std::string TargetsTexto() const;   // la misma lista, para guardar

        ~Mirror();
};

#endif
