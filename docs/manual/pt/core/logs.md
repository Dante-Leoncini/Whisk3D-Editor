# Logs

Três níveis no estilo printf: `w3dLogf` (informação), `w3dLogfW` (aviso) e `w3dLogfE` (erro). O `g_w3dLogNivel` filtra em tempo de execução (3 = tudo, 0 = nada), e compilar com `W3D_DEV_LOG=0` apaga as chamadas do executável. As últimas linhas também ficam na memória: é o que o [Console](../editor/consola.md) do editor mostra.

| Sistema | Para onde vai |
|---|---|
| PC | `whisk3d.log`, cada linha com uma marca em milissegundos e `[INFO]`, `[WARN]` ou `[ERROR]` |
| Android | logcat, com a tag `Whisk3D` (`adb logcat -s Whisk3D`) |
| Navegador | O console: `console.log`, `console.warn` e `console.error`, conforme o nível |
| Symbian | `E:\whisk3d.log` |
| Dreamcast | A porta serial (dc-tool ou o console do Flycast) |
| PSP | A saída padrão (PPSSPP, psplink) e `whisk3d.log` ao lado do `EBOOT.PBP` |
| PS Vita | `sceClibPrintf` e `ux0:data/Whisk3D/whisk3d.log` |
| 3DS | `svcOutputDebugString` e `sdmc:/3ds/Whisk3D/whisk3d.log` |

Por que importa: num console de verdade não há terminal, e o log do cartão é a única coisa que sobra depois de um travamento. Por isso, no 3DS, no Vita e no PSP, cada linha é escrita na hora. No N95 acontecia o contrário: escrever cada linha na hora levava dezenas de milissegundos e travava o jogo. Lá, as informações se acumulam e são gravadas a cada 3 KB, os avisos são gravados de imediato e os erros são forçados para o disco. Em Lua: [Mensagens e depuração](../lua/api-mensajes-y-depuracion.md).
