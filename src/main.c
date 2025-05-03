#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <inttypes.h>

#define SERVER_PORT 9876
#define MAX_PACKET_SIZE 4096
#define MAX_MESSAGE_SIZE 128

#define uuideq(u1, u2) (u1.first == u2.first && u1.second == u2.second)

typedef struct {
	uint8_t address[4];
} Ipv4;

typedef struct {
	Ipv4 address;
	uint16_t port;
} Destination;

typedef enum {
	MESSAGE_PACKET = 0,
	REGISTER_PACKET = 1,
	CONNECT_PACKET = 2,
	ACK_PACKET = 3,
	PEER_PACKET = 4,
	FAILURE_PACKET = 5,
	PUNCH_PACKET = 6,
	FIST_PACKET = 7,
} Header;

typedef struct {
	uint64_t first;
	uint64_t second;
} UUID;

typedef struct {
	Header type;
} FistPacket;

typedef struct {
	Header type;
	Destination destination;
} PunchPacket;

typedef struct {
	Header type;
	UUID peer;
} RegisterPacket;

typedef struct {
	Header type;
	UUID to;
} ConnectPacket;

typedef struct {
	Header type;
	UUID id;
} AckPacket;

typedef struct {
	Header type;
	Destination destination;
} PeerPacket;

typedef struct {
	UUID id;
	Destination destination;
	void* next;
} Peer;

typedef struct {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t zone;
} Timestamp;

typedef struct {
    Header type;
    UUID from;
    UUID to;
    UUID id;
    Timestamp time;
    uint16_t size;
    char text[MAX_MESSAGE_SIZE]; // IMPORTANT: this should be last
} Message;

Peer* g_peers = NULL;
size_t g_num_peers = 0;

PeerPacket find_peer(UUID id) {
	PeerPacket p = { 0 };
	p.type = PEER_PACKET;
	Peer* curr = g_peers;
	while (curr) {
		if (uuideq(id, curr->id)) {
			p.destination = curr->destination;
			printf("Found peer ID#%" PRIx64 "%" PRIx64 "\n", curr->id.first, curr->id.second);
			return p;
		}
		curr = (Peer*)curr->next;
	}
	printf("UNKNOWN Peer ID#%" PRIx64 "%" PRIx64 " was requested\n", id.first, id.second);
	return p;
}

PunchPacket get_punch(struct sockaddr_in peer_addr) {
	PunchPacket p = { 0 };
	p.type = PUNCH_PACKET;
	p.destination.port = ntohs(peer_addr.sin_port);
	memcpy(p.destination.address.address, &peer_addr.sin_addr.s_addr, 4);
	return p;
}

struct sockaddr_in get_sock_addr(Destination destination) {
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	memcpy(&sin.sin_addr.s_addr, destination.address.address, 4);
	sin.sin_port = htons(destination.port);
	return sin;
}

void register_peer(struct sockaddr_in addr, UUID id) {
	Peer* curr = g_peers;
	while (curr) {
		if (uuideq(id, curr->id)) {
			memcpy(curr->destination.address.address, &addr.sin_addr.s_addr, 4);
			curr->destination.port = ntohs(addr.sin_port);
			printf("Updated peer ID#%" PRIx64 "%" PRIx64 " to address %s:%d\n", id.first, id.second, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
			return;
		}
		curr = (Peer*)curr->next;
	}
	Peer* new = calloc(1, sizeof(Peer));
	new->id = id;
	memcpy(new->destination.address.address, &addr.sin_addr.s_addr, 4);
	new->destination.port = ntohs(addr.sin_port);
	new->next = g_peers;
	g_peers = new;
	g_num_peers++;
	printf("Added new peer#%d with ID#%" PRIx64 "%" PRIx64 " on address %s:%d\n", (int)g_num_peers, id.first, id.second, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
}

int main(int argc, const char** argv) {
	// setup
	int server_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (server_socket < 0) {
		printf("Socket failed...\n");
		exit(1);
	}
	int optval = 1;
	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
		printf("Unable to set server socket options\n");
		exit(1);
	}
	struct sockaddr_in server_addr;
	char buffer[MAX_PACKET_SIZE] = { 0 };
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
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
		RegisterPacket regp;
		ConnectPacket conp;
		PeerPacket peep;
		PunchPacket punp;
		AckPacket ack = { 0 };
		Message msg = { 0 };
		ack.type = ACK_PACKET;
		punp.type = PUNCH_PACKET;
		memcpy(&packtype, buffer, sizeof(Header));
		switch (packtype) {
			case REGISTER_PACKET:
				memcpy(&regp, buffer, sizeof(RegisterPacket));
				register_peer(client_addr, regp.peer);
				ack.id.first = 0;
				ack.id.second = 0;
				memcpy(buffer, &ack, sizeof(AckPacket));
				buffer[sizeof(AckPacket)] = '\0';
				sendto(server_socket, buffer, sizeof(AckPacket), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
				break;
			case CONNECT_PACKET:
				memcpy(&conp, buffer, sizeof(ConnectPacket));
				peep = find_peer(conp.to);
				punp = get_punch(client_addr);
				struct sockaddr_in punch_addr = get_sock_addr(peep.destination);
				memcpy(buffer, &punp, sizeof(PunchPacket));
				buffer[sizeof(PunchPacket)] = '\0';
				sendto(server_socket, buffer, sizeof(PunchPacket), 0, (struct sockaddr*)&punch_addr, sizeof(punch_addr));
				printf("Sent a punch command to %s:%d\n", inet_ntoa(punch_addr.sin_addr), ntohs(punch_addr.sin_port));
				memcpy(buffer, &peep, sizeof(PeerPacket));
				buffer[sizeof(PeerPacket)] = '\0';
				sendto(server_socket, buffer, sizeof(PeerPacket), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
				break;
			case MESSAGE_PACKET:
				printf("TODO");
				break;
			case ACK_PACKET:
				printf("TODO");
				break;
			default:
				printf("An unknown packet type was recieved\n");
				break;
		}
	}

	// close
	close(server_socket);
	return 0;
}
