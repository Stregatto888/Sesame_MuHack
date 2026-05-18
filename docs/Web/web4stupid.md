# TaskWeb — Come Funziona

**Priorità:** 1 · **Stack:** 8 KB · **Ciclo:** ogni ~1 ms

Gestisce il server DNS del portale captive e il server HTTP. Tutte le 8 rotte passano da qui. Il meccanismo di blocco "hack-lock" vive interamente in questo task.

```mermaid
flowchart TD
    START([Inizio Ciclo Web]) --> PUMP["Ascolta Rete\n(Attende connessioni)"]

    PUMP --> DNS_PROC["Gestisci richieste DNS\n(Ignora se non ce ne sono)"]
    DNS_PROC --> HTTP_PROC["Gestisci richieste HTTP\n(Prende in carico un utente)"]
    HTTP_PROC --> ROUTE{Cosa vuole\nl'utente?}

    ROUTE -->|Apre il sito| ROOT["Invia la pagina Web\n(Interfaccia grafica)"]
    ROUTE -->|Pulsante movimento| CMD["Comando Semplice\n(Es. Cammina, Fermati)"]
    ROUTE -->|Terminale Web| TERM["Comando Avanzato\n(Testo dal terminale web)"]
    ROUTE -->|Integrazione esterna| APICMD["Comando API\n(Dati strutturati JSON)"]
    ROUTE -->|Richiesta stato| STATUS["Invia Stato Attuale\n(Cosa sto facendo? IP?)"]
    ROUTE -->|Richiesta parametri| GETS["Invia Impostazioni\n(Velocità attuali)"]
    ROUTE -->|Salvataggio parametri| SETS["Aggiorna Impostazioni\n(Applica nuove velocità)"]
    ROUTE -->|Pagina inesistente| ROOT

    TERM --> HACK_CHECK{Che comando è?}
    HACK_CHECK -->|Voglio il controllo| LOCK["Blocca il robot\nAssocia al tuo IP\n→ Successo"]
    HACK_CHECK -->|Rilascia il controllo| UNLOCK{"Sei il proprietario?"}
    UNLOCK -->|Sì| FREE["Sblocca il robot per tutti\n→ Successo"]
    UNLOCK -->|No| DENY2["→ Errore: Non hai i permessi"]
    HACK_CHECK -->|Info di sistema| STAT_RESP["Rispondi con Info\n(IP, Connessioni, Stato Lock)"]
    HACK_CHECK -->|Voglio muoverti| BLOCKED{"Il robot è bloccato\nda un altro utente?"}
    BLOCKED -->|Sì| DENY["→ Errore: Accesso Negato"]
    BLOCKED -->|No| ENQUEUE["Accetta il comando\nSveglia lo schermo\nPassa la palla ai Motori"]

    APICMD --> PARSE["Analizza il testo JSON"]
    PARSE -->|Solo espressione| FACE_SET["Cambia faccia\nSveglia lo schermo"]
    PARSE -->|Espressione + Movimento| CMD_PUSH["Accetta il comando\nOpzionale: Cambia faccia"]

    SETS --> WRITE["Salva nuovi parametri in memoria\n(Usa variabili protette)"]

    ENQUEUE --> DELAY["Riposo brevissimo (1ms)\nper non bloccare la CPU"]
    ROOT --> DELAY
    DELAY --> START

    style LOCK fill:#e63946,color:#fff
    style FREE fill:#3fb950,color:#000
    style DENY fill:#e63946,color:#fff
    style DENY2 fill:#e63946,color:#fff
    style ENQUEUE fill:#2a2a3e,color:#ffd700
    style CMD_PUSH fill:#2a2a3e,color:#ffd700
```

## Macchina a stati del blocco (hack-lock)

Chiunque si connette alla rete WiFi del robot può controllarlo.
Il comando `hack` permette a un singolo utente di "prendersi" il controllo esclusivo:

| Stato | Come si attiva | Effetto |
| --- | --- | --- |
| **Sbloccato** | — (stato di partenza) | Qualsiasi client può inviare comandi |
| **Bloccato** | `hack` da qualsiasi client | Solo l'IP che ha lanciato `hack` può comandare |
| **Rilasciato** | `muhack` dall'IP proprietario | Torna sbloccato per tutti |
| — | `muhack` da un IP non proprietario | `ERRORE: solo l'hacker può sbloccare` — rimane bloccato |

## Diagrammi correlati

- [Panoramica Sistema](../Architecture/architecture4stupid.md)
- [TaskDisplay — Come Funziona](../Display/display4stupid.md)
- [TaskMotor — Come Funziona](../Motor/motor4stupid.md)
