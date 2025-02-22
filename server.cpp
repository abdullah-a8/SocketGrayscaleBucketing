#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

using namespace std;

// Image and network constants
const int PORT = 55000;
const int WIDTH = 800;
const int HEIGHT = 600;
const int CHANNELS = 3;
const int TOTAL_PIXELS = WIDTH * HEIGHT;
const int IMAGE_SIZE = TOTAL_PIXELS * CHANNELS;  // 1,440,000 bytes (RGB)
const int GRAY_SIZE = TOTAL_PIXELS;                // 480,000 bytes (grayscale)
const int BUCKETS = 8;
const int BUCKET_SIZE = GRAY_SIZE / BUCKETS;         // 60,000 bytes

// Function to create the server socket
int createServerSocket() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    return server_fd;
}

// Function to bind the socket to a port and listen for connections
void bindAndListen(int server_fd, int port) {
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Accept connections from any interface
    address.sin_port = htons(port);
    
    if (bind(server_fd, (sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    
    cout << "Server listening on port " << port << endl;
}

// Function to accept an incoming connection
int acceptClient(int server_fd) {
    sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int client_sock = accept(server_fd, (sockaddr *)&address, &addrlen);
    if (client_sock < 0) {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }
    cout << "Client connected." << endl;
    return client_sock;
}

// Function to receive the raw RGB image data from the client
bool receiveImageData(int sock, vector<unsigned char> &image) {
    image.resize(IMAGE_SIZE);
    int total_received = 0;
    while (total_received < IMAGE_SIZE) {
        int bytes = recv(sock, image.data() + total_received, IMAGE_SIZE - total_received, 0);
        if (bytes <= 0) {
            cerr << "Failed to receive image data." << endl;
            return false;
        }
        total_received += bytes;
    }
    cout << "Received image (" << total_received << " bytes)." << endl;
    return true;
}

// Function to convert an RGB image to grayscale using the average method.
vector<unsigned char> convertToGrayscale(const vector<unsigned char> &image) {
    vector<unsigned char> gray(GRAY_SIZE);
    for (int i = 0; i < TOTAL_PIXELS; i++) {
        int r = image[3 * i];
        int g = image[3 * i + 1];
        int b = image[3 * i + 2];
        gray[i] = static_cast<unsigned char>((r + g + b) / 3);
    }
    return gray;
}

// Function to partition the grayscale image into 8 equal buckets
vector<vector<unsigned char>> partitionIntoBuckets(const vector<unsigned char> &gray) {
    vector<vector<unsigned char>> buckets(BUCKETS, vector<unsigned char>(BUCKET_SIZE));
    for (int i = 0; i < BUCKETS; i++) {
        copy(gray.begin() + i * BUCKET_SIZE, gray.begin() + (i + 1) * BUCKET_SIZE, buckets[i].begin());
    }
    return buckets;
}

// Function to print a summary of the grayscale buckets (first 10 values per bucket)
void printBucketsSummary(const vector<vector<unsigned char>> &buckets) {
    cout << "Grayscale image data partitioned into 8 buckets:" << endl;
    for (int i = 0; i < BUCKETS; i++) {
        cout << "Bucket " << i + 1 << ": ";
        for (int j = 0; j < 10 && j < BUCKET_SIZE; j++) {
            cout << static_cast<int>(buckets[i][j]) << " ";
        }
        cout << "..." << endl;
    }
}

// Function to send each bucket of data back to the client
bool sendBucketsData(int sock, const vector<vector<unsigned char>> &buckets) {
    for (int i = 0; i < BUCKETS; i++) {
        ssize_t bytes_sent = send(sock, buckets[i].data(), BUCKET_SIZE, 0);
        if (bytes_sent < 0) {
            perror("send failed");
            return false;
        }
        cout << "Sent bucket " << i + 1 << " (" << bytes_sent << " bytes)." << endl;
    }
    return true;
}

int main() {
    // Step 1: Create and configure the server socket.
    int server_fd = createServerSocket();
    bindAndListen(server_fd, PORT);

    // Step 2: Accept a client connection.
    int client_sock = acceptClient(server_fd);

    // Step 3: Receive the raw RGB image data from the client.
    vector<unsigned char> image;
    if (!receiveImageData(client_sock, image)) {
        close(client_sock);
        close(server_fd);
        return -1;
    }

    // Step 4: Convert the RGB image to grayscale.
    vector<unsigned char> gray = convertToGrayscale(image);

    // Step 5: Partition the grayscale image into 8 buckets.
    vector<vector<unsigned char>> buckets = partitionIntoBuckets(gray);

    // Step 6: Display a summary of each bucket.
    printBucketsSummary(buckets);

    // Step 7: Send each bucket back to the client.
    if (!sendBucketsData(client_sock, buckets)) {
        close(client_sock);
        close(server_fd);
        return -1;
    }

    // Close the connection.
    close(client_sock);
    close(server_fd);
    cout << "Processing complete. Connection closed." << endl;
    return 0;
}