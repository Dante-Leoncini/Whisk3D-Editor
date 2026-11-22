# Tuş ve düğme adları

## `key` ve `keyPressed` için

| Nerede | Adlar |
|---|---|
| PC, Android ve Web | `"a"` … `"z"` (küçük harfle), `"0"` … `"9"`, `"up"`, `"down"`, `"left"`, `"right"`, `"space"`, `"enter"`, `"shift"`, `"ctrl"` |
| Nokia N95 (telefon tuş takımı) | `"up"`, `"down"`, `"left"`, `"right"`, `"enter"` (okların ortasındaki tuş `"space"` da gönderir), `"softleft"`, `"softright"`, `"0"` … `"9"`, `"*"`, `"#"` |

> [!NOTE]
> Esc için bir ad yoktur: editörde Esc oyunu duraklatır ve fareyi serbest bırakır. F tuşları ve Tab için de ad yoktur.

## `button` ve `buttonPressed` için (oyun kumandası)

`"a"`, `"b"`, `"x"`, `"y"`, `"up"`, `"down"`, `"left"`, `"right"` (yön tuşları). Analog çubuk hareketsizken yön tuşları `stick("left")`'yi de hareket ettirir.

Ayrıca: `"l1"`, `"r1"` (omuz tuşları), `"l2"`, `"r2"` (düğme olarak tetikler: yarıdan fazla basılıysa basılı sayılır; analog değer için `trigger` var), `"l3"`, `"r3"` (basılan analog çubuklar), `"start"` ve `"select"`. Adlar bir PlayStation kumandasınınkilerdir; bir Xbox kumandasında bunlar LB / RB, LT / RT ve Back'tir. Harfler düğmenin üzerindeki çizimi değil konumunu takip eder: `"a"` alttaki (çarpı ×), `"b"` sağdaki (daire ○), `"x"` soldaki (kare □) ve `"y"` üstteki (üçgen △) düğmedir.
