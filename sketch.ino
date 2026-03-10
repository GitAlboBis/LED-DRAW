#include <avr/io.h>
#include <avr/interrupt.h>

#define LED_PIN      6
#define SELECT_PIN   2
#define JOY_X_PIN    1
#define JOY_Y_PIN    0

#define MATRIX_COLS  4
#define MATRIX_ROWS  8
#define NUM_LEDS     (MATRIX_COLS * MATRIX_ROWS)

const int thresholdHigh = 600;
const int thresholdLow  = 400;

const unsigned long blinkInterval = 125;
const unsigned long initialMoveDelay = 1000;
const unsigned long repeatMoveDelay  = 500;
const unsigned long buttonHoldThreshold = 1000;

volatile uint8_t displayBuffer[NUM_LEDS * 3];
uint8_t canvas[NUM_LEDS * 3];

int pencilX = 0, pencilY = 0;
bool blinkState = false;
unsigned long lastBlinkTime = 0;

uint8_t pencilR = 255, pencilG = 0, pencilB = 0;

unsigned long lastMoveTime = 0;
unsigned long moveHoldStart = 0;

bool lastButtonState = HIGH;
unsigned long buttonPressTime = 0;

bool setupMode = false;

// Definisco Macro per controllo PIN
#define scritturaPin(port, pins, value) if(value == HIGH)((port) |= (pins)); else ((port) &= ~(pins))
#define letturaPin(port, pin) ((port & (1 << pin)) != 0)

// imposto ADC per lettura analogica
void setupADC() {
    ADMUX = (1 << REFS0);    
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); 
}

// Setup dei PIN
void setupPins() {
    DDRD |= (1 << LED_PIN);
    DDRD &= ~(1 << SELECT_PIN);
    PORTD |= (1 << SELECT_PIN);

    DDRC &= ~((1 << JOY_X_PIN) | (1 << JOY_Y_PIN));
    PORTC |= (1 << JOY_X_PIN) | (1 << JOY_Y_PIN);
}

// Lettura analogica diretta
uint16_t readAnalog(uint8_t pin) {
    ADMUX = (1 << REFS0) | pin;  
    ADCSRA |= (1 << ADSC);       
    while (ADCSRA & (1 << ADSC)); 
    return ADC;
}

int getIndex(int x, int y) {
    return y * MATRIX_COLS + x;
}

// Aggiornamento del buffer display
void updateDisplayBuffer() {
    for (int i = 0; i < NUM_LEDS * 3; i++) {
        displayBuffer[i] = canvas[i];
    }
    int idx = getIndex(pencilX, pencilY);
    if (blinkState) {
        displayBuffer[idx * 3 + 0] = pencilG;
        displayBuffer[idx * 3 + 1] = pencilR;
        displayBuffer[idx * 3 + 2] = pencilB;
    }
}


void setupTimer2() {
    TCCR2A = 0;  // Modalità normale
    TCCR2B = (1 << CS22) | (1 << CS21); // Prescaler 256 (overflow ogni ~16ms)
    TIMSK2 = (1 << TOIE2);  // Abilita interrupt di overflow
}
ISR(TIMER2_OVF_vect) {
    static uint8_t blinkCount = 0;
    static uint8_t moveCount = 0;
    static uint16_t buttonPressCount = 0;
    static bool buttonPressed = false;

    // Lampeggio a 4Hz (ogni 61 overflow ≈ 250ms)
    blinkCount++;
    if (blinkCount >= 61) {  
        blinkState = !blinkState;
        updateDisplayBuffer();
        blinkCount = 0;
    }

    // Movimento cursore ogni 125ms (31 overflow)
    moveCount++;
    if (moveCount >= 31) {  
        checkJoystickMovement();
        moveCount = 0;
    }

    // Controllo pressione pulsante per setupMode
    if (!letturaPin(PIND, SELECT_PIN)) {  // Se il pulsante è premuto
        buttonPressCount++;  // Incrementa il tempo di pressione
        buttonPressed = true;
    } else {  
        if (buttonPressed) {  // Se il pulsante viene rilasciato
            if (buttonPressCount >= (1000 / 4.096)) {  // Pressione lunga (>1s)
                setupMode = !setupMode;  // Cambia modalità
            } else {  // Pressione breve
                if (!setupMode) {
                    writePixel();
                } else {
                    setupMode = false;
                }
            }
            buttonPressCount = 0;
            buttonPressed = false;
        }
    }
    if (setupMode) {
        int joyX = readAnalog(JOY_X_PIN);
        int joyY = readAnalog(JOY_Y_PIN);
        adjustPencilColor(joyX, joyY);  // Cambia colore del pennello
    }
}


void checkJoystickMovement() {
    // Solo se non siamo in setupMode, il movimento è attivo
    if (setupMode) {
        return;  // Ignora il movimento del joystick in modalità setupMode
    }

    int joyX = readAnalog(JOY_X_PIN);
    int joyY = readAnalog(JOY_Y_PIN);

    int dx = (joyX > thresholdHigh) - (joyX < thresholdLow);
    int dy = (joyY > thresholdHigh) - (joyY < thresholdLow);

    if (dx || dy) {
        pencilX = pencilX + dx;
        pencilY = pencilY + dy;
    }
}


#define PORT_LED PORTD
#define DDR_LED DDRD
#define NBIT 6  // Pin di output su PD6

void showStrip(uint8_t nb, uint8_t *vp) {
    DDR_LED |= (1 << NBIT); // Imposta il pin come output
    PORT_LED &= ~(1 << NBIT); // Assicura che il pin sia LOW all'inizio
    cli(); // Disabilita gli interrupt per evitare interferenze

    asm volatile (
        "lwhile0:\n\t"  // Ciclo esterno per caricamento byte
        "ld r24, %a[vp]+ \n\t"  // (c2) Carico prossimo byte
        "ldi r25, 8 \n\t"  // (c1) e contatore bit

        "lwhile:\n\t"  // Ciclo interno analisi bit
        "sbi %[port],%[nbit] \n\t"  // (c1) Imposta a 1 il pin di port bit;
        "nop\n\t nop\n\t nop\n\t nop\n\t"  // [attendi 4 cicli] (per arrivare a 7 cicli)
        "sbrs r24, 7 \n\t"  // (c1) Test bit 7, skip se 1
        "cbi %[port],%[nbit] \n\t"  // (c1) Imposta a 0 il pin di port bit;
        "add r24, r24 \n\t"  // (c1) Shift a sinistra un bit
        "subi r25, 1 \n\t"  // (c1) Decremento contatore bit
        "breq vnextb \n\t"  // (c1) Ciclo esterno Controllo se finito
        "nop\n\t nop\n\t"  // [attendi 2 cicli] (per arrivare a 13 cicli)
        "cbi %[port],%[nbit] \n\t"  // (c1) Imposta a 0 il pin di port bit;
        "nop\n\t nop\n\t nop\n\t nop\n\t"  // [attendi 4 cicli] (per arrivare a 20 cicli)
        "rjmp lwhile \n\t"  // (c2) Torna al ciclo interno

        "vnextb:\n\t"  // Arrivo da 12 cicli
        "nop\n\t"  // [attendo 1 ciclo] (Per arrivare a 13)
        "cbi %[port],%[nbit] \n\t"  // (c1) Imposta a 0 il pin di port bit;
        "subi %[nb], 1 \n\t"  // (c1) Decremento contatore byte
        "brne lwhile0 \n\t"  // (c2) Continua il ciclo se ci sono byte rimanenti

        : [vp] "+e" (vp)  // Output list
        : [nb] "d" (nb), 
          [port] "i" (_SFR_IO_ADDR(PORT_LED)), 
          [nbit] "i" (NBIT)
        : "r24", "r25"
    );

    sei(); // Riabilita gli interrupt dopo la trasmissione
     DDR_LED &= ~(1 << NBIT); // Ritorna a input per sicurezza
}


// Scrittura di un pixel sulla matrice
void writePixel() { 
    int idx = getIndex(pencilX, pencilY); 
    canvas[idx * 3 + 0] = pencilG; 
    canvas[idx * 3 + 1] = pencilR; 
    canvas[idx * 3 + 2] = pencilB; 
} 

// Pulizia della matrice
void clearCanvas() {
    for (int i = 0; i < NUM_LEDS * 3; i++) {
        canvas[i] = 0;
    }
}
void handleButtonPress(uint16_t pressDuration) {
    if (pressDuration < (1000 / 4.096)) {  // Pressione breve (<1000ms)
        if (!setupMode) {
            writePixel();
        } else {
            setupMode = false;
        }
    } else {  // Pressione lunga (>1000ms)
        setupMode = !setupMode;
    }
}


// Gestione del colore del pennello in modalità setup
void adjustPencilColor(int joyX, int joyY) {
  // Logica per l'aumento e la diminuzione del rosso (Nord-Ovest / Sud-Est)
  if (joyX < thresholdLow && joyY > thresholdHigh) { // Nord-Ovest (Rosso aumenta)
    pencilR = (pencilR + 5 > 255) ? 255 : pencilR + 5;
  } else if (joyX > thresholdHigh && joyY < thresholdLow) { // Sud-Est (Rosso diminuisce)
    pencilR = (pencilR < 5) ? 0 : pencilR - 5;
  }
  
  // Logica per l'aumento e la diminuzione del verde (Nord / Sud)
  if (joyY > thresholdHigh) { // Nord (Verde aumenta)
    pencilG = (pencilG + 5 > 255) ? 255 : pencilG + 5;
  } else if (joyY < thresholdLow) { // Sud (Verde diminuisce)
    pencilG = (pencilG < 5) ? 0 : pencilG - 5;
  }
  
  // Logica per l'aumento e la diminuzione del blu (Nord-Est / Sud-Ovest)
  if (joyX > thresholdHigh && joyY > thresholdHigh) { // Nord-Est (Blu aumenta)
    pencilB = (pencilB + 5 > 255) ? 255 : pencilB + 5;
  } else if (joyX < thresholdLow && joyY < thresholdLow) { // Sud-Ovest (Blu diminuisce)
    pencilB = (pencilB < 5) ? 0 : pencilB - 5;
  }
}

void setup() {
    setupPins();
    setupADC();
    setupTimer2();  
    clearCanvas();
    pencilX = 0;
    pencilY = 0;
    lastMoveTime = 0;
    moveHoldStart = 0;
    lastButtonState = letturaPin(PIND, SELECT_PIN);
}

void loop() {
    // Solo in modalità non setupMode, il cursore si muove
    if (!setupMode) {
        int joyX = readAnalog(JOY_X_PIN);
        int joyY = readAnalog(JOY_Y_PIN);

        int dx = (joyX > thresholdHigh) - (joyX < thresholdLow);
        int dy = (joyY > thresholdHigh) - (joyY < thresholdLow);

        if ((dx || dy) && millis() - lastMoveTime >= repeatMoveDelay) {
            pencilX = pencilX + dx;
            pencilY = pencilY + dy;
            lastMoveTime = millis();
        }
    } else {
        // Quando siamo in setupMode, cambia solo il colore senza muovere il cursore
        int joyX = readAnalog(JOY_X_PIN);
        int joyY = readAnalog(JOY_Y_PIN);
        adjustPencilColor(joyX, joyY);  // Modifica il colore senza muovere il cursore
    }

    updateDisplayBuffer();
    showStrip(NUM_LEDS * 3, displayBuffer);
    delay(10);  // Facoltativo per stabilità
}
