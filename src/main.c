#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdint.h>

#define SERVER_PORT 9876
#define MAX_PACKET_SIZE 4096

typedef enum {
	MESSAGE_PACKET = 0,
	REGISTER_PACKET = 1,
	CONNECT_PACKET = 2,
	ACK_PACKET = 3,
	PEER_PACKET = 4,
	FAILURE_PACKET = 5,
} Header;

typedef struct {
	uint64_t first;
	uint64_t second;
} UUID;

typedef struct {
	Header type;
	UUID peer;
} RegisterPacket;

typedef struct {
	Header type;
	UUID from;
	UUID to;
} ConnectPacket;

typedef struct {
	Header type;
	UUID id;
} AckPacket;

int main(int argc, const char** argv) {
	// setup
	int server_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (server_socket < 0) {
		printf("Socket failed...\n");
		exit(1);
	}
	struct sockaddr_in server_addr;
	char buffer[MAX_PACKET_SIZE] = { 0 };
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	//inet_pton(AF_INET, "0.0.0.0", &server_addr.sin_addr);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr))) {
		printf("Bind failed...\n");
		exit(1);
	}
	printf("Successfully set up udp punching server!\n");
	
	// activity
	while (1) {
		struct sockaddr_in client_addr;
		socklen_t addr_len = sizeof(client_addr);
		int recieved = recvfrom(
				server_socket,
				buffer,
				MAX_PACKET_SIZE,
				0,
				(struct sockaddr*)&client_addr,
				&addr_len);
		if (recieved < 0) {
			printf("ERROR: an error occured while recieving data\n");
			continue;
		}
		if (recieved <= (int)sizeof(Header)) {
			printf("ERROR: an invalid packet was reieved\n");
			continue;
		}
		if (recieved >= MAX_PACKET_SIZE) {
			printf("ERROR: an abnormally large packet was recieved\n");
			continue;
		}
		buffer[recieved] = '\0';
		Header packtype;
		AckPacket ack = { 0 };
		ack.type = ACK_PACKET;
		memcpy(&packtype, buffer, sizeof(Header));
		switch (packtype) {
			case REGISTER_PACKET:
				printf("Registering peer %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
				ack.id.first = 0;
				ack.id.second = 0;
				memcpy(buffer, &ack, sizeof(Header));
				buffer[sizeof(Header)] = '\0';
				sendto(server_socket, buffer, sizeof(Header), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
				break;
			default: break;
		}
	}

	// close
	close(server_socket);
	return 0;
}
