#ifndef APP_TASKS_H
#define APP_TASKS_H

// Application-level FreeRTOS tasks. Kept separate from serial_interfaces.c
// (the actual I2C1 driver) -- these functions are policy (when to scan,
// when to fade, what to do with results), not mechanism.

void vI2CTask(void *pvParameters);
void vPWMTimerTask(void *pvParameters);
void vOneWireTask(void *pvParameters);
void vThermistorTask(void *pvParameters);
void vFlameAlarmTask(void *pvParameters);
void vLaserTripTask(void *pvParameters);

#endif // APP_TASKS_H
