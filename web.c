// Handles the TCP server

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include "web.h"
#include "web_client.h"
#include "web_handlers.h"
#include "rds.h"

int web_echo = 1;

typedef int (*web_register_cb)(client_context_t* client, int arg);

typedef struct web_register {

	const char* name;
	int arg;
	web_register_cb read;
	web_register_cb write;

} web_register_t;

static web_register_t registers[] = {
	{
		.name = "PI",
		.arg = 0,
		.read = web_read_pi,
		.write = web_write_pi
	},
	{
		.name = "PS",
		.arg = 0,
		.read = web_read_ps,
		.write = web_write_ps
	},
	{
		.name = "DPS",
		.arg = 0,
		.read = web_read_dps,
		.write = web_write_dps
	},
	{
		.name = "DPSS",
		.arg = 0,
		.read = web_read_dpss,
		.write = web_write_dpss
	},
	{
		.name = "PARSE",
		.arg = 0,
		.read = web_read_parse,
		.write = web_write_parse
	},
	{
		.name = "PTY",
		.arg = 0,
		.read = web_read_pty,
		.write = web_write_pty
	},
	{
		.name = "TP",
		.arg = 0,
		.read = web_read_tp,
		.write = web_write_tp
	},
	{
		.name = "TA",
		.arg = 0,
		.read = web_read_ta,
		.write = web_write_ta
	},
	{
		.name = "AF1",
		.arg = 1,
		.read = web_read_af,
		.write = web_write_af
	},
	{
		.name = "AF2",
		.arg = 2,
		.read = web_read_af,
		.write = web_write_af
	},
	{
		.name = "AF3",
		.arg = 3,
		.read = web_read_af,
		.write = web_write_af
	},
	{
		.name = "AF4",
		.arg = 4,
		.read = web_read_af,
		.write = web_write_af
	},
	{
		.name = "AF5",
		.arg = 5,
		.read = web_read_af,
		.write = web_write_af
	},
	{
		.name = "AF6",
		.arg = 6,
		.read = web_read_af,
		.write = web_write_af
	},
	{
		.name = "AF7",
		.arg = 7,
		.read = web_read_af,
		.write = web_write_af
	},
	{
		.name = "AF7",
		.arg = 7,
		.read = web_read_af,
		.write = web_write_af
	},
	{
		.name = "AF8",
		.arg = 8,
		.read = web_read_af,
		.write = web_write_af
	},
	{
		.name = "AF9",
		.arg = 9,
		.read = web_read_af,
		.write = web_write_af
	},
	{
		.name = "AF10",
		.arg = 10,
		.read = web_read_af,
		.write = web_write_af
	},
	{
		.name = "AF11",
		.arg = 11,
		.read = web_read_af,
		.write = web_write_af
	},
	{
		.name = "AF12",
		.arg = 12,
		.read = web_read_af,
		.write = web_write_af
	},
	{
		.name = "AF13",
		.arg = 13,
		.read = web_read_af,
		.write = web_write_af
	},
	{
		.name = "AF14",
		.arg = 14,
		.read = web_read_af,
		.write = web_write_af
	},
	{
		.name = "AF15",
		.arg = 15,
		.read = web_read_af,
		.write = web_write_af
	},
	{
		.name = "DI",
		.arg = 0,
		.read = web_read_di,
		.write = web_write_di
	},
	{
		.name = "MS",
		.arg = 0,
		.read = web_read_ms,
		.write = web_write_ms
	},
	{
		.name = "TEXT",
		.arg = 0,
		.read = web_read_text,
		.write = web_write_text
	},
	{
		.name = "ECHO",
		.arg = 0,
		.read = web_read_echo,
		.write = web_write_echo
	},
	{
		.name = "RDS",
		.arg = 0,
		.read = web_read_rdson,
		.write = web_write_rdson
	},
	{
		.name = "PILOT",
		.arg = 0,
		.read = web_read_pilot,
		.write = web_write_pilot
	},
	{
		.name = "PHASE",
		.arg = 0,
		.read = web_read_phase,
		.write = web_write_phase
	},
	{
		.name = "SCROLLSPEED",
		.arg = 0,
		.read = web_read_scrollspeed,
		.write = web_write_scrollspeed
	},
	//ALWAYS AT END
	{
		.name = 0,
		.arg = 0,
		.read = 0,
		.write = 0
	}
};

// Flushes the response to the client
static int client_flush(client_context_t* client) {
	//Check if the response buffer was full, in which case blow everything up and handle as an error
	if (client->response.use >= CLIENT_BUFFER_LENGTH) {
		//Log
		printf("Response to command for client \"%s\" resulted in an overfilled response buffer.\r\n", client->ip);

		return 1;
	}

	//Send
	if (client->response.use > 0 && send(client->fd, client->response.buffer, client->response.use, 0) != client->response.use) {
		printf("Dropped client \"%s\" while transmitting %i bytes.\r\n", client->ip, client->response.use);
		return 1;
	}

	//Reset response buffer
	client->response.use = 0;

	return 0;
}

// Scans for a web register by name and returns 1 if matching, setting indexOut, otherise 0
static int find_web_register(client_context_t* client, int* indexOut) {
	int index = 0;
	while (registers[index].name != 0) {
		if (client_buffer_compare(&client->cmd, registers[index].name)) {
			(*indexOut) = index;
			return 1;
		}
		index++;
	}
	return 0;
}

// Reads out all registers stored
static int client_read_all_registers(client_context_t* client) {
	//Enumerate registers
	int index = 0;
	int status;
	while (registers[index].name != 0) {
		//Read
		status = registers[index].read(client, registers[index].arg);
		index++;

		//If failed, bail out
		if (status)
			return 1;

		//Unlock mutex during flush
		rds_unlock();

		//Flush
		status = client_flush(client);

		//Re-lock RDS mutex
		rds_lock();

		//If flush failed, bail out
		if (status)
			return 1;
	}
	return 0;
}

// Processes a command where state never left the command phase
static int client_process_command_blank(client_context_t* client) {
	//Check for certain commands
	if (client_buffer_compare(&client->cmd, "SAVE")) {
		web_proc_save(client);
		return 0;
	}
	if (client_buffer_compare(&client->cmd, "RESET")) {
		web_proc_reset(client);
		return 0;
	}

	//If length > 0 respond with NO
	if (client->cmd.use > 0) {
		client_respond_str(client, "NO\r\n");
		return 0;
	}

	//Respond with info kinda like the Inovonics does
	client_respond_str(client, "MRDS to Inovonics Emulator v0.1\r\n");

	return 0;
}

// Processes a write command
static int client_process_command_write(client_context_t* client) {
	//Scan for register
	int regIndex = -1;
	if (find_web_register(client, &regIndex))
		registers[regIndex].write(client, registers[regIndex].arg);
	else
		client_respond_str(client, "NO\r\n"); // unknown register

	return 0;
}

// Processes a read command
static int client_process_command_read(client_context_t* client) {
	//Scan for register
	int regIndex = -1;
	if (find_web_register(client, &regIndex)) {
		//Process callback
		registers[regIndex].read(client, registers[regIndex].arg);
	}
	else if (client->cmd.use == 0 && (client->arg.use == 0 || client_buffer_compare(&client->arg, "?"))) {
		//With 1 (including command ?) or 2 question marks, return all registers, flushing between each
		return client_read_all_registers(client);
	}
	else {
		//Unknown register
		client_respond_str(client, "NO\r\n");
	}

	return 0;
}

// Processes an error command
static int client_process_command_error(client_context_t* client) {
	//Return NO
	client_respond_str(client, "NO\r\n");

	return 0;
}

// Processes a command when a new line is sent; If 1 is returned, terminate client
static int client_process_command(client_context_t* client) {
	//Lock mutex
	rds_lock();

	//Switch from state
	int result;
	switch (client->state) {
	case CLIENT_STATE_CMD:
		result = client_process_command_blank(client);
		break;
	case CLIENT_STATE_ARG_READ:
		result = client_process_command_read(client);
		break;
	case CLIENT_STATE_ARG_WRITE:
		result = client_process_command_write(client);
		break;
	default:
		result = client_process_command_error(client);
		break;
	}

	//Automatically write \r\n as the inovonics does
	client_respond_eol(client);

	//Unlock mutex
	rds_unlock();

	//Commit the response buffer
	result = client_flush(client) | result;

	//Reset state
	client_reset(client);

	return result;
}

static void process_client(client_context_t* client) {
	char transportBuffer[64];

	//Reset the client
	client_reset(client);

	//Enter loop
	int ok = 1;
	while (ok) {
		//Recieve over network
		int read = (int)recv(client->fd, transportBuffer, sizeof(transportBuffer), 0);
		if (read == 0) {
			//Properly closed connection
			break;
		}
		if (read < 0) {
			//Error
			printf("Dropped %s client while reading.\r\n", client->ip);
			break;
		}

		//Process characters
		int echoLen;
		int lastEchoed = 0;
		for (int i = 0; i < read; i++) {
			//Process echoing; if this a newline, echo everything back to it
			if (transportBuffer[i] == '\n') {
				//Calculate size to echo
				echoLen = (i - lastEchoed) + 1;

				//Echo back like the Inoovnics does
				if (web_echo && send(client->fd, &transportBuffer[lastEchoed], echoLen, 0) != echoLen) {
					printf("Dropped %s client while echoing back.\r\n", client->ip);
					ok = 0;
					break;
				}

				//Update
				lastEchoed = i + 1;
			}

			//Process characters
			if (transportBuffer[i] == '\r') {
				//Intentionally left blank; Skip
			}
			else if (transportBuffer[i] == '\n') {
				//Terminate current command
				if (client_process_command(client)) {
					break;
				}
			}
			else if (transportBuffer[i] == '?' && client->state == CLIENT_STATE_CMD) {
				//Change state to accepting read argument
				client->state = CLIENT_STATE_ARG_READ;
			}
			else if (transportBuffer[i] == '=' && client->state == CLIENT_STATE_CMD) {
				//Change state to accepting write argument
				client->state = CLIENT_STATE_ARG_WRITE;
			}
			else if (client->state == CLIENT_STATE_CMD) {
				//Regular input being written into command buffer
				if (client_buffer_write(&client->cmd, &transportBuffer[i], 1)) {
					//Out of room; Change to error state
					client->state = CLIENT_STATE_ERROR;
				}
			}
			else if (client->state == CLIENT_STATE_ARG_WRITE || client->state == CLIENT_STATE_ARG_READ) {
				//Regular input being written into argument buffer
				if (client_buffer_write(&client->arg, &transportBuffer[i], 1)) {
					//Out of room; Change to error state
					client->state = CLIENT_STATE_ERROR;
				}
			}
			else {
				//Shouldn't ever fall here but if we do set to error state
				client->state = CLIENT_STATE_ERROR;
			}
		}

		//Calculate size to echo
		echoLen = read - lastEchoed;

		//Echo back like the Inoovnics does
		if (web_echo && send(client->fd, &transportBuffer[lastEchoed], echoLen, 0) != echoLen) {
			printf("Dropped %s client while echoing back.\r\n", client->ip);
			ok = 0;
		}
	}

	//Close
	close(client->fd);
}

static void* process_client_thread(void* ctxRaw) {
	//Get correct type
	client_context_t* client = (client_context_t*)ctxRaw;

	//Log
	printf("Client at %s connected.\r\n", client->ip);

	//Enter client loop
	process_client(client);

	//Log
	printf("Lost client at %s.\r\n", client->ip);

	//Finally, free client context
	free(client);

	return NULL;
}

int web_run_server(int port) {
	//Create server socket
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		printf("Failed to create server socket.\r\n");
		return 1;
	}

	//Set option
	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		printf("Failed to configure server.\r\n");
		close(server_fd);
		return 1;
	}

	//Create listening address
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	//Bind
	if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		printf("Failed to bind server.\r\n");
		close(server_fd);
		return 1;
	}

	//Listen
	if (listen(server_fd, 5) < 0) {
		printf("Failed to start listening on server.\r\n");
		close(server_fd);
		return 1;
	}

	//Log
	printf("TCP server listening on port %i\n", port);

	//Enter loop
	while (1) {
		//Accept client
		struct sockaddr_in clientAddr;
		socklen_t clientAddrLen = sizeof(clientAddr);
		int clientFd = accept(server_fd, (struct sockaddr*)&clientAddr, &clientAddrLen);
		if (clientFd < 0)
			continue;

		//Get IP address
		char ip[32];
		if (inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip)) == NULL)
			strcpy(ip, "<failed>");

		//Allocate the client context
		client_context_t* clientCtx = (client_context_t*)malloc(sizeof(client_context_t));
		if (clientCtx == NULL) {
			printf("Failed to allocate memory for client context.\r\n");
			close(clientFd);
			continue;
		}

		//Set up client
		clientCtx->fd = clientFd;
		snprintf(clientCtx->ip, CLIENT_IP_STR_LEN - 1, "%s:%i", ip, (int)ntohs(clientAddr.sin_port));

		//Spawn thread for client
		pthread_t threadId;
		if (pthread_create(&threadId, NULL, process_client_thread, (void*)clientCtx) != 0) {
			printf("Failed to create thread for client.\r\n");
			close(clientFd);
			continue;
		}
	}

	//Close server
	close(server_fd);
	return 0;
}