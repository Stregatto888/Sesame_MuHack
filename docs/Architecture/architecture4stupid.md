# Architettura — Panoramica del Sistema

Tutti e tre i task FreeRTOS, la coda dei comandi, i dispositivi fisici, le interfacce utente e i dati condivisi con protezione thread.

```mermaid
graph TB
subgraph HW["HARDWARE FISICO"]
OLED["Schermo OLED\n(Il volto del robot)"]
SERVOS["8 Servomotori\n(Le gambe del robot)"]
WIFI["Modulo WiFi\n(Crea la rete del robot)"]
end

    subgraph CLIENTS["INTERFACCE UTENTE"]
        BROWSER["Smartphone / PC\n(App Web)"]
        CURL["Script / API\n(Integrazioni esterne)"]
        SERIAL["Terminale Seriale\n(Cavo USB)"]
    end

    subgraph RTOS["SISTEMA MULTITASKING (FreeRTOS)"]
        direction TB

        subgraph TW["1. Cervello di Rete (TaskWeb)"]
            DNS["Trappola DNS\n(Forza l'apertura dell'App Web)"]
            HTTP["Server Web\n(Gestisce le richieste utente)"]
            LOCK["Gestore Sicurezza\n(Blocca/Sblocca il controllo)"]
        end

        subgraph TD["2. Cervello Visivo (TaskDisplay)"]
            TICK["Animatore Volto\n(Scorre i frame)"]
            IDLE["Gestore Inattività\n(Sbatte le palpebre)"]
            MARQ["Gestore Banner\n(Mostra info WiFi se inattivo)"]
        end

        CQ["Coda dei Comandi\n(Disaccoppia la Rete dai Motori)"]

        subgraph TM["3. Cervello Motorio (TaskMotor)"]
            DISP["Smistatore Comandi\n(Decide cosa fare)"]
            POSES["Libreria Movimenti\n(Pose e Camminate)"]
            CLI["Terminale Diagnostico\n(Ascolta il cavo USB)"]
        end
    end

    subgraph SHARED["DATI CONDIVISI (Sicuri per accesso multiplo)"]
        FN["Nome Faccia Attuale\n(Protetto da lettura/scrittura a metà)"]
        FFPS["Velocità Animazione\n(Protetta da modifiche simultanee)"]
        ATOM["Parametri di Movimento\n(Velocità, Cicli, Ritardi)"]
    end

    BROWSER -->|Richieste Web| HTTP
    CURL -->|Richieste Web| HTTP
    SERIAL -->|Testo| CLI

    DNS -->|Reindirizza| WIFI
    HTTP -->|Invia Comando| CQ
    CQ -->|Legge Comando| DISP
    DISP --> POSES
    POSES -->|Muove Fisicamente| SERVOS
    POSES -->|Cambia Espressione| FN

    HTTP -->|Legge per mostrare stato| FN
    HTTP -->|Legge/Modifica| FFPS
    HTTP -->|Legge/Modifica| ATOM
    TM -->|Legge per muoversi| ATOM

    TICK -->|Disegna pixel| OLED
    TICK -->|Legge espressione| FN
    TICK -->|Legge velocità| FFPS
    IDLE -->|Forza battito ciglia| OLED
    MARQ -->|Scorre testo| OLED

    style TW fill:#1a1a2e,color:#00d4ff
    style TD fill:#1a1a2e,color:#00d4ff
    style TM fill:#1a1a2e,color:#e63946
    style CQ fill:#2a2a3e,color:#ffd700
    style SHARED fill:#0d1117,color:#3fb950
```

## Sicurezza degli accessi condivisi

Tre task diversi girano in parallelo e leggono/scrivono gli stessi dati.
Per non corrompere le informazioni, ogni variabile condivisa è protetta da un meccanismo diverso:

| Variabile condivisa | Chi scrive | Chi legge | Protezione usata |
| --- | --- | --- | --- |
| Nome faccia attuale (`String`) | TaskMotor (`Display::set`) | TaskWeb (stato/terminale) | Mutex (`SemaphoreHandle_t`) |
| Velocità animazione (`int`) | TaskWeb (`setSettings`) | TaskDisplay (`tickFace`) | Spinlock (`portMUX_TYPE`) |
| `frameDelay`, `walkCycles`, `motorCurrentDelay` (`int`) | TaskWeb (`setSettings`) | TaskMotor (pose, servo) | Variabile atomica (`std::atomic<int>`) |
| Coda comandi (`CmdQueue`) | TaskWeb (handler HTTP) | TaskMotor (dispatcher) | Coda FreeRTOS (sicura da interrupt) |

## Diagrammi correlati

- [TaskWeb — Come Funziona](../Web/web4stupid.md)
- [TaskDisplay — Come Funziona](../Display/display4stupid.md)
- [TaskMotor — Come Funziona](../Motor/motor4stupid.md)
