#include "mpu9250.h"

#include <math.h>
#include <stdio.h>
#include <hardware/gpio.h>
#include <pico/stdlib.h>
#include <hardware/spi.h>
#include <pico/time.h>

static inline void cs_set(const MPU9250* mpu, int state) {
    __asm volatile("nop \n nop \n nop");
    gpio_put(mpu->_desc.pinCS, state);
    __asm volatile("nop \n nop \n nop");
}

static inline void cs_select(const MPU9250* mpu) {
    cs_set(mpu, 0);
}

static inline void cs_deselect(const MPU9250* mpu) {
    cs_set(mpu, 1);
}

#define READ_BIT 0x80
static inline void readRegister(const MPU9250 *mpu, uint8_t reg, uint8_t *out, uint16_t len) {
    const MPU9250Desc *desc = &mpu->_desc;
    reg |= READ_BIT;
    cs_select(mpu);
    spi_write_blocking(desc->spiPort, &reg, 1);
    sleep_ms(10);
    spi_read_blocking(desc->spiPort, 0, out, len);
    cs_deselect(mpu);
    sleep_ms(10);
}
#undef READ_BIT



MPU9250 mpu9250Init(const MPU9250Desc *desc) {
    MPU9250 ret = {
        ._desc = *desc,
    };
    return ret;
}

bool mpu9250Start(MPU9250 *mpu) {
    const MPU9250Desc *desc = &mpu->_desc;
    spi_init(desc->spiPort, 500 * 1000);
    gpio_set_function(desc->pinMISO, GPIO_FUNC_SPI);
    gpio_set_function(desc->pinMOSI, GPIO_FUNC_SPI);
    gpio_set_function(desc->pinSCK, GPIO_FUNC_SPI);

    // Chip select is active-low
    gpio_init(desc->pinCS);
    gpio_set_dir(desc->pinCS, GPIO_OUT);
    gpio_put(desc->pinCS, 1);

    mpu9250Reset(mpu);

    // Check if SPI is working (get ID number)
    uint8_t id;
    readRegister(mpu, 0x75, &id, 1);
    printf("MPU9250 ID: %02x\n", id);
    if (id != 0x70 && id != 0x71) {
        // 0x70 -> MPU6500
        // 0x71 -> MPU9250
        return false;
    }

    return true;
}
void mpu9250Reset(MPU9250 *mpu) {
    // Two byte reset {register, data}
    uint8_t payload[2] = {0x6B, 0x00};
    cs_set(mpu, 0);
    spi_write_blocking(mpu->_desc.spiPort, payload, 2);
    cs_set(mpu, 1);
}

static inline void readI16Data(const MPU9250 *mpu, uint8_t reg, int16_t *data, int16_t len) {
    const int16_t byteSize = len * 2;
    uint8_t buffer[byteSize];
    readRegister(mpu, reg, buffer, byteSize);
    for (int i = 0; i < len; i++) {
        data[i] = (int16_t) (buffer[i * 2] << 8 | buffer[(i * 2) + i]);
    }
}

void mpu9250CalibrateGyro(MPU9250 *mpu, int16_t loop) {
    int16_t raw[3];
    int16_t *gyroCal = mpu->_gyroCal;
    for (int i = 0; i < loop; i++) {
        readI16Data(mpu, 0x43, raw, 3);
        gyroCal[0] += raw[0];
        gyroCal[1] += raw[1];
        gyroCal[2] += raw[2];
    }
    gyroCal[0] /= loop;
    gyroCal[1] /= loop;
    gyroCal[2] /= loop;
}

static inline void calculateAnglesFromAcc(int16_t eulerAngles[2], int16_t accel[3]) {
    float accTotalVector = sqrt((accel[0] * accel[0]) + (accel[1] * accel[1]) + (accel[2] * accel[2]));

    float anglePitchAcc = asin(accel[1] / accTotalVector) * 57.296;
    float angleRollAcc = asin(accel[0] / accTotalVector) * -57.296;

    eulerAngles[0] = anglePitchAcc;
    eulerAngles[1] = angleRollAcc;
}

void mpu9250Read(MPU9250 *mpu) {
    readI16Data(mpu, 0x3B, mpu->accel, 3);
    readI16Data(mpu, 0x43, mpu->gyro, 3);
    // magnetometer
    mpu->gyro[0] -= mpu->_gyroCal[0];
    mpu->gyro[1] -= mpu->_gyroCal[1];
    mpu->gyro[2] -= mpu->_gyroCal[2];

    // Calculate angles
    uint64_t hertz = 1000000/ absolute_time_diff_us(mpu->usSinceLastRead, get_absolute_time());
    mpu->usSinceLastRead = get_absolute_time();
    if (hertz < 200) {
        calculateAnglesFromAcc(mpu->eulerAngles, mpu->accel);
    } else {
        uint64_t temp = 1.1 / (hertz * 65.51);

        mpu->eulerAngles[0] += mpu->gyro[0] * temp;
        mpu->eulerAngles[1] += mpu->gyro[1] * temp;

        mpu->eulerAngles[0] += mpu->eulerAngles[1] * sin(mpu->gyro[2] * temp * 0.1f);
        mpu->eulerAngles[1] -= mpu->eulerAngles[0] * sin(mpu->gyro[2] * temp * 0.1f);

        int16_t accelEuler[2];
        calculateAnglesFromAcc(accelEuler, mpu->accel);

        mpu->eulerAngles[0] = mpu->eulerAngles[0] * 0.9996 + accelEuler[0] * 0.0004;
        mpu->eulerAngles[1] = mpu->eulerAngles[1] * 0.9996 + accelEuler[1] * 0.0004;
    }

    // Convert to full

    if (mpu->accel[1] > 0 && mpu->accel[2] > 0) mpu->fullAngles[0] = mpu->eulerAngles[0];
    if (mpu->accel[1] > 0 && mpu->accel[2] < 0) mpu->fullAngles[0] = 180 - mpu->eulerAngles[0];
    if (mpu->accel[1] < 0 && mpu->accel[2] < 0) mpu->fullAngles[0] = 180 - mpu->eulerAngles[0];
    if (mpu->accel[1] < 0 && mpu->accel[2] > 0) mpu->fullAngles[0] = 360 + mpu->eulerAngles[0];

    if (mpu->accel[0] < 0 && mpu->accel[2] > 0) mpu->fullAngles[1] = mpu->eulerAngles[1];
    if (mpu->accel[0] < 0 && mpu->accel[2] < 0) mpu->fullAngles[1] = 180 - mpu->eulerAngles[1];
    if (mpu->accel[0] > 0 && mpu->accel[2] < 0) mpu->fullAngles[1] = 180 - mpu->eulerAngles[1];
    if (mpu->accel[0] > 0 && mpu->accel[2] > 0) mpu->fullAngles[1] = 360 + mpu->eulerAngles[1];

}
