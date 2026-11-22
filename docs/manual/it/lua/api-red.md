# Giocare in rete (LAN, Wi-Fi o Internet)

Una partita in rete ha un **host** (chi la crea: giocatore 0) e quelli che si **uniscono** (1, 2…). Tutto passa per
l'host: un giocatore invia all'host e l'host distribuisce. Chi è sulla stessa rete (la stessa LAN o lo stesso Wi-Fi)
si trova da solo; via Internet bisogna unirsi con l'indirizzo dell'host (e lui deve avere la porta 47023 aperta sul
router).

| Funzione | Cosa fa |
|---|---|
| `netAvailable() → bool [, reason]` | Se questo gioco può usare la rete. Se no, il motivo: `"no permission"` (l'editor non ha dato il permesso a questo progetto) o `"no network"` (questa piattaforma non ce l'ha ancora). |
| `netDevice() → text` | Il nome di questo dispositivo (quello che vedono gli altri quando cercano partite). Si cambia nell'editor: riquadro **Impostazioni**, *Nome del dispositivo*. |
| `netAddress() → text` | L'IP di questo dispositivo nella rete locale (per unirsi via Internet o scrivendolo a mano). |
| `netHost(name [, max [, game]]) → bool [, error]` | Crea una partita e aspetta i giocatori (`max` contando l'host: 2 per impostazione predefinita). `game` è il nome del tuo gioco: si vedono solo le partite dello stesso gioco. |
| `netOpen(bool)` | L'host compare nelle ricerche e accetta giocatori (`true`) oppure no (per esempio, quando la partita è iniziata). |
| `netFind([game]) → bool` | Inizia a cercare partite nella rete locale. Richiamarla **aggiorna** l'elenco. |
| `netMatches() → { {name, device, players, max, address}, … }` | Le partite che hanno risposto (l'elenco si aggiorna da solo mentre cerchi). |
| `netJoin(address) → bool` | Entra in una partita: la `address` di `netMatches()` o una scritta a mano (`"192.168.0.10"`, `"x.x.x.x:puerto"`). |
| `netState() → state, error` | `"none"`, `"searching"`, `"waiting"` (sei l'host), `"connecting"`, `"connected"` o `"error"` (con il motivo). |
| `netSelf() → id` | Il tuo numero nella partita (0 = l'host; -1 senza partita). |
| `netPlayers() → { {id, name, isSelf}, … }` | Chi è nella partita (il nome è quello del suo dispositivo). |
| `netSilence([id]) → seconds, limite` | Da quanto non arriva niente da quel giocatore (senza `id`: da quello che tace da più tempo). Chi si unisce sente solo l'host, quindi per lui è il silenzio dell'host. Superato il `limite` (8 s) lo si considera uscito: l'host lo toglie dalla partita e, a chi si è unito, `netState()` restituisce `"error"`. |
| `netSend(value [, para [, reliable]]) → bool` | Invia un valore Lua (numero, testo, booleano o tabella, anche con tabelle dentro) a un giocatore (`para` = il suo id) o a tutti (`nil`). **Affidabile** (predefinito): arriva di sicuro e in ordine. Con `false` non viene reinviato se si perde: per ciò che invii a ogni fotogramma (posizioni, il controller), dove il successivo lo sostituisce. |
| `netReceive() → { {from, data}, … }` | Tutto ciò che è arrivato dall'ultima volta, in ordine. |
| `netClose()` | Esce dalla partita (se sei l'host, la chiude per tutti). Anche lo Stop dell'editor la chiude. |

## Il permesso

Un progetto che crea, cerca o si unisce a partite non esce in rete finché l'utente non lo consente. Quando lo apri (o
quando premi Play, se ha iniziato a usare la rete dopo) l'editor chiede **una sola volta** per file e ricorda la
risposta. Si cambia poi nel riquadro **File**: *Accesso alla rete*. Senza permesso, il gioco gira lo stesso e
`netAvailable()` restituisce `false, "no permission"`: mostra un messaggio al posto dell'opzione per giocare in rete.

## Un esempio: la lobby

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

## Consigli

- **Un solo calcolo**: la cosa più semplice, e quella che funziona meglio tra dispositivi diversi (un PC e un
  telefono), è che l'host calcoli il gioco e mandi agli altri ciò che devono mostrare; gli altri gli mandano solo il
  loro controller. Così fa la demo di Dragon Ball (`dbz2w3d/lua/pelea.lua`).
- Ciò che si invia a ogni fotogramma, **non affidabile**; ciò che succede una volta sola (un avviso, "inizia"),
  **affidabile**.
- **Quando la rete si blocca**: se in un combattimento `netSilence()` supera qualche decimo di secondo, l'altro ha
  smesso di inviare (è caduto il Wi-Fi, il telefono è passato in background). Metti in pausa il gioco e avvisa ("in
  attesa di…", con il tempo che manca al `limite`): se torna, riprende da dove era; se no, `netState()` o
  `netPlayers()` lo dicono.
- Invia lo stretto necessario: numeri e testi brevi. Un messaggio non può superare i 60000 byte.
- Nel browser la rete non c'è ancora (non ha UDP). Android richiede il permesso `INTERNET` (il gioco compilato lo
  include); Symbian, la capability `NetworkServices` (il telefono chiede da quale punto di accesso uscire).
