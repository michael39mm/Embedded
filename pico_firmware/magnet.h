#ifndef MAGNET_H
#define MAGNET_H

/* --- Electromagnet Relay Signal Pin (GP13 = Physical Pin 17, Active-LOW Open-Drain) --- */
/* Relay module power: VCC -> Pin 40 (VBUS), GND -> Pin 18 (GND), IN -> GP13 (Pin 17)     */
#define PIN_RELAY         13
#define MAGNET_SETTLE_MS  250   /* Pause at pickup/drop so piece locks and releases cleanly */

void magnet_init(void);
void magnet_on(void);
void magnet_off(void);

#endif /* MAGNET_H */