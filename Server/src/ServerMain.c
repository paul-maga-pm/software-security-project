#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include "shlwapi.h"
#include <ctype.h>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

void handleClientRequests(SOCKET clientSocket);

const char* CURRENT_WORKING_DIR;

int __cdecl main(int argc, char** argv) {

    CURRENT_WORKING_DIR = argv[0];
    printf(CURRENT_WORKING_DIR);

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
        

        // cleanup
        printf("Client disconnected\n");
        closesocket(ClientSocket);
    }
    
    WSACleanup();

    return 0;
}

int checkMetaChars(char* path) {
    for (; *path; path++) {
        if (!isalnum(*path) && *path != '_' && *path != '.' && *path != '/' && *path != ' ' && *path != '-') {
            return 0;
        }
    }
    return 1;
}


int getFullPath(char* partialPath, char* fullPathOutput) {
    if (_fullpath(fullPathOutput, partialPath, _MAX_PATH) != NULL) {
        return 1;
    }
    else {
        return 0;
    }
}


int checkTraversal(char* path) {
    if (!PathIsRelativeA(path)) {
        return 0;
    }

    char fullPath[_MAX_PATH];
    if (!getFullPath(path, fullPath)) {
        return 0;
    }

    char currentWorkingDirFullPath[_MAX_PATH];
    if (!getFullPath(".", currentWorkingDirFullPath)) {
        return 0;
    }

    for (int i = 0; currentWorkingDirFullPath[i]; i++) {
        if (fullPath[i] == NULL) {
            return 0;
        }

        if (currentWorkingDirFullPath[i] != fullPath[i]) {
            return 0;
        }
    }
    return 1;
}

int validatePath(char* path) {
    return checkMetaChars(path) && checkTraversal(path);
}

#define PATH_OK "PATH_OK"
#define PATH_ERR "PATH_ERR"
#define FILE_OPEN_OK "FILE_OPEN_OK"
#define FILE_OPEN_ERR "FILE_OPEN_ERR"

#define PATH_OK_SIZE strlen(PATH_OK)
#define PATH_ERR_SIZE strlen(PATH_ERR)
#define FILE_OPEN_OK_SIZE strlen(FILE_OPEN_OK)
#define FILE_OPEN_ERR_SIZE strlen(FILE_OPEN_ERR)

#define BUFFER_SIZE 1025

void handleClientRequests(SOCKET clientSocket) {

    char filePath[BUFFER_SIZE] = { 0 };

    int recvResult = recv(clientSocket, filePath, BUFFER_SIZE, 0);

    if (recvResult == SOCKET_ERROR) {
        printf("Error while receiving path: %d\n", WSAGetLastError());
        return;
    }

    printf("Path received = %s\n", filePath);

    int SUCCESS = 1;
    if (!validatePath(filePath)) {
        printf("Invalid path received!\n");
        SUCCESS = 0;
    }


    FILE* filePtr = NULL;

    if (!SUCCESS || fopen_s(&filePtr, filePath, "w") != 0) {
        printf("Couldn't open file %s\n", filePath);
        SUCCESS = 0;
    }



    while (1) {
        char buffer[BUFFER_SIZE] = { 0 };
        int recvResult = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
        if (recvResult == SOCKET_ERROR) {
            printf("Error while receiving buffer\n");
            break;
        }
        buffer[BUFFER_SIZE - 1] = 0;
        if (recvResult == 0) {
            printf("Finished receiving buffers\n");
            break;
        }

        printf("Received buffer size: %zu\nBuffer: %s\n", strlen(buffer), buffer);

        if (SUCCESS) {
            fwrite(buffer, sizeof(char), strlen(buffer), filePtr);
        }
    }

    int iResult = shutdown(clientSocket, SD_RECEIVE);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(clientSocket);
    }

    char requestResult[BUFFER_SIZE] = { 0 };
    if (SUCCESS) {
        fclose(filePtr);
        strncpy_s(requestResult, BUFFER_SIZE - 1,"SUCCESS", strlen("SUCCESS"));
    } else {
        strncpy_s(requestResult, BUFFER_SIZE - 1, "FAIL", strlen("FAIL"));
    }

    send(clientSocket, requestResult, strlen(requestResult), 0);
}