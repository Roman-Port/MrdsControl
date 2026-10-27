#pragma once

#include <stdint.h>

#define RDS_ERROR_OK 0
#define RDS_ERROR_NO_REPLY 1
#define RDS_ERROR_MALFORMED_REPLY 2
#define RDS_ERROR_TRANSMIT 3
#define RDS_ERROR_OVERFLOW 4

#define RDS_REG_PI 0
#define RDS_REG_PS_BUFFERED 2
#define RDS_REG_PTY 10
#define RDS_REG_DI 11
#define RDS_REG_MS 12
#define RDS_REG_TP 13
#define RDS_REG_TA 14
#define RDS_REG_AF 15
#define RDS_REG_RTEN 31
#define RDS_REG_RT 32
#define RDS_REG_UDG1EN_BUF 96
#define RDS_REG_UDG2_BUF 103
#define RDS_REG_UDG2_CNT 109
#define RDS_REG_EXTSYNC 110
#define RDS_REG_PHASE 111
#define RDS_REG_STATUS 112
#define RDS_REG_CONTROL 113
#define RDS_REG_SPSPER 114
#define RDS_REG_DPSMOD 115
#define RDS_REG_LABPER 116
#define RDS_REG_SCROLLSPD 117
#define RDS_REG_DPSNUM 118
#define RDS_REG_DPS 119
#define RDS_REG_PS_RAW 200

#define RDS_CTRL_SAVE 0x45
#define RDS_CTRL_RESET 0x52
#define RDS_CTRL_OFF 0x30
#define RDS_CTRL_ON 0x31

// Opens the serial port. Returns 1 on error, otherwise 0
int rds_init(const char* port, int speed);

// Shuts down the port
void rds_close();

// Writes length bytes to the specified register. Returns 1 on error, otherwise 0
int rds_register_write(uint8_t reg, uint8_t length, const uint8_t* buffer);

// Reads length bytes to the specified register. Returns 1 on error, otherwise 0
int rds_register_read(uint8_t reg, uint8_t length, uint8_t* buffer);

// Locks the RDS mutex
void rds_lock();

// Unlocks the RDS mutex
void rds_unlock();