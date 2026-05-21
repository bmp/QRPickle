#ifndef WEB_SERVER_H
#define WEB_SERVER_H

// Registers endpoints and mounts the asynchronous HTTP engine on Port 80
void web_server_init();

// Shuts down the socket threads cleanly to release memory when transitioning out of AP mode
void web_server_stop();

// Monitored flag hook that checks if an incoming web submit requires a system reboot
void web_server_update();

#endif // WEB_SERVER_H
