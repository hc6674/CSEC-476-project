#include <iostream>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

const char* SERVER_IP = "127.0.0.1";
const int PORT = 8080;

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    // Create a socket for the client
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cerr << "Socket creation error" << endl;
        return 1;
    }

    // Set server address and port
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        cerr << "Invalid address/ Address not supported" << endl;
        return 1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "Connection failed" << endl;
        return 1;
    }

    while (true) {
        // Receive a message from the server
        int valread = read(sock, buffer, 1024);
        cout <<buffer[valread] = '\0';
    cout << buffer;

    // Get user input
    string input;
    getline(cin, input);

    // Send user input to the server
    send(sock, input.c_str(), input.length(), 0);

    // Receive a message from the server
    valread = read(sock, buffer, 1024);
    buffer[valread] = '\0';
    cout << buffer;

    // If the server is sending a file, receive it
    if (strncmp(buffer, "Sending", 7) == 0) {
        // Extract the filename
        string filename = buffer + 8;
        filename = filename.substr(0, filename.find(" to client"));

        // Receive the file size
        valread = read(sock, buffer, 1024);
        buffer[valread] = '\0';
        int filesize = stoi(buffer);

        // Receive the file data
        ofstream outfile(filename, ios::out | ios::binary);
        if (!outfile.is_open()) {
            cerr << "Could not create file" << endl;
        } else {
            int bytes_received = 0;
            while (bytes_received < filesize) {
                valread = read(sock, buffer, 1024);
                bytes_received += valread;
                outfile.write(buffer, valread);
            }
            outfile.close();
            cout << "File received" << endl;
        }
    }
}

close(sock);

return 0;
}