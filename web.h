#pragma once

// Flag indicating if echo is enabled. Set on startup and by user request
extern int web_echo;

// Run web server
int web_run_server(int port);