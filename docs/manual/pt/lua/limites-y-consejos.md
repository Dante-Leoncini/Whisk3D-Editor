# Limites e dicas

## Quais partes de Lua não estão disponíveis

- Estão `string`, `table` e `math`, mais as funções básicas (`print`, `pairs`, `ipairs`, `type`, `tostring`, `tonumber`, `pcall`, `assert`…).
- **Não** estão `io` nem `os` (um jogo não mexe em arquivos do disco; por isso não há `os.time`), nem `require`, `coroutine`, `utf8` ou `debug`. Para salvar dados, use `getConfig` / `saveConfig`.
- Cada script é independente: não pode ver as variáveis de outro. Para se comunicar, `setShared` / `getShared`.

## Erros

- Se uma função tiver um erro, ele é anotado no registro (`archivo.lua:LÍNEA: mensaje`) e essa chamada é interrompida, mas ela é chamada de novo no quadro seguinte: o mesmo erro se repete a cada quadro até você corrigi-lo.
- Se o arquivo não puder ser lido ou tiver um erro de sintaxe, esse script fica desligado até o próximo Play.
- `error("...")` no Whisk3D só anota a mensagem, e o script continua. Se você quiser interromper, use `assert(condicion, "mensaje")`.

## Coisas a ter em conta

- `update` só roda em objetos **visíveis** da cena ativa: ocultar um objeto desliga a sua lógica.
- `destroy`, `changeScene` e `playScene` são aplicados no fim do quadro, não no momento.
- A fonte dos textos do jogo não desenha acentos nem eñes: escreva `"MENU"` em vez de `"MENÚ"`.
- `safeArea`, `uiBox` e `resolution` só têm valores reais depois do primeiro quadro desenhado: não as use em `start`.
- Na Web e no Android, só a **primeira** luz ilumina.
- No Nokia N95 não há controle nem mouse: use `key` com os nomes do teclado do celular. Os números inteiros são de 32 bits.

## Para rodar rápido (sobretudo no celular)

- Busque os objetos uma única vez, em `start`, e guarde-os em variáveis `local`.
- Use as versões "de muitos de uma vez" (`setVertices`, `setVerticesColor`) em vez de chamar mil vezes a de um só.
- Prefira as funções do motor (`scrollUV`, `linePoint`, `setRotation` com ordem) a fazer as contas em Lua.
- Meça com `luaProfile` e `clockMs` antes de otimizar às cegas.
