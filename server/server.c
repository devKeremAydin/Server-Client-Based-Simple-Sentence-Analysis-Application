#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT 8080
#define BUFFER_SIZE 1024

int client_count = 0;
CRITICAL_SECTION client_count_lock;

// kelime ile ilgili hesaplamalar� yapan fonksiyon
void count_word_frequency(char *buffer, int *letter_count, int *word_count, char most_frequent_word[BUFFER_SIZE], int *max_frequency) {
    char *word;
    char *delim = " \t\n\r"; // kelimeleri ay�rmak i�in kullan�lan ay�r�c�lar
    char temp_buffer[BUFFER_SIZE];
    char word_list[100][BUFFER_SIZE]; // farkl� kelimeleri saklamak i�in dizi
    int word_freq[100] = {0}; // kelimelerin frekanslar�n� tutmak i�in dizi
    int word_index = 0;
    int i;

    *letter_count = 0;
    *word_count = 0;
    *max_frequency = 0;
    most_frequent_word[0] = '\0'; // en s�k kelime i�in ba�lang�� de�eri

    strcpy(temp_buffer, buffer); 
    word = strtok(temp_buffer, delim); // ilk kelime bulunuyor

    while (word != NULL) {
        for (i = 0; word[i] != '\0'; i++) {
            if ((word[i] >= 'a' && word[i] <= 'z') || (word[i] >= 'A' && word[i] <= 'Z')) {
                (*letter_count)++; // harf sayac�
            }
        }

        (*word_count)++; // kelime sayac� 

        int found = 0;
        for (i = 0; i < word_index; i++) {
            if (strcmp(word, word_list[i]) == 0) {
                word_freq[i]++; // kelime bulunursa frekans art�r�l�yor
                found = 1;
                break;
            }
        }
        if (!found) {
            strcpy(word_list[word_index], word); // yeni kelimeyi listeye ekleme
            word_freq[word_index] = 1;
            word_index++;
        }

        word = strtok(NULL, delim); // sonraki kelimeye ge�
    }

    for (i = 0; i < word_index; i++) {
        if (word_freq[i] > *max_frequency) {
            *max_frequency = word_freq[i]; // en s�k kelimenin frekans�n� bulma
            strcpy(most_frequent_word, word_list[i]); // en s�k kelimeyi kaydetme
        }
    }
}

// istemciyi y�neten fonksiyon
void handle_client(void* client_socket_ptr) {
    SOCKET client_socket = *(SOCKET*)client_socket_ptr;
    free(client_socket_ptr); // dinamik bellek serbest b�rakma

    EnterCriticalSection(&client_count_lock); // kritik b�lgeye giri�
    client_count++;
    printf("New client connected. Current client count: %d\n", client_count);
    LeaveCriticalSection(&client_count_lock); // kritik b�lgeden ��k��

    char buffer[BUFFER_SIZE];
    int read_size;

    // Dosya yazma i�lemi i�in dosya pointer'�
    FILE *file = fopen("received_messages.txt", "a"); // 'a' modu: dosyaya ekleme
    if (file == NULL) {
        printf("Failed to open the file.\n");
        closesocket(client_socket);
        _endthread();
        return;
    }

    while ((read_size = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[read_size] = '\0'; // gelen veriyi null karakter ile sonland�rma

        // Dosyaya gelen veriyi yazma
        fprintf(file, "%s\n", buffer); // gelen mesaj� dosyaya yaz

        if (strcmp(buffer, "exit") == 0) {
            printf("Client disconnected.\n");
            break; // istemci ��k�� yaparsa d�ng�den ��kma
        }

        int letter_count = 0, word_count = 0, max_frequency = 0;
        char most_frequent_word[BUFFER_SIZE];

        count_word_frequency(buffer, &letter_count, &word_count, most_frequent_word, &max_frequency); // metin analizi yap�l�yor

        char response[BUFFER_SIZE];
        snprintf(response, BUFFER_SIZE, "Letter Count: %d, Word Count: %d, Most Frequent Word: '%s' (%d occurrences)\n", 
                 letter_count, word_count, most_frequent_word, max_frequency); // cevap haz�rlama
        send(client_socket, response, strlen(response), 0); // cevab� istemciye g�nderme
    }

    // Dosya kapama
    fclose(file);

    closesocket(client_socket); // istemci soketi kapatma

    EnterCriticalSection(&client_count_lock);
    client_count--; // istemci say�s� azaltma
    printf("Client exited. Current client count: %d\n", client_count);
    LeaveCriticalSection(&client_count_lock);

    _endthread(); // i� par�ac��� sonland�rma
}

int main() {
    WSADATA wsa;
    SOCKET server_socket, *client_socket;
    struct sockaddr_in server_addr, client_addr;
    int addr_len;

    InitializeCriticalSection(&client_count_lock); // kritik b�lge ba�latma

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed. Error code: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    server_socket = socket(AF_INET, SOCK_STREAM, 0); // sunucu soketi olu�turma
    if (server_socket == INVALID_SOCKET) {
        printf("Socket creation failed. Error code: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // t�m IP adreslerinden ba�lant� kabul edilir
    server_addr.sin_port = htons(PORT); // port numaras� ayarlan�yor

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed. Error code: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 3) == SOCKET_ERROR) {
        printf("Listen failed. Error code: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d. Waiting for connections...\n", PORT);

    while (1) {
        addr_len = sizeof(client_addr);
        client_socket = malloc(sizeof(SOCKET)); // istemci soketi i�in bellek ay�rma
        if ((*client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len)) == INVALID_SOCKET) {
            printf("Accept failed. Error code: %d\n", WSAGetLastError());
            free(client_socket); // kabul hatas� durumunda belle�i serbest b�rakma
            continue;
        }

        printf("New client connected.\n");
        _beginthread(handle_client, 0, client_socket); // yeni i� par�ac��� olu�turma
    }

    DeleteCriticalSection(&client_count_lock); // kritik b�lge temizleme
    closesocket(server_socket); // sunucu soketi kapatma
    WSACleanup(); // winsock temizleme

    return 0;
}

