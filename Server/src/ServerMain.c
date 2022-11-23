#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

void handleClientRequests(SOCKET clientSocket);

int __cdecl main(void)
{
    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo* result = NULL;
    struct addrinfo hints;


    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for the server to listen for client connections.
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

 
    printf("Listening on port %s\n", DEFAULT_PORT);
    while (1) {
        // accept client connection
        ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) {
            printf("accept failed with error: %d\n", WSAGetLastError());
            closesocket(ListenSocket);
            continue;
        }

        printf("Client connected\n");
        handleClientRequests(ClientSocket);
     
        // shutdown the connection since we're done
        iResult = shutdown(ClientSocket, SD_SEND);
        if (iResult == SOCKET_ERROR) {
            printf("shutdown failed with error: %d\n", WSAGetLastError());
            closesocket(ClientSocket);
        }

        // cleanup
        printf("Client disconnected\n");
        closesocket(ClientSocket);
    }
    
    WSACleanup();

    return 0;
}



void handleClientRequests(SOCKET clientSocket) {
    
    const int BUFFER_SIZE = 1025;
    char filePath[1025] = { 0 };

    int recvResult = recv(clientSocket, filePath, BUFFER_SIZE, 0);

    if (recvResult == SOCKET_ERROR) {
        printf("Error while receiving path: %d\n", WSAGetLastError());
        return;
    }
    
    printf("Path received = %s\n", filePath);


    FILE* filePtr = NULL;

    if (fopen_s(&filePtr, filePath, "w") != 0) {
        printf("Couldn't open file %s\n", filePath);
        return;
    }

    char buffer[1025] = { 0 };
    while (recv(clientSocket, buffer, BUFFER_SIZE, 0) > 0) {
        printf("Received Buffer:\n%s\n", buffer);

        if (fwrite(buffer, 1, strlen(buffer), filePtr) < strlen(buffer)) {
            printf("Eror while writting to file");
            fclose(filePtr);
            return;
        }
    }

    fclose(filePtr);
}