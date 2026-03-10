# Joystick Draw (LED-DRAW)

## Descrizione del Progetto
**Joystick Draw** è un progetto pratico sviluppato per il "Corso di Architettura degli Elaboratori - Modulo Laboratorio". L'applicativo consente agli utenti di interagire con una matrice LED per "disegnare" interattivamente sfruttando un joystick analogico.

## Architettura e Hardware
Il sistema core è basato sull'architettura di un microcontrollore **Arduino Nano**, che elabora i segnali del joystick e aggiorna in tempo reale la visualizzazione visiva. 

Il circuito utilizza le seguenti specifiche:
- **Matrice LED**: Configurazione a 4 colonne (`MATRIX_COLS 4`) e 8 righe (`MATRIX_ROWS 8`), che compone un display di manovra da 32 LED (`NUM_LEDS`).
- **Joystick Analogico**: Gestisce il tracciamento del movimento bi-dimensionale (X, Y) e supporta l'input di un pulsante di selezione.

### Mappatura dei Pin
L'interfacciamento hardware (Pinout) è configurato a basso livello nel firmware nel seguente modo:
- **Pin 6**: Output della Matrice LED (`LED_PIN`)
- **Pin 2**: Pulsante di Select del Joystick (`SELECT_PIN`)
- **Pin 1**: Asse X del Joystick (`JOY_X_PIN`)
- **Pin 0**: Asse Y del Joystick (`JOY_Y_PIN`)

## Stack Software e Logica
- **Linguaggio**: Il firmware è sviluppato in C/C++.
- **nessuna libreria**: Il codice include nativamente `<avr/io.h>` e `<avr/interrupt.h>`, indicando l'utilizzo di interrupt hardware e manipolazione diretta dei registri AVR per un controllo performante e preciso dell'I/O, tramite TIMER 2.
- **Simulazione**: Il progetto è pronto per la validazione digitale tramite la piattaforma **Wokwi**, includendo la configurazione del layout e l'infrastruttura di simulazione all'interno del file di topologia.

## Setup e Installazione
1. **Configurazione Hardware**: Cablare l'Arduino Nano, il Joystick e la matrice LED seguendo rigorosamente la mappatura dei pin descritta sopra.
2. **Ambiente di Simulazione**: Per testare il codice virtualmente, aprire il progetto su Wokwi importando il file `diagram.json`.
3. **Compilazione**: Importare `sketch.ino` nel proprio IDE di riferimento (es. Arduino IDE) e flashare il firmware direttamente sul microcontrollore AVR.
