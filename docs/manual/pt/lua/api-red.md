# Jogar em rede (LAN, wi-fi ou internet)

Uma partida em rede tem um **anfitrião** (quem a cria: jogador 0) e os que **entram** (1, 2…). Tudo passa pelo
anfitrião: um jogador envia para o anfitrião e o anfitrião distribui. Quem está na mesma rede (a mesma LAN ou o mesmo
wi-fi) se encontra sozinho; pela internet, é preciso entrar com o endereço do anfitrião (e ele precisa ter a porta 47023
aberta no roteador).

| Função | O que faz |
|---|---|
| `netAvailable() → bool [, reason]` | Se este jogo pode usar a rede. Se não puder, o motivo: `"no permission"` (o editor não deu permissão a este projeto) ou `"no network"` (esta plataforma ainda não tem rede). |
| `netDevice() → text` | O nome deste dispositivo (o que os outros veem ao procurar partidas). Muda-se no editor: cartão **Ajustes**, *Nome do dispositivo*. |
| `netAddress() → text` | O IP deste aparelho na rede local (para entrar pela internet ou digitando-o). |
| `netHost(name [, max [, game]]) → bool [, error]` | Cria uma partida e espera jogadores (`max` contando o anfitrião: 2 por padrão). `game` é o nome do seu jogo: só aparecem as partidas do mesmo jogo. |
| `netOpen(bool)` | O anfitrião aparece nas buscas e aceita jogadores (`true`) ou não (por exemplo, quando a partida já começou). |
| `netFind([game]) → bool` | Começa a procurar partidas na rede local. Chamá-la de novo **atualiza** a lista. |
| `netMatches() → { {name, device, players, max, address}, … }` | As partidas que responderam (a lista se atualiza sozinha enquanto você procura). |
| `netJoin(address) → bool` | Entra numa partida: a `address` de `netMatches()` ou uma digitada (`"192.168.0.10"`, `"x.x.x.x:puerto"`). |
| `netState() → state, error` | `"none"`, `"searching"`, `"waiting"` (você é o anfitrião), `"connecting"`, `"connected"` ou `"error"` (com o motivo). |
| `netSelf() → id` | O seu número na partida (0 = o anfitrião; -1 sem partida). |
| `netPlayers() → { {id, name, isSelf}, … }` | Quem está na partida (o nome é o do dispositivo de cada um). |
| `netSilence([id]) → seconds, limite` | Há quanto tempo não chega nada desse jogador (sem `id`: do que está há mais tempo calado). Quem entra só ouve o anfitrião, então para ele é o silêncio do anfitrião. Passado o `limite` (8 s), o jogador é dado como ausente: o anfitrião o tira da partida e, para quem entrou, `netState()` retorna `"error"`. |
| `netSend(value [, para [, reliable]]) → bool` | Envia um valor de Lua (número, texto, booleano ou tabela, com tabelas dentro) para um jogador (`para` = seu id) ou para todos (`nil`). **Confiável** (padrão): chega com certeza e em ordem. Com `false`, não é reenviado se for perdido: para o que você envia a cada quadro (posições, o controle), em que o seguinte o substitui. |
| `netReceive() → { {from, data}, … }` | Tudo o que chegou desde a última vez, em ordem. |
| `netClose()` | Sai da partida (para o anfitrião: a fecha para todos). O Stop do editor também a fecha. |

## A permissão

Um projeto que cria, procura ou entra em partidas não acessa a rede até que o usuário permita. Ao abri-lo (ou ao dar
Play, se ele começou a usar a rede depois), o editor pergunta **uma única vez** por arquivo e lembra a resposta. Dá para
mudar depois no cartão **Arquivo**: *Acesso à rede*. Sem permissão, o jogo roda igual e `netAvailable()` retorna
`false, "no permission"`: mostre uma mensagem em vez da opção de jogar em rede.

## Um exemplo: a sala

```lua
local GAME = "mi_juego"

function start()
  if not netAvailable() then info("sin red") return end
  if property("anfitrion") then netHost(netDevice(), 2, GAME) else netFind(GAME) end
end

function update(dt)
  local state = netState()
  if state == "searching" then
    local p = netMatches()
    if p[1] then netJoin(p[1].address) end
  end
  if keyPressed("enter") then netSend({t = "listo"}) end
  for _, m in ipairs(netReceive()) do
    if m.data.t == "listo" then info("el jugador " .. m.from .. " esta listo") end
  end
end
```

## Dicas

- **Um único cálculo**: o mais simples, e o que funciona melhor entre aparelhos diferentes (um PC e um celular), é o
  anfitrião calcular o jogo e enviar aos outros o que eles têm que mostrar; os outros só lhe enviam o seu controle. É
  assim que faz a demo de Dragon Ball (`dbz2w3d/lua/pelea.lua`).
- O que é enviado a cada quadro, **não confiável**; o que acontece uma vez (um aviso, "começar"), **confiável**.
- **Quando a rede trava**: se numa luta `netSilence()` passar de alguns décimos de segundo, o outro parou de enviar
  (o wi-fi caiu, o celular foi para segundo plano). Pause o jogo e avise ("esperando por…", com o que falta para o
  `limite`): se ele voltar, continua de onde estava; se não, `netState()` ou `netPlayers()` dizem isso.
- Envie só o necessário: números e textos curtos. Uma mensagem não pode passar de 60000 bytes.
- No navegador ainda não há rede (ele não tem UDP). O Android pede a permissão `INTERNET` (o jogo compilado já a
  traz); o Symbian, a capacidade `NetworkServices` (o celular pergunta por qual ponto de acesso sair).
