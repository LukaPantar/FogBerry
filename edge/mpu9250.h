#ifndef MPU9250_H
#define MPU9250_H

#include <stdint.h>
#include <hardware/spi.h>

typedef struct MPU9250Desc {
    uint16_t pinMISO;
    uint16_t pinMOSI;
    uint16_t pinSCK;
    uint16_t pinCS;

    spi_inst_t *spiPort;
} MPU9250Desc;

typedef struct MPU9250 {
    int16_t accel[3];
    int16_t gyro[3];
    int16_t eulerAngles[2];
    int16_t fullAngles[2];
    int16_t mag[3];


    int16_t _gyroCal[3];
    int32_t usSinceLastRead;
    MPU9250Desc _desc;
} MPU9250;

MPU9250 mpu9250Init(const MPU9250Desc *desc);

bool mpu9250Start(MPU9250 *mpu);
void mpu9250Reset(MPU9250 *mpu);

void mpu9250CalibrateGyro(MPU9250 *mpu, int16_t loop);

void mpu9250Read(MPU9250 *mpu);

#endif //MPU9250_H
