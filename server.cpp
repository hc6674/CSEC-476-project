#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <netinet/if_ether.h>
#include <sys/types.h>
#include <pwd.h>

using namespace std;

const int PORT = 8080;

// Function to get the client's IP address
string get_client_ip(int client_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    if (getpeername(client_fd, (struct sockaddr *)&client_addr, &client_addr_len) < 0) {
        cerr << "Getpeername error" << endl;
        return "";
    }
    return inet_ntoa(client_addr.sin_addr);
}

// Function to get the client's MAC address
string get_client_mac(int client_fd) {
    struct sockaddr_ll client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    if (getpeername(client_fd, (struct sockaddr *)&client_addr, &client_addr_len) < 0) {
        cerr << "Getpeername error" << endl;
        return "";
    }
    char mac[18];
    sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
            client_addr.sll_addr[0], client_addr.sll_addr[1], client_addr.sll_addr[2],
            client_addr.sll_addr[3], client_addr.sll_addr[4], client_addr.sll_addr[5]);
    return string(mac);
}

// Function to get the client's username
string get_client_username() {
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    if (pw == NULL) {
        cerr << "Getpwuid error" << endl;
        return "";
    }
    return string(pw->pw_name);
}

// Function to get the client's operating system
string get_client_os() {
    int mib[2];
    mib[0] = CTL_KERN;
    mib[1] = KERN_OSRELEASE;
    char os_release[256];
    size_t len = sizeof(os_release);
    if (sysctl(mib, 2, os_release, &len, NULL, 0) < 0) {
        cerr << "Sysctl error" << endl;
        return "";
    }
    return string(os_release);
}

// Function to list out currently running processes
string list_processes() {
    string output = "";
    FILE *pipe = popen("ps -ax", "r");
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }
    pclose(pipe);
    return output;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    int opt = 1;
    int addrlen = sizeof(server_addr);
    char buffer[1024] = {0};
    string filename;
    ofstream outfile;

    // Create a socket for the server
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        cerr << "Socket creation error" << endl;
        return 1;
    }

    // Set socket options
   
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        cerr << "Setsockopt error" << endl;
        return 1;
    }

    // Bind the socket to a port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Bind error" << endl;
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        cerr << "Listen error" << endl;
        return 1;
    }

    while (true) {
        // Accept a client connection
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t*)&addrlen)) < 0) {
            cerr << "Accept error" << endl;
            return 1;
        }

        // Send a message to the client
        string message = "Choose an action:\n1. List running processes\n2. Upload/download a file\n3. Send client information\n";
        send(client_fd, message.c_str(), message.length(), 0);

        // Receive the client's choice
        int valread = read(client_fd, buffer, 1024);
        char choice = buffer[0];

        // List out currently running processes
        if (choice == '1') {
            message = list_processes();
            send(client_fd, message.c_str(), message.length(), 0);
        }
        // Upload or download a file
        else if (choice == '2') {
            // Prompt the client for the action
            message = "Choose an action:\n1. Upload a file\n2. Download a file\n";
            send(client_fd, message.c_str(), message.length(), 0);

            // Receive the client's choice
            valread = read(client_fd, buffer, 1024);
            char file_choice = buffer[0];

            // Prompt the client for the filename
            message = "Enter filename: ";
            send(client_fd, message.c_str(), message.length(), 0);

            // Receive the filename from the client
            valread = read(client_fd, buffer, 1024);
            buffer[valread] = '\0';
            filename = buffer;

            // Upload a file
            if (file_choice == '1') {
                message = "Receiving " + filename + " from client";
                send(client_fd, message.c_str(), message.length(), 0);
                outfile.open(filename, ios::out | ios::binary);
                if (!outfile.is_open()) {
                    message = "Could not create file";
                    send(client_fd, message.c_str(), message.length(), 0);
                } else {
                    valread = read(client_fd, buffer, 1024);
                    int filesize = stoi(buffer);
                    message = "File size: " + to_string(filesize) + " bytes";
                    send(client_fd, message.c_str(), message.length(), 0);
                    int bytes_received = 0;
                    while (bytes_received < filesize) {
                        valread = read(client_fd, buffer, 1024);
                        bytes_received += valread;
                        outfile.write(buffer, valread);
                    }
                    message = "File received";
                    send(client_fd, message.c_str(), message.length(), 0);
                    outfile.close();
                }
            }
            // Download a file
            else if (file_choice == '2') {
                ifstream infile(filename);
                if (!infile.good()) {
                
                    message = "File not found";
                    send(client_fd, message.c_str(), message.length(), 0);
                } else {
                    message = "Sending " + filename + " to client";
                    send(client_fd, message.c_str(), message.length(), 0);
                    infile.seekg(0, ios::end);
                    int filesize = infile.tellg();
                    infile.seekg(0, ios::beg);
                    message = to_string(filesize);
                    send(client_fd, message.c_str(), message.length(), 0);
                    char filedata[1024] = {0};
                    while (!infile.eof()) {
                        infile.read(filedata, 1024);
                        send(client_fd, filedata, infile.gcount(), 0);
                    }
                    infile.close();
                }
            }
            // Invalid choice
            else {
                message = "Invalid choice";
                send(client_fd, message.c_str(), message.length(), 0);
            }
        }
        // Send client information to the server
        else if (choice == '3') {
            string ip_address = get_client_ip(client_fd);
            string mac_address = get_client_mac(client_fd);
            string username = get_client_username();
            string os = get_client_os();
            message = "IP address: " + ip_address + "\nMAC address: " + mac_address + "\nUsername: " + username + "\nOperating System: " + os;
            send(client_fd, message.c_str(), message.length(), 0);
        }
        // Invalid choice
        else {
            message = "Invalid choice";
            send(client_fd, message.c_str(), message.length(), 0);
        }

        // Close the client socket
        close(client_fd);
    }

    // Close the server socket
    close(server_fd);

    return 0;
}