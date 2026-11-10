# Som e música

O **W3dAudio** é um **mixer por software** escrito em C++03, idêntico em todos os sistemas (a mesma ideia que o Quake usava). Ele mixa até 64 vozes em um fluxo estéreo de 16 bits, com interpolação linear (as mudanças de tom não soam granuladas), um limitador de picos, envelopes ADSR, reverb e um relógio próprio para que um sequenciador de música não dependa de quando o quadro roda.

Cada sistema contribui só com a **saída**: quatro funções (`W3dAudioBackendInit`, `Shutdown`, `Lock`, `Unlock`, em `W3dAudioBackend.h`) que abrem o dispositivo e pedem ao mixer o próximo pedaço.

| Sistema | Saída |
|---|---|
| PC, Android, navegador | SDL2 (44.100 Hz) |
| Symbian | `CMdaAudioOutputStream` em sua própria thread, com buffer duplo (22.050 Hz no N95) |
| Consoles | O editor compilado para console vai sem som. Um jogo compilado com "Usar som" leva o mixer com a saída do SDL |
| Dreamcast | Além disso, o `w3dAICA`: um módulo opcional que usa o chip de som diretamente (64 canais por hardware e ADPCM de 4 bits). Não convive com o W3dAudio |

Há mais três peças:

- **W3dMusic:** a música longa (mp3, ogg, aac) não é convertida para PCM: o sistema a decodifica, quase sempre por hardware. Hoje tem backend para o navegador (um elemento `<audio>`); os do Symbian e do desktop estão pendentes.
- **W3dClip:** sons curtos de interface. No navegador, vão pelo WebAudio, porque no iOS dispará-los com `<audio>` trava o celular. No resto, são sons do mixer.
- **W3dVolumen:** o volume geral e o mudo valem para tudo o que soa e são salvos sozinhos na configuração.

Por que importa: o jogo de Android saía **mudo** com os .wav bem empacotados dentro do APK, porque eram abertos com `fopen`, e lá dentro eles não são arquivos. Desde que o som passou a ser carregado por `ReadFileBytes`, funciona em todo lugar. Em Lua: [Som](../lua/api-sonido.md).
