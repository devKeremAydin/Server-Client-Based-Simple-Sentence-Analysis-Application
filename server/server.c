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

// kelime ile ilgili hesaplamalarý yapan fonksiyon
void count_word_frequency(char *buffer, int *letter_count, int *word_count, char most_frequent_word[BUFFER_SIZE], int *max_frequency) {
    char *word;
    char *delim = " \t\n\r"; // kelimeleri ayýrmak için kullanýlan ayýrýcýlar
    char temp_buffer[BUFFER_SIZE];
    char word_list[100][BUFFER_SIZE]; // farklý kelimeleri saklamak için dizi
    int word_freq[100] = {0}; // kelimelerin frekanslarýný tutmak için dizi
    int word_index = 0;
    int i;

    *letter_count = 0;
    *word_count = 0;
    *max_frequency = 0;
    most_frequent_word[0] = '\0'; // en sýk kelime için baþlangýç deðeri

    strcpy(temp_buffer, buffer); 
    word = strtok(temp_buffer, delim); // ilk kelime bulunuyor

    while (word != NULL) {
        for (i = 0; word[i] != '\0'; i++) {
            if ((word[i] >= 'a' && word[i] <= 'z') || (word[i] >= 'A' && word[i] <= 'Z')) {
                (*letter_count)++; // harf sayacý
            }
        }

        (*word_count)++; // kelime sayacý 

        int found = 0;
        for (i = 0; i < word_index; i++) {
            if (strcmp(word, word_list[i]) == 0) {
                word_freq[i]++; // kelime bulunursa frekans artýrýlýyor
                found = 1;
                break;
            }
        }
        if (!found) {
            strcpy(word_list[word_index], word); // yeni kelimeyi listeye ekleme
            word_freq[word_index] = 1;
            word_index++;
        }

        word = strtok(NULL, delim); // sonraki kelimeye geç
    }

    for (i = 0; i < word_index; i++) {
        if (word_freq[i] > *max_frequency) {
            *max_frequency = word_freq[i]; // en sýk kelimenin frekansýný bulma
            strcpy(most_frequent_word, word_list[i]); // en sýk kelimeyi kaydetme
        }
    }
}

// istemciyi yöneten fonksiyon
void handle_client(void* client_socket_ptr) {
    SOCKET client_socket = *(SOCKET*)client_socket_ptr;
    free(client_socket_ptr); // dinamik bellek serbest býrakma

    EnterCriticalSection(&client_count_lock); // kritik bölgeye giriþ
    client_count++;
    printf("New client connected. Current client count: %d\n", client_count);
    LeaveCriticalSection(&client_count_lock); // kritik bölgeden çýkýþ

    char buffer[BUFFER_SIZE];
    int read_size;

    // Dosya yazma iþlemi için dosya pointer'ý
    FILE *file = fopen("received_messages.txt", "a"); // 'a' modu: dosyaya ekleme
    if (file == NULL) {
        printf("Failed to open the file.\n");
        closesocket(client_socket);
        _endthread();
        return;
    }

    while ((read_size = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[read_size] = '\0'; // gelen veriyi null karakter ile sonlandýrma

        // Dosyaya gelen veriyi yazma
        fprintf(file, "%s\n", buffer); // gelen mesajý dosyaya yaz

        if (strcmp(buffer, "exit") == 0) {
            printf("Client disconnected.\n");
            break; // istemci çýkýþ yaparsa döngüden çýkma
        }

        int letter_count = 0, word_count = 0, max_frequency = 0;
        char most_frequent_word[BUFFER_SIZE];

        count_word_frequency(buffer, &letter_count, &word_count, most_frequent_word, &max_frequency); // metin analizi yapýlýyor

        char response[BUFFER_SIZE];
        snprintf(response, BUFFER_SIZE, "Letter Count: %d, Word Count: %d, Most Frequent Word: '%s' (%d occurrences)\n", 
                 letter_count, word_count, most_frequent_word, max_frequency); // cevap hazýrlama
        send(client_socket, response, strlen(response), 0); // cevabý istemciye gönderme
    }

    // Dosya kapama
    fclose(file);

    closesocket(client_socket); // istemci soketi kapatma

    EnterCriticalSection(&client_count_lock);
    client_count--; // istemci sayýsý azaltma
    printf("Client exited. Current client count: %d\n", client_count);
    LeaveCriticalSection(&client_count_lock);

    _endthread(); // iþ parçacýðý sonlandýrma
}

int main() {
    WSADATA wsa;
    SOCKET server_socket, *client_socket;
    struct sockaddr_in server_addr, client_addr;
    int addr_len;

    InitializeCriticalSection(&client_count_lock); // kritik bölge baþlatma

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed. Error code: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    server_socket = socket(AF_INET, SOCK_STREAM, 0); // sunucu soketi oluþturma
    if (server_socket == INVALID_SOCKET) {
        printf("Socket creation failed. Error code: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // tüm IP adreslerinden baðlantý kabul edilir
    server_addr.sin_port = htons(PORT); // port numarasý ayarlanýyor

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
        client_socket = malloc(sizeof(SOCKET)); // istemci soketi için bellek ayýrma
        if ((*client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len)) == INVALID_SOCKET) {
            printf("Accept failed. Error code: %d\n", WSAGetLastError());
            free(client_socket); // kabul hatasý durumunda belleði serbest býrakma
            continue;
        }

        printf("New client connected.\n");
        _beginthread(handle_client, 0, client_socket); // yeni iþ parçacýðý oluþturma
    }

    DeleteCriticalSection(&client_count_lock); // kritik bölge temizleme
    closesocket(server_socket); // sunucu soketi kapatma
    WSACleanup(); // winsock temizleme

    return 0;
}

