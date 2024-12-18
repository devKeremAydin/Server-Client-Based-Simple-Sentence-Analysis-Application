#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

int main() {
    WSADATA wsa;
    SOCKET client_socket;
    struct sockaddr_in server_addr;
    char message[BUFFER_SIZE];
    char server_response[BUFFER_SIZE];

    // winsock ba�latma
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed. Error code: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    // istemci soketi olu�turma
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) {
        printf("Socket creation failed. Error code: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    // sunucu adres bilgileri ayarlama
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        printf("Invalid address or address not supported.\n");
        exit(EXIT_FAILURE);
    }

    // sunucuya ba�lant� yapma
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Connection to server failed. Error code: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    printf("Connected to the server.\n");

    while (1) {
        printf("Enter a sentence (or type 'exit' to quit): ");
        fgets(message, BUFFER_SIZE, stdin); // kullan�c�dan mesaj alma
        message[strcspn(message, "\n")] = '\0'; // sonundaki yeni sat�r� temizle

        // ��k�� i�lemi
        if (strcmp(message, "exit") == 0) {
            send(client_socket, message, strlen(message), 0);
            break;
        }

        // mesaj� sunucuya g�nderme
        if (send(client_socket, message, strlen(message

