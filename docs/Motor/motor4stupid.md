# TaskMotor — Come Funziona

**Priorità:** 2 (la più alta) · **Stack:** 8 KB · **Ciclo:** ogni ~1 ms

Il task più importante. Prende i comandi dalla coda e li esegue: camminate, pose, e movimenti diretti via cavo USB. La priorità più alta garantisce che i motori rispondano subito.

```mermaid
flowchart TD
    START([Inizio Ciclo Motori]) --> POP["Controlla la Casella\ndei Comandi Ricevuti"]
    POP -->|C'è un nuovo comando| SET_CMD["Memorizzalo come\n'Comando Attuale'"]
    POP -->|Casella vuota| DISPATCH

    SET_CMD --> DISPATCH{"C'è un Comando Attuale\nda eseguire?"}

    DISPATCH -->|No, sono fermo| SERIAL_CHK
    DISPATCH -->|Sì| CMD_SW{Qual è il comando?}

    CMD_SW -->|Avanti| WALK["Camminata\n(Ripeti ciclo passi, fermati\nse arriva altro comando)"]
    CMD_SW -->|Indietro| WALKB["Camminata all'indietro"]
    CMD_SW -->|Sinistra| TURNL["Rotazione a Sinistra"]
    CMD_SW -->|Destra| TURNR["Rotazione a Destra"]
    CMD_SW -->|Riposo| REST["Mettiti a terra\nRilassa i motori\nCancella comando attuale"]
    CMD_SW -->|In piedi| STAND["Alzati\nAttiva animazione inattiva\nCancella comando attuale"]
    CMD_SW -->|Saluta, Balla, Ecc.| POSE["Esegui Posa Specifica\nCambia espressione\nCancella comando attuale"]

    WALK --> PRESS_CHK["Controllo Rapido:\nÈ arrivato un nuovo comando\nmentre camminavo?"]
    WALKB --> PRESS_CHK
    TURNL --> PRESS_CHK
    TURNR --> PRESS_CHK
    PRESS_CHK -->|Sì, nuovo comando| SET_CMD
    PRESS_CHK -->|No, continua| DISPATCH

    POSE --> SERVOS["Applica angoli ai motori\n(Aggiungi calibrazione)\nAttendi prima del prossimo step"]
    SERVOS --> DISPATCH

    REST --> DISPATCH
    STAND --> DISPATCH

    DISPATCH --> SERIAL_CHK{"C'è un tecnico collegato\nvia cavo USB?"}
    SERIAL_CHK -->|No| DELAY
    SERIAL_CHK -->|Sì| READ_CHAR["Leggi cosa sta scrivendo\nil tecnico sul terminale"]
    READ_CHAR --> NEWLINE{"Ha premuto Invio?"}
    NEWLINE -->|No| SERIAL_CHK
    NEWLINE -->|Sì| CLI_PARSE{Analizza il comando USB}

    CLI_PARSE -->|Comando camminata| MOV_CMD["Forza comando camminata"]
    CLI_PARSE -->|Comando posa| POSE_CMD["Esegui Posa"]
    CLI_PARSE -->|Calibra 1 motore| ONE_SERVO["Muovi un singolo motore"]
    CLI_PARSE -->|Calibra tutti| ALL_SERVO["Muovi tutti i motori"]
    CLI_PARSE -->|Imposta correzione| SUBTRIM["Salva offset calibrazione"]
    CLI_PARSE -->|Salva calibrazione| ST_SAVE["Mostra codice da incollare"]
    CLI_PARSE -->|Azzera calibrazione| ST_RESET["Azzera offset"]

    MOV_CMD --> CLEAR["Pulisci terminale USB"]
    POSE_CMD --> CLEAR
    ONE_SERVO --> CLEAR
    ALL_SERVO --> CLEAR
    SUBTRIM --> CLEAR
    ST_SAVE --> CLEAR
    ST_RESET --> CLEAR
    CLEAR --> DELAY["Riposo brevissimo (1ms)\nper non bloccare la CPU"]
    DELAY --> START

    style WALK fill:#e63946,color:#fff
    style WALKB fill:#e63946,color:#fff
    style TURNL fill:#e63946,color:#fff
    style TURNR fill:#e63946,color:#fff
    style POSE fill:#2a2a3e,color:#ffd700
    style PRESS_CHK fill:#1a1a2e,color:#00d4ff
    style SERVOS fill:#1a1a2e,color:#3fb950
```

## Interruzione durante la camminata

Le camminate (avanti/indietro/sinistra/destra) si possono interrompere in qualsiasi momento. Ogni 5 ms durante il ciclo di passo, `pressingCheck()` controlla se è arrivato un nuovo comando. Se sì:

1. Il nuovo comando diventa quello attuale
2. Il robot torna in posizione sicura (`runStandPose`)
3. La camminata si interrompe subito

## Come viene mosso un servo

```
angolo_richiesto
  + servoSubtrim[canale]          ← offset di calibrazione int8_t [-90, +90]
  = constrain(risultato, 0, 180)
  → servos[canale].write()
  → vTaskDelay(motorCurrentDelay) ← ritardo per distribuire il picco di corrente
```

## Comandi continui vs. pose singole

| Tipo | Comandi | Comportamento |
| --- | --- | --- |
| **Continui** | avanti, indietro, sinistra, destra | Il comando rimane attivo e si riavvia ogni ciclo finché non viene interrotto |
| **Pose singole** | rest, stand, wave, dance, ecc. | La funzione cancella il comando attuale quando ha finito |

## Diagrammi correlati

- [Panoramica Sistema](../Architecture/architecture4stupid.md)
- [TaskWeb — Come Funziona](../Web/web4stupid.md)
- [TaskDisplay — Come Funziona](../Display/display4stupid.md)
