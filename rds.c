#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <pthread.h>
#include "rds.h"

#define TRANSPORT_BUFFER_SIZE 600
//#define RDS_DUMMY_MODE

static int port_fd = -1;
static uint8_t transport_buffer[TRANSPORT_BUFFER_SIZE]; // Holds encoded data for transport
static pthread_mutex_t rds_mutex;

#ifdef RDS_DUMMY_MODE
static uint8_t dummy_regs[256];
#endif

// Opens the serial port. Returns 1 on error, otherwise 0
int rds_init(const char* port, int speed) {
	//Init the mutex
	if (pthread_mutex_init(&rds_mutex, NULL)) {
		printf("Failed to init RDS mutex.\r\n");
		return 1;
	}

#ifdef RDS_DUMMY_MODE
	printf("WARN: Running in dummy mode!\r\n");
	memset(dummy_regs, 0, sizeof(dummy_regs));
#else

	//Determine baud rate to use
	speed_t speedCfg;
	switch (speed) {
	case 19200:
		speedCfg = B19200;
		break;
	case 2400:
		speedCfg = B2400;
		break;
	default:
		printf("WARN: Speed is not officially supported and may not function correctly. Continuing on...\r\n");
		speedCfg = speed;
		break;
	}

	//Open port
	port_fd = open(port, O_RDWR | O_NOCTTY);
	if (port_fd < 0) {
		printf("Failed to open serial port %s with error %i: %s\r\n", port, errno, strerror(errno));
		return 1;
	}

	//Pull settings
	struct termios tty;
	memset(&tty, 0, sizeof(struct termios));
	if (tcgetattr(port_fd, &tty) != 0) {
		printf("Failed to tcgetattr serial port with error %i: %s\r\n", errno, strerror(errno));
		return 1;
	}

	//Configure...
	//Parity: None
	tty.c_cflag &= ~PARENB;
	//Stop bits: 1
	tty.c_cflag &= ~CSTOPB;
	//Bits per byte: 8
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;
	//Disable hardware flow control
	tty.c_cflag &= ~CRTSCTS;
	//Turn on READ & ignore ctrl lines
	tty.c_cflag |= CREAD | CLOCAL;

	//Local modes: Canonical vs Raw
	tty.c_lflag &= ~ICANON; // Disable canonical mode (read byte-by-byte)
	tty.c_lflag &= ~ECHO;   // Disable echo
	tty.c_lflag &= ~ECHOE;  // Disable erasure
	tty.c_lflag &= ~ECHONL; // Disable new-line echo
	tty.c_lflag &= ~ISIG;   // Disable interpretation of INTR, QUIT and SUSP

	//Input modes
	tty.c_iflag &= ~(IXON | IXOFF | IXANY); //Turn off software flow control
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); //Disable any special handling of received bytes

	//Output modes
	tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
	tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

	//Timeout settings (Blocking read until 1 byte arrives)
	tty.c_cc[VTIME] = 5; // Half second timeout
	tty.c_cc[VMIN] = 0;

	//Set Baud Rate
	cfsetispeed(&tty, speedCfg);
	cfsetospeed(&tty, speedCfg);

	//Commit
	if (tcsetattr(port_fd, TCSANOW, &tty) != 0) {
		printf("Failed to tcsetattr serial port with error %i: %s\n", errno, strerror(errno));
		return 1;
	}

#endif
	return 0;
}

// Shuts down the port
void rds_close() {
#ifndef RDS_DUMMY_MODE
	close(port_fd);
	port_fd = -1;
#endif
}

int rds_register_write(uint8_t reg, uint8_t length, const uint8_t* buffer) {
#ifdef RDS_DUMMY_MODE
	memcpy(&dummy_regs[reg], buffer, length);
#else
	//Set up the write command in transport_buffer
	int outOffset = 0;
	transport_buffer[outOffset++] = 0xFE; // Start byte
	transport_buffer[outOffset++] = reg; // Register

	//Stuff/escape bytes
	for (int i = 0; i < length; i++) {
		//Check bounds
		if (outOffset >= TRANSPORT_BUFFER_SIZE)
			return RDS_ERROR_OVERFLOW;

		//Check if it can be written as-is or must be escaped
		if (buffer[i] >= 0xFD) {
			//Write the escape code
			transport_buffer[outOffset++] = 0xFD;

			//Check bounds
			if (outOffset >= TRANSPORT_BUFFER_SIZE)
				return RDS_ERROR_OVERFLOW;

			//Write the packed byte
			transport_buffer[outOffset++] = buffer[i] - 0xFD;
		}
		else {
			//Write directly
			transport_buffer[outOffset++] = buffer[i];
		}
	}

	//Check bounds
	if (outOffset >= TRANSPORT_BUFFER_SIZE)
		return RDS_ERROR_OVERFLOW;

	//Write stop byte
	transport_buffer[outOffset++] = 0xFF;

	//Transmit
	if (write(port_fd, transport_buffer, outOffset) != outOffset)
		return RDS_ERROR_TRANSMIT;
#endif

	//OK
	return RDS_ERROR_OK;
}

// Reads register reg, reading length bytes, into buffer.
int rds_register_read(uint8_t reg, uint8_t length, uint8_t* buffer) {
#ifdef RDS_DUMMY_MODE
	memcpy(buffer, &dummy_regs[reg], length);
#else
	//Create the write command
	uint8_t writeBuf[5];
	writeBuf[0] = 0xFE; // Start byte
	writeBuf[1] = 0xD0; // Read constant
	writeBuf[2] = reg;
	writeBuf[3] = length;
	writeBuf[4] = 0xFF; // End byte

	//Write
	if (write(port_fd, writeBuf, sizeof(writeBuf)) != sizeof(writeBuf))
		return RDS_ERROR_TRANSMIT;

	//Skip any garbage until the correct start byte
	uint8_t start = 0;
	int garbageCount = 0;
	while (start != 0xFE) {
		//At some point give up
		if (garbageCount++ >= 255)
			return RDS_ERROR_MALFORMED_REPLY;

		//Read
		if (read(port_fd, &start, 1) != 1)
			return RDS_ERROR_NO_REPLY;
	}

	//Read the response register byte
	uint8_t responseReg;
	if (read(port_fd, &responseReg, 1) != 1)
		return RDS_ERROR_NO_REPLY;
	if (responseReg != reg)
		return RDS_ERROR_MALFORMED_REPLY;

	//Start processing data
	int index = 0;
	while (index < length) {
		//Read byte
		if (read(port_fd, &buffer[index], 1) != 1)
			return RDS_ERROR_NO_REPLY;

		//Check if it makes sense
		if (buffer[index] > 0xFD)
			return RDS_ERROR_MALFORMED_REPLY;

		//Check if this is an escape character
		if (buffer[index] == 0xFD) {
			//Read again
			if (read(port_fd, &buffer[index], 1) != 1)
				return RDS_ERROR_NO_REPLY;

			//Check if it makes sense
			if (buffer[index] > 3)
				return RDS_ERROR_MALFORMED_REPLY;

			//Transform
			buffer[index] += 0xFD;
		}

		//Increment index
		index++;
	}

	//Read the stop byte
	uint8_t stop;
	if (read(port_fd, &stop, 1) != 1)
		return RDS_ERROR_NO_REPLY;
	if (stop != 0xFF)
		return RDS_ERROR_MALFORMED_REPLY;
#endif

	//OK
	return RDS_ERROR_OK;
}

// Locks the RDS mutex
void rds_lock() {
	pthread_mutex_lock(&rds_mutex);
}

// Unlocks the RDS mutex
void rds_unlock() {
	pthread_mutex_unlock(&rds_mutex);
}