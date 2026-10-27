#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>

#include "web_client.h"

// Resets the state of the client object
void client_reset(client_context_t* client) {
	//Reset state to awaiting command
	client->state = CLIENT_STATE_CMD;

	//Reset buffers
	client->cmd.use = 0;
	client->arg.use = 0;
	client->response.use = 0;
}

// Writes len bytes from src into the buffer, discarding anything additional. Returns 0 on success, otherwise 1 if overflowed
int client_buffer_write(client_buffer_t* buffer, const char* src, int len) {
	for (int i = 0; i < len; i++) {
		//Check if out of room
		if (buffer->use >= CLIENT_BUFFER_LENGTH)
			return 1;

		//Copy
		buffer->buffer[buffer->use++] = src[i];
	}
	return 0;
}

// Reads a maximum of len bytes from the buffer into dst, returning the total length.
int client_buffer_read(client_buffer_t* buffer, char* dst, int len) {
	int copied = 0;
	while (copied < len && copied < buffer->use) {
		dst[copied] = buffer->buffer[copied];
		copied++;
	}
	return copied;
}

// Reads the client buffer as an integer with the specified base into result. Returns 0 on success, otherwise 1
int client_buffer_read_int(client_buffer_t* buffer, int* result, int base) {
	//Read the input and set a NULL terminator
	char temp[16];
	int len = client_buffer_read(buffer, temp, sizeof(temp) - 1);
	temp[len] = 0;

	//Attempt to parse
	char* endptr = 0;
	long val = strtol(temp, &endptr, base);

	//Check for overflow or underflow of integer itself
	if (val > INT_MAX || val < INT_MIN)
		return 1;

	//Check for valid input
	if (endptr == temp)
		return 1;

	//Check for trailing
	if (*endptr != 0)
		return 1;

	//Set output
	(*result) = val;

	return 0;
}

// Compares the contents of the client buffer to a properly terminated string in chal. Returns 1 if they match, otherwise 0
int client_buffer_compare(client_buffer_t* buffer, const char* chal) {
	//Get length of the challenge
	int chalLen = strlen(chal);

	//Check if the length matches
	if (buffer->use != chalLen)
		return 0;

	//Compare strings, ignoring case, using a fixed value. There is no standard lib call for this so do it manually
	for (int i = 0; i < chalLen; i++) {
		if (toupper(chal[i]) != toupper(buffer->buffer[i]))
			return 0;
	}

	return 1;
}

// Responds on the network to a client. Sends len bytes of response.
void client_respond(client_context_t* context, const char* response, int len) {
	client_buffer_write(&context->response, response, len);
}

// Responds on the network to a client. Sends a properly terminated string.
void client_respond_str(client_context_t* context, const char* response) {
	//Get length of the string
	int len = strlen(response);

	//Write
	client_respond(context, response, len);
}

// Appends an integer to the response buffer.
void client_respond_int(client_context_t* context, int value) {
	//Print to buffer
	char temp[16];
	snprintf(temp, sizeof(temp) - 1, "%i", value);

	//Write
	client_respond_str(context, temp);
}

// Responds on the network to a client. Sends a properly terminated string.
void client_respond_no(client_context_t* context) {
	client_respond_str(context, "NO\r\n");
}

// Responds on the network to a client. Sends a properly terminated string.
void client_respond_ok(client_context_t* context) {
	client_respond_str(context, "OK\r\n");
}

// Responds on the network to a client. Sends a properly terminated string.
void client_respond_eol(client_context_t* context) {
	client_respond_str(context, "\r\n");
}