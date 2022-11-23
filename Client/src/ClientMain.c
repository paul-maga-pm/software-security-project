#define WIN32_LEAN_AND_MEAN
#define __STDC_LIB_EXT1__

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_READ_BUFLEN 4
#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

void send_to_server(SOCKET* ConnectSocket, char* string_to_send) {
    int iResult = send(ConnectSocket, string_to_send, (int)strlen(string_to_send), 0);
    if (iResult == SOCKET_ERROR) {
        printf("Send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        exit(1);
    }
}

void get_full_path(char* partialPath, char* full)
{
    char full_path[_MAX_PATH];
    if (_fullpath(full, partialPath, _MAX_PATH) != NULL) {
        printf("%s %s\n", partialPath, full);
    }
    else {
        printf("Invalid path\n");
        exit(1);
    }
}


int __cdecl main(int argc, char** argv) {
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;
    const char* sendbuf = "this is a test";
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;

    // Validate the parameters
    if (argc != 2) {
        printf("usage: %s server-name\n", argv[0]);
        return 1;
    }

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(argv[1], DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }
    // Here we have an active connection so we can start sending datas

    char path[1025] = { 0 };
    printf("Give me the path: ");
    if (scanf_s("%s", path, (unsigned int)_countof(path)) <= 0) {
        printf("Error when reading the path!");
        return 1;
    }

    if (check_path(path) == 0) {
        printf("Error! The introduced path is not safe!");
        return 1;
    }

    char new_path[1025] = { 0 };
    printf("Give me the path where you want to save the content: ");

    if (scanf_s("%s", new_path, (unsigned int)_countof(new_path)) <= 0) {
        printf("Error when reading the path!");
        return 1;
    }

    send_to_server(ConnectSocket, new_path);

    FILE* file = NULL;

    if (fopen_s(&file, path, "r") != 0) {
        printf("could not open file!");
        return 1;
    }
    
    char file_buff[DEFAULT_READ_BUFLEN];
    
    while (fgets(file_buff, DEFAULT_READ_BUFLEN, file)) {
        printf("Now sending: %s\n", file_buff);
        file_buff[DEFAULT_READ_BUFLEN - 1] = 0;
        send_to_server(ConnectSocket, file_buff);
    }

    iResult = shutdown(ConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    char requestResult[DEFAULT_BUFLEN] = { 0 };
    recv(ConnectSocket, requestResult, strlen(requestResult), 0);

    printf(requestResult);


    //// Receive until the peer closes the connection
    //do {

    //    iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
    //    if (iResult > 0)
    //        printf("Bytes received: %d\n", iResult);
    //    else if (iResult == 0)
    //        printf("Connection closed\n");
    //    else
    //        printf("recv failed with error: %d\n", WSAGetLastError());

    //} while (iResult > 0);

    // cleanup
   //closesocket(ConnectSocket);
   // WSACleanup();

    return 0;
}

int check_path_traversal(char* path) {
    char full_path[_MAX_PATH];
    get_full_path(path, full_path);

    char current_path[_MAX_PATH];
    get_full_path(".", current_path);

    for (int i = 0; current_path[i]; i++) {
        if (full_path[i] == NULL) {
            return 0;
        }

        if (current_path[i] != full_path[i]) {
            return 0;
        }
    }
    return 1;
}

int check_metacharacters(char* path) {
    for (; *path; path++) {
        if (!isalnum(*path) && *path != '_' && *path != '.' && *path != '/' && *path != ' ' && *path != '-') {
            return 0;
        }
    }
    return 1;
}

int check_path(char* path) {
    return check_metacharacters(path) && check_path_traversal(path);
}

