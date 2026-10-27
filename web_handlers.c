// Contains actual handlers for requests

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "web.h"
#include "web_handlers.h"
#include "rds.h"

// Macro that checks the error and bails out with a message on issue
#define RDSOP(op) if (check_rds_error(client, op)) { return 1; }

// Replaces any illegal characters, including NULL, in the text with spaces
static void replace_illegal_chars(char* text, int len) {
	for (int i = 0; i < len; i++) {
		if (text[i] < 32)
			text[i] = ' ';
	}
}

// If there is an RDS error, responds to it on the client
static int check_rds_error(client_context_t* client, int result) {
	switch (result) {
	case RDS_ERROR_OK:
		break; // Default
	case RDS_ERROR_NO_REPLY:
		printf("RDS operation failed; IC did not respond.\r\n");
		client_respond_str(client, "NO ; RDS IC DID NOT RESPOND\r\n");
		break;
	case RDS_ERROR_MALFORMED_REPLY:
		printf("RDS operation failed; Invalid reply.\r\n");
		client_respond_str(client, "NO ; RDS IC RESPONDED WITH INVALID DATA\r\n");
		break;
	case RDS_ERROR_TRANSMIT:
		printf("RDS operation failed; Transmission failure - Cable unplugged?\r\n");
		client_respond_str(client, "NO ; RDS IC TRANSMISSION ERROR\r\n");
		break;
	case RDS_ERROR_OVERFLOW:
		printf("RDS operation failed; Command buffer overflow\r\n");
		client_respond_str(client, "NO ; RDS IC TX OVERFLOW ERROR\r\n");
		break;
	default:
		printf("RDS operation failed; Unknown error.\r\n");
		client_respond_str(client, "NO ; RDS IO UNKNOWN ERROR\r\n");
		break;
	}
	return result;
}

// Utility function for reading a single integer from the IC
static int read_single_int(client_context_t* client, int reg, const char* prefix) {
	//Read from IC
	uint8_t result;
	RDSOP(rds_register_read(reg, 1, &result));

	//Write
	client_respond_str(client, prefix);
	client_respond_int(client, (int)result);
	client_respond_eol(client);

	return 0;
}

// Utility function for writing a single integer from the IC
static int write_single_int(client_context_t* client, int reg, int min, int max) {
	//Attempt to parse code
	int value;
	if (client_buffer_read_int(&client->arg, &value, 10) || value < min || value > max) {
		//Invalid
		client_respond_no(client);
		return 1;
	}

	//Write to IC
	uint8_t valueByte = (uint8_t)value;
	RDSOP(rds_register_write(reg, 1, &valueByte));

	//Respond OK
	client_respond_ok(client);

	return 0;
}

// Utility function that procsses a write to a control register and sends back OK
static int write_control_register(client_context_t* client, uint8_t cmd) {
	//Write to IC's control register
	RDSOP(rds_register_write(RDS_REG_CONTROL, 1, &cmd));

	//Respond OK
	client_respond_ok(client);

	return 0;
}

// Utility function for reading from the status register from the IC
static int read_status_register(client_context_t* client, int shift, const char* prefix) {
	//Read status register from IC
	uint8_t result;
	RDSOP(rds_register_read(RDS_REG_STATUS, 1, &result));

	//Shift
	result = (result >> shift) & 1;

	//Write
	client_respond_str(client, prefix);
	client_respond_int(client, (int)result);
	client_respond_eol(client);

	return 0;
}

// PI

int web_read_pi(client_context_t* client, int arg) {
	//Read from IC
	uint16_t pi;
	RDSOP(rds_register_read(RDS_REG_PI, 2, (uint8_t*)&pi));

	//Swap order
	pi = ((pi >> 8) & 0xFF) | (pi << 8);

	//Print to buffer
	char temp[8];
	snprintf(temp, sizeof(temp) - 1, "%04X", (int)pi);

	//Write
	client_respond_str(client, "PI=");
	client_respond_str(client, temp);
	client_respond_eol(client);

	return 0;
}

int web_write_pi(client_context_t* client, int arg) {
	//Attempt to parse PI code
	int pi;
	if (client_buffer_read_int(&client->arg, &pi, 16) || pi < 0 || pi > 0xFFFF) {
		//Invalid
		client_respond_no(client);
		return 1;
	}

	//Swap order
	pi = ((pi >> 8) & 0xFF) | (pi << 8);

	//Write to IC
	uint16_t piShort = (uint16_t)pi;
	RDSOP(rds_register_write(RDS_REG_PI, 2, (uint8_t*) &piShort));

	//Respond OK
	client_respond_ok(client);

	return 0;
}

// (static) PS

int web_read_ps(client_context_t* client, int arg) {
	//Read from chip
	char data[8];
	RDSOP(rds_register_read(RDS_REG_PS_BUFFERED, 8, data));

	//Replace illegal characters
	replace_illegal_chars(data, sizeof(data));

	//Write the header
	client_respond_str(client, "PS=");
	client_respond(client, data, sizeof(data));
	client_respond_eol(client);

	return 0;
}

int web_write_ps(client_context_t* client, int arg) {
	//Init intermediate buffer
	char data[8];
	memset(data, ' ', sizeof(data));

	//Read the client's text into and replace illegal characters
	client_buffer_read(&client->arg, data, 8);
	replace_illegal_chars(data, 8);

	//Write to chip
	RDSOP(rds_register_write(RDS_REG_PS_BUFFERED, 8, data));

	//Respond OK
	client_respond_ok(client);

	return 0;
}

// DPS

int web_read_dps(client_context_t* client, int arg) {
	//Read DPS + DPSNUM from chip
	char data[81];
	RDSOP(rds_register_read(RDS_REG_DPSNUM, 81, data));

	//The first byte contains the number of characters to copy
	int chars = (int)data[0];
	if (chars < 0 || chars > 80)
		chars = 0;

	//Write the header
	client_respond_str(client, "DPS=");
	if (chars > 0)
		client_respond(client, &data[1], chars);
	client_respond_eol(client);

	return 0;
}

int web_write_dps(client_context_t* client, int arg) {
	//Init intermediate buffer for DPS + DPSLEN
	char data[81];
	memset(data, ' ', sizeof(data));

	//Read the client's text starting at 1 of the buffer and replace illegal characters
	int len = client_buffer_read(&client->arg, &data[1], 80);
	replace_illegal_chars(&data[1], 80);

	//Set DPSLEN to length
	data[0] = len;

	//Write to chip
	RDSOP(rds_register_write(RDS_REG_DPSNUM, 81, data));

	//Respond OK
	client_respond_ok(client);

	return 0;
}

// DPSS

int web_read_dpss(client_context_t* client, int arg) {
	return read_single_int(client, RDS_REG_LABPER, "DPSS=");
}

int web_write_dpss(client_context_t* client, int arg) {
	return write_single_int(client, RDS_REG_LABPER, 0, 255); // should be between 0-9 but whatever
}

// PARSE

int web_read_parse(client_context_t* client, int arg) {
	//Read the DPS mode register
	uint8_t mode;
	RDSOP(rds_register_read(RDS_REG_DPSMOD, 1, &mode));

	//Translate from the mode the IC uses to he one the inovonics uses
	int inoResult = 8;
	switch (mode) {
	case 0: inoResult = 8; break; //Fixed 8 chars
	case 1: inoResult = 1; break; //1 character at a time
	case 2: inoResult = 0; break; //Word alignment
	case 3: inoResult = 1; break; //1 character at a time (alt)
	}

	//Write the header
	client_respond_str(client, "PARSE=");
	client_respond_int(client, inoResult);
	client_respond_eol(client);

	return 0;
}

int web_write_parse(client_context_t* client, int arg) {
	//Read integer from client
	int ino;
	if (client_buffer_read_int(&client->arg, &ino, 10)) {
		//Invalid
		client_respond_no(client);
		return 1;
	}

	//Convert to what the chip expects
	uint8_t mode;
	switch (ino) {
	case 8: mode = 0; break; // Fixed 8 chars
	case 1: mode = 1; break; // 1 character at a time
	case 0: mode = 2; break; // Word alignment
	default:
		//Respond NO
		client_respond_no(client);
		return 1;
	}

	//Write to chip
	RDSOP(rds_register_write(RDS_REG_DPSMOD, 1, &mode));

	//Respond OK
	client_respond_ok(client);

	return 0;
}

// PTY

int web_read_pty(client_context_t* client, int arg) {
	return read_single_int(client, RDS_REG_PTY, "PTY=");
}

int web_write_pty(client_context_t* client, int arg) {
	return write_single_int(client, RDS_REG_PTY, 0, 31);
}

// TP

int web_read_tp(client_context_t* client, int arg) {
	return read_single_int(client, RDS_REG_TP, "TP=");
}

int web_write_tp(client_context_t* client, int arg) {
	return write_single_int(client, RDS_REG_TP, 0, 1);
}

// TA

int web_read_ta(client_context_t* client, int arg) {
	return read_single_int(client, RDS_REG_TA, "TA=");
}

int web_write_ta(client_context_t* client, int arg) {
	return write_single_int(client, RDS_REG_TA, 0, 1);
}

// DI

int web_read_di(client_context_t* client, int arg) {
	return read_single_int(client, RDS_REG_DI, "DI=");
}

int web_write_di(client_context_t* client, int arg) {
	return write_single_int(client, RDS_REG_DI, 0, 15);
}

// MS

int web_read_ms(client_context_t* client, int arg) {
	return read_single_int(client, RDS_REG_MS, "MS=");
}

int web_write_ms(client_context_t* client, int arg) {
	return write_single_int(client, RDS_REG_MS, 0, 1);
}

// TEXT

int web_read_text(client_context_t* client, int arg) {
	//Read from chip
	char data[64];
	RDSOP(rds_register_read(RDS_REG_RT, 64, data));

	//Replace illegal characters
	replace_illegal_chars(data, sizeof(data));

	//Write the header
	client_respond_str(client, "TEXT=");
	client_respond(client, data, sizeof(data));
	client_respond_eol(client);

	return 0;
}

int web_write_text(client_context_t* client, int arg) {
	//Init intermediate buffer for RT + RTEN control register
	char data[65];
	memset(data, ' ', sizeof(data));

	//Read the current control register from the IC
	RDSOP(rds_register_read(RDS_REG_RTEN, 1, data));

	//Read the client's RT into the rest of the buffer and replace illegal characters
	client_buffer_read(&client->arg, &data[1], 64);
	replace_illegal_chars(&data[1], 64);

	//Enable RT in the control register
	data[0] |= 1 << 0;

	//Flip the A/B flag in the control register
	data[0] ^= 1 << 1;

	//Write to chip
	RDSOP(rds_register_write(RDS_REG_RTEN, 65, data));

	//Respond OK
	client_respond_ok(client);

	return 0;
}

// AFs

int web_read_af(client_context_t* client, int arg) {
	//Retrieve this AF
	uint8_t af;
	RDSOP(rds_register_read(RDS_REG_AF + arg, 1, &af));

	//Format
	char result[32];
	snprintf(result, sizeof(result) - 1, "AF%i=%i\r\n", arg, (int)af);

	//Send
	client_respond_str(client, result);

	return 0;
}

int web_write_af(client_context_t* client, int arg) {
	//Read the value and perform bounds check
	int value;
	if (client_buffer_read_int(&client->arg, &value, 10) || value < 0 || value > 204) {
		//Invalid
		client_respond_no(client);
		return 1;
	}

	//Read all AFs from IC
	uint8_t afs[16];
	RDSOP(rds_register_read(RDS_REG_AF, sizeof(afs), afs));
	
	//Update in memory
	afs[arg] = value;

	//Enumerate and find the max valid AF to determine the count
	uint8_t count = 0;
	for (uint8_t i = 1; i < 16; i++) {
		if (afs[i] > 0)
			count = i;
	}

	//Write count into first slot
	afs[0] = count;

	//Write to IC
	RDSOP(rds_register_write(RDS_REG_AF, sizeof(afs), afs));

	//Respond OK
	client_respond_ok(client);

	return 0;
}

// ECHO

int web_read_echo(client_context_t* client, int arg) {
	client_respond_str(client, "ECHO=");
	client_respond_int(client, web_echo);
	client_respond_eol(client);
	return 0;
}

int web_write_echo(client_context_t* client, int arg) {
	//Attempt to parse
	int value;
	if (client_buffer_read_int(&client->arg, &value, 10) || value < 0 || value > 1) {
		//Invalid
		client_respond_no(client);
		return 1;
	}

	//Set
	web_echo = value;

	//Return OK
	client_respond_ok(client);

	return 0;
}

// RDS (on/off)

int web_read_rdson(client_context_t* client, int arg) {
	return read_status_register(client, 7, "RDS=");
}

int web_write_rdson(client_context_t* client, int arg) {
	//Read the value as int
	int value;
	if (client_buffer_read_int(&client->arg, &value, 10))
		value = -1;

	//Switch on this
	switch (value) {
	case 0:
		write_control_register(client, RDS_CTRL_OFF);
		break;
	case 1:
		write_control_register(client, RDS_CTRL_ON);
		break;
	default:
		client_respond_no(client);
		return 1;
	}

	return 0;
}

// PILOT

int web_read_pilot(client_context_t* client, int arg) {
	return read_status_register(client, 0, "PILOT=");
}

int web_write_pilot(client_context_t* client, int arg) {
	return write_single_int(client, RDS_REG_EXTSYNC, 0, 1);
}

// PHASE

int web_read_phase(client_context_t* client, int arg) {
	return read_single_int(client, RDS_REG_PHASE, "PHASE=");
}

int web_write_phase(client_context_t* client, int arg) {
	return write_single_int(client, RDS_REG_PHASE, 0, 18);
}

// X_SCROLLSPEED

int web_read_scrollspeed(client_context_t* client, int arg) {
	return read_single_int(client, RDS_REG_SCROLLSPD, "SCROLLSPEED=");
}

int web_write_scrollspeed(client_context_t* client, int arg) {
	return write_single_int(client, RDS_REG_SCROLLSPD, 0, 1);
}

// COMMANDS

void web_proc_save(client_context_t* client) {
	write_control_register(client, RDS_CTRL_SAVE);
}

void web_proc_reset(client_context_t* client) {
	write_control_register(client, RDS_CTRL_RESET);
}