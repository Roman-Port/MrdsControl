#pragma once

#define CLIENT_BUFFER_LENGTH 150
#define CLIENT_IP_STR_LEN 64

#define CLIENT_STATE_CMD 0 // Client is entering command
#define CLIENT_STATE_ARG_READ 1 // Client is entering argument (after ?)
#define CLIENT_STATE_ARG_WRITE 2 // Client is entering argument (after =)
#define CLIENT_STATE_ERROR 3 // Non-sensible input; Return NO

typedef struct client_buffer {

	int use; // Number of bytes used
	char buffer[CLIENT_BUFFER_LENGTH]; // Buffer

} client_buffer_t;

typedef struct client_context {

	int fd; // Client file descriptor
	int state;
	char ip[CLIENT_IP_STR_LEN];
	client_buffer_t cmd;
	client_buffer_t arg;
	client_buffer_t response;

} client_context_t;

// Resets the state of the client object
void client_reset(client_context_t* client);

// Writes len bytes from src into the buffer, discarding anything additional. Returns 0 on success, otherwise 1 if overflowed
int client_buffer_write(client_buffer_t* buffer, const char* src, int len);

// Reads a maximum of len bytes from the buffer into dst, returning the total length.
int client_buffer_read(client_buffer_t* buffer, char* dst, int len);

// Reads the client buffer as an integer with the specified base into result. Returns 0 on success, otherwise 1
int client_buffer_read_int(client_buffer_t* buffer, int* result, int base);

// Compares the contents of the client buffer to a properly terminated string in chal. Returns 1 if they match, otherwise 0
int client_buffer_compare(client_buffer_t* buffer, const char* chal);

// Appends to the response buffer. Sends len bytes of response.
void client_respond(client_context_t* context, const char* response, int len);

// Appends to the response buffer. Sends a properly terminated string.
void client_respond_str(client_context_t* context, const char* response);

// Appends an integer to the response buffer.
void client_respond_int(client_context_t* context, int value);

// Responds on the network to a client. Sends a properly terminated string.
void client_respond_no(client_context_t* context);

// Responds on the network to a client. Sends a properly terminated string.
void client_respond_ok(client_context_t* context);

// Appends to the response buffer. Sends \r\n
void client_respond_eol(client_context_t* context);