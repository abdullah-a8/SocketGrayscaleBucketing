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
const int PORT = 55000;       // Self-defined network port to connect client with server and vice versa.
const int WIDTH = 800;        // As we are taking an image of size 800x600 as our input, the width is constant and will never change.
const int HEIGHT = 600;       // The height of the input image (constant at 600 pixels).
const int CHANNELS = 3;       // Number of color channels in the image: red, green, and blue (RGB).
const int TOTAL_PIXELS = WIDTH * HEIGHT;  // Total number of pixels in the image: 800 x 600 = 480,000.
const int IMAGE_SIZE = TOTAL_PIXELS * CHANNELS;  // Total size of the RGB image in bytes: 480,000 x 3 = 1,440,000.
const int GRAY_SIZE = TOTAL_PIXELS;                // Total size of the grayscale image in bytes: 480,000.
const int BUCKETS = 8;        // Number of partitions (buckets) to divide the grayscale image.
const int BUCKET_SIZE = GRAY_SIZE / BUCKETS;         // Size of each bucket in bytes: 480,000 / 8 = 60,000.

// Function to create the server socket
int createServerSocket() 
{
    // Creating a TCP socket using IPv4 (AF_INET) and stream sockets (SOCK_STREAM).
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    { // Check if socket creation failed (socket() returns a negative value on error).
        perror("Socket creation failed"); // Print an error message with details from the system (using errno).
        exit(EXIT_FAILURE); // Exit the program with a failure status.
    }
    return server_fd; // Return the file descriptor representing the newly created server socket.
}

// Function to bind the socket to a port and listen for connections
void bindAndListen(int server_fd, int port) 
{
    int opt = 1; // Option value used to enable socket options.
    // Set socket options to allow reuse of the address and port, avoiding "Address already in use" errors.
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) 
    {
        perror("setsockopt failed"); // Print an error message if setsockopt fails.
        exit(EXIT_FAILURE); // Exit the program due to the error.
    }
    
    sockaddr_in address; // Structure to hold the server's address information.
    address.sin_family = AF_INET; // Specify the address family as IPv4.
    address.sin_addr.s_addr = INADDR_ANY; // Bind to all available network interfaces.
    address.sin_port = htons(port); // Convert the port number to network byte order (big-endian) using htons.
    
    // Bind the socket to the address and port specified in the 'address' structure.
    if (bind(server_fd, (sockaddr *)&address, sizeof(address)) < 0) 
    {
        perror("bind failed"); // Print an error message if binding fails.
        exit(EXIT_FAILURE); // Exit the program due to the error.
    }
    
    // Start listening for incoming connections; the second parameter defines the maximum queue length for pending connections.
    if (listen(server_fd, 3) < 0) 
    {
        perror("listen failed"); // Print an error message if listen fails.
        exit(EXIT_FAILURE); // Exit the program due to the error.
    }
    
    cout << "Server listening on port " << port << endl; // Inform the user that the server is now listening.
}

// Function to accept an incoming connection
int acceptClient(int server_fd) 
{
    sockaddr_in address; // Structure to store the client's address information.
    socklen_t addrlen = sizeof(address); // The size of the address structure.
    // Accept a new connection from a client; a new socket is created for this client.
    int client_sock = accept(server_fd, (sockaddr *)&address, &addrlen);
    if (client_sock < 0) 
    { // Check if accept() failed.
        perror("accept failed"); // Print an error message if accepting the connection fails.
        exit(EXIT_FAILURE); // Exit the program due to the error.
    }
    cout << "Client connected." << endl; // Notify that a client has successfully connected.
    return client_sock; // Return the client's socket file descriptor.
}

// Function to receive the raw RGB image data from the client
bool receiveImageData(int sock, vector<unsigned char> &image) 
{
    image.resize(IMAGE_SIZE); // Resize the vector to hold the complete image data.
    int total_received = 0; // Counter for the total number of bytes received.
    // Loop until the entire image data (IMAGE_SIZE bytes) is received.
    while (total_received < IMAGE_SIZE) 
    {
        // Receive data from the client and store it in the vector at the correct offset.
        int bytes = recv(sock, image.data() + total_received, IMAGE_SIZE - total_received, 0);
        if (bytes <= 0) 
        { // If no bytes are received or an error occurs...
            cerr << "Failed to receive image data." << endl; // Print an error message to the standard error stream.
            return false; // Return false to indicate failure in receiving the image data.
        }
        total_received += bytes; // Update the total number of bytes received.
    }
    cout << "Received image (" << total_received << " bytes)." << endl; // Log the total bytes received.
    return true; // Return true indicating the image data was received successfully.
}

// Function to convert an RGB image to grayscale using the average method.
vector<unsigned char> convertToGrayscale(const vector<unsigned char> &image) 
{
    vector<unsigned char> gray(GRAY_SIZE); // Create a vector to store the grayscale image.
    // Process each pixel: each pixel in the RGB image consists of three consecutive bytes (R, G, B).
    for (int i = 0; i < TOTAL_PIXELS; i++) 
    {
        int r = image[3 * i];       // Extract the red component.
        int g = image[3 * i + 1];   // Extract the green component.
        int b = image[3 * i + 2];   // Extract the blue component.
        // Compute the average value of R, G, and B to convert to a single grayscale value.
        gray[i] = static_cast<unsigned char>((r + g + b) / 3);
    }
    return gray; // Return the grayscale image vector.
}

// Function to partition the grayscale image into 8 equal buckets
vector<vector<unsigned char>> partitionIntoBuckets(const vector<unsigned char> &gray) 
{
    // Create a 2D vector (vector of vectors) where each sub-vector represents a bucket of BUCKET_SIZE bytes.
    vector<vector<unsigned char>> buckets(BUCKETS, vector<unsigned char>(BUCKET_SIZE));
    // For each bucket, copy a segment of the grayscale image into it.
    for (int i = 0; i < BUCKETS; i++) {
        // Use the standard algorithm 'copy' to move BUCKET_SIZE elements from the grayscale vector into the current bucket.
        copy(gray.begin() + i * BUCKET_SIZE, gray.begin() + (i + 1) * BUCKET_SIZE, buckets[i].begin());
    }
    return buckets; // Return the 2D vector containing all the buckets.
}

// Function to print a summary of the grayscale buckets (first 10 values per bucket)
void printBucketsSummary(const vector<vector<unsigned char>> &buckets) 
{
    cout << "Grayscale image data partitioned into 8 buckets:" << endl; // Header message for clarity.
    // Loop through each bucket to print a summary.
    for (int i = 0; i < BUCKETS; i++) 
    {
        cout << "Bucket " << i + 1 << ": "; // Print the bucket number (using i+1 for a human-friendly count).
        // Print the first 10 grayscale values from each bucket (or less if the bucket has fewer than 10 values).
        for (int j = 0; j < 10 && j < BUCKET_SIZE; j++) 
        {
            cout << static_cast<int>(buckets[i][j]) << " "; // Convert the unsigned char to an int for clear numeric output.
        }
        cout << "..." << endl; // Indicate that the bucket contains more data.
    }
}

// Function to send each bucket of data back to the client
bool sendBucketsData(int sock, const vector<vector<unsigned char>> &buckets) 
{
    // Loop through all buckets to send them one by one.
    for (int i = 0; i < BUCKETS; i++) 
    {
        // Send the current bucket's data over the network using the send() function.
        ssize_t bytes_sent = send(sock, buckets[i].data(), BUCKET_SIZE, 0);
        if (bytes_sent < 0) 
        { // Check if the send() operation failed.
            perror("send failed"); // Print an error message using perror.
            return false; // Return false to indicate the failure to send data.
        }
        cout << "Sent bucket " << i + 1 << " (" << bytes_sent << " bytes)." << endl; // Log the successful transmission.
    }
    return true; // Return true if all buckets were sent successfully.
}

int main() 
{
    // Step 1: Create and configure the server socket.
    int server_fd = createServerSocket(); // Create a new server socket.
    bindAndListen(server_fd, PORT);         // Bind the socket to a port and set it to listen for incoming connections.

    // Step 2: Accept a client connection.
    int client_sock = acceptClient(server_fd); // Accept an incoming connection from a client.

    // Step 3: Receive the raw RGB image data from the client.
    vector<unsigned char> image; // Declare a vector to hold the received image data.
    if (!receiveImageData(client_sock, image)) 
    { // Attempt to receive the image data; check for success.
        close(client_sock); // Close the client socket if reception failed.
        close(server_fd);   // Close the server socket.
        return -1;          // Terminate the program with an error code.
    }

    // Step 4: Convert the RGB image to grayscale.
    vector<unsigned char> gray = convertToGrayscale(image); // Process the image to convert it from RGB to grayscale.

    // Step 5: Partition the grayscale image into 8 buckets.
    vector<vector<unsigned char>> buckets = partitionIntoBuckets(gray); // Divide the grayscale image into 8 equal parts (buckets).

    // Step 6: Display a summary of each bucket.
    printBucketsSummary(buckets); // Print a brief summary (first 10 values) of each bucket to the console.

    // Step 7: Send each bucket back to the client.
    if (!sendBucketsData(client_sock, buckets)) 
    { // Send the partitioned buckets back to the client; check for errors.
        close(client_sock); // Close the client socket if sending failed.
        close(server_fd);   // Close the server socket.
        return -1;          // Terminate the program with an error code.
    }

    // Close the connection.
    close(client_sock); // Close the connection with the client.
    close(server_fd);   // Close the server socket.
    cout << "Processing complete. Connection closed." << endl; // Inform that processing is finished and the connection is closed.
    return 0; // End the program successfully.
}