#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "web.h"
#include "rds.h"

//#define TEST_MODE

static char conf_serial_port[256];
static int conf_serial_speed = 19200;
static int conf_web_port = 10001;

static void print_help(char* pgm) {
	printf("Usage: %s\r\n", pgm);
	printf("    -f --serial-port  : The path to the serial port to use. Required.\r\n");
	printf("    -s --serial-speed : Speed of the serial port to use. Default is 19200.\r\n");
	printf("    -p --web-port     : Port of the web server. Default is 10001.\r\n");
	printf("    -e --echo         : Enable echo at startup, 1 or 0. On by default.\r\n");
}

int parse_args(int argc, char* argv[]) {
	static const struct option long_opts[] = {
		{ "serial-port", required_argument, NULL, 'f'},
		{ "serial-speed", required_argument, NULL, 's'},
		{ "web-port", required_argument, NULL, 'p'},
		{ "echo", required_argument, NULL, 'e'},
		{ 0 }
	};

	int opt;
	while ((opt = getopt_long(argc, argv, "f:s:p:e", long_opts, NULL)) != -1) {
		switch (opt) {

		case 'f':
			// SERIAL PORT
			if (strlen(optarg) > sizeof(conf_serial_port) - 1) {
				printf("Path to serial port is too long!\r\n");
				return 1;
			}
			strcpy(conf_serial_port, optarg);
			break;

		case 's':
			// SERIAL SPEED
			conf_serial_speed = atoi(optarg);
			break;

		case 'p':
			// WEB PORT
			conf_web_port = atoi(optarg);
			break;

		case 'e':
			// ECHO
			web_echo = atoi(optarg);
			if (web_echo < 0)
				web_echo = 0;
			if (web_echo > 1)
				web_echo = 1;
			break;

		default:
			print_help(argv[0]);
			return 1;
		}
	}
	return 0;
}

int validate_args() {
	//Serial port
	if (strlen(conf_serial_port) <= 0) {
		printf("No serial port file is specified. Specify it with -f.\r\n");
		return 1;
	}

	//Serial speed
	if (conf_serial_speed <= 0) {
		printf("Serial port speed is invalid.\r\n");
		return 1;
	}

	//Web port
	if (conf_web_port <= 0 || conf_web_port > 0xFFFF) {
		printf("Web port is invalid.\r\n");
		return 1;
	}

	return 0;
}

int main(int argc, char* argv[])
{
#ifndef TEST_MODE
	//Show help automatically and quit if only 1 arg
	if (argc <= 1) {
		print_help(argc == 1 ? argv[0] : "");
		return 1;
	}
#endif

	//Init
	conf_serial_port[0] = 0;
#ifdef TEST_MODE
	strcpy(conf_serial_port, "/dev/ttyUSB0");
#endif

	//Read and validate args
	if (parse_args(argc, argv) || validate_args())
		return 1;

	//Open port
	if (rds_init(conf_serial_port, conf_serial_speed))
		return 1;

	//Start web server
	int result = web_run_server(conf_web_port);

	//Close port
	rds_close();

	return result;
}