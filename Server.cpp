#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <tchar.h>
#include <thread>
#include <vector>
#include <mutex>

using namespace std;

#pragma comment(lib, "ws2_32.lib")

bool initialize() {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}

void InteractWithClient(SOCKET clientSocket, vector<SOCKET>& clients, mutex& mtx) {
    char buffer[4096];
    string message;
    while (true) {
        int bytesrecvd = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesrecvd > 0) {
            message = string(buffer, bytesrecvd);
            cout << "Message from client: " << message << endl;

            // Broadcast message to all other clients
            lock_guard<mutex> lock(mtx);
            for (auto client : clients) {
                if (client != clientSocket) {
                    send(client, message.c_str(), message.length(), 0);
                }
            }
        }
        else {
            cout << "Client disconnected or receiving message failed" << endl;
            break;
        }
    }

    // Remove the client from the list and close the socket
    {
        lock_guard<mutex> lock(mtx);
        auto it = find(clients.begin(), clients.end(), clientSocket);
        if (it != clients.end()) {
            clients.erase(it);
        }
    }
    closesocket(clientSocket);
}

int main() {
    if (!initialize()) {
        cout << "Winsock initialization failed!" << endl;
        return 1;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET) {
        cout << "Socket creation failed" << endl;
        WSACleanup();
        return 1;
    }

    int port = 12345;
    sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    InetPton(AF_INET, _T("0.0.0.0"), &serveraddr.sin_addr);

    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&serveraddr), sizeof(serveraddr)) == SOCKET_ERROR) {
        cout << "Bind failed!" << endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        cout << "Listen failed!" << endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    cout << "Server has started listening on port: " << port << endl;
    vector<SOCKET> clients;
    mutex mtx;

    while (true) {
        SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            cout << "Invalid client socket" << endl;
            closesocket(listenSocket);
            WSACleanup();
            return 1;
        }
        {
            lock_guard<mutex> lock(mtx);
            clients.push_back(clientSocket);
        }
        thread t1(InteractWithClient, clientSocket, ref(clients), ref(mtx));
        t1.detach();
    }

    closesocket(listenSocket);
    WSACleanup();
    return 0;
}
