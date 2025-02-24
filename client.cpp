#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

// Server configuration
const char* SERVER_IP = "192.168.100.XX";  // Hard-coded server IP address to connect to our server.
const int PORT = 55000;                    // Network port for communication.

// Image constants
const int WIDTH = 800;                     // Fixed width of the input image in pixels.
const int HEIGHT = 600;                    // Fixed height of the input image in pixels.
const int CHANNELS = 3;                    // Number of color channels in the image (RGB).
const int TOTAL_PIXELS = WIDTH * HEIGHT;   // Total number of pixels in the image (800 * 600 = 480,000).
const int IMAGE_SIZE = TOTAL_PIXELS * CHANNELS; // Total size of the raw RGB image (480,000 * 3 = 1,440,000 bytes).
const int GRAY_SIZE = TOTAL_PIXELS;        // Total size of the grayscale image (one byte per pixel: 480,000 bytes).
const int BUCKETS = 8;                     // Number of equal partitions (buckets) to split the grayscale image.
const int BUCKET_SIZE = GRAY_SIZE / BUCKETS; // Size of each bucket (480,000 / 8 = 60,000 bytes).

// Function to convert the input image (jpg/png/etc.) to raw RGB binary using ImageMagick.
// This function uses ImageMagick's 'magick' command to convert the image and outputs "temp_input.bin".
bool convertImageToRaw(const string &inputImagePath) {
    // Build the command string to convert the image.
    string command = "magick " + inputImagePath + " -resize 800x600 -depth 8 -colorspace RGB RGB:temp_input.bin";
    cout << "Converting input image to raw binary format using ImageMagick..." << endl;
    // Execute the command using the system call.
    int ret = system(command.c_str());
    if (ret != 0) { // If the command fails, print an error message.
        cerr << "Image conversion failed. Ensure ImageMagick is installed on Termux." << endl;
        return false; // Return false to indicate failure.
    }
    return true; // Conversion succeeded.
}

// Function to load the raw image from a file into a vector.
// This function expects the file size to match IMAGE_SIZE.
bool loadRawImage(const string &filename, vector<unsigned char> &image) {
    // Open the file in binary mode and position the pointer at the end (ios::ate) to get the file size.
    ifstream file(filename, ios::binary | ios::ate);
    if (!file.is_open()) { // Check if the file was opened successfully.
        cerr << "Failed to open the raw image file (" << filename << ")." << endl;
        return false;
    }
    // Get the size of the file.
    streamsize fileSize = file.tellg();
    file.seekg(0, ios::beg); // Move the pointer back to the beginning for reading.
    if (fileSize != IMAGE_SIZE) { // Validate the file size against expected IMAGE_SIZE.
        cerr << "Raw image file size mismatch. Expected " << IMAGE_SIZE
             << " bytes, but got " << fileSize << " bytes." << endl;
        return false;
    }
    image.resize(IMAGE_SIZE); // Resize the vector to hold the image data.
    // Read the file content into the vector.
    if (!file.read(reinterpret_cast<char*>(image.data()), IMAGE_SIZE)) {
        cerr << "Failed to read raw image data from " << filename << endl;
        return false;
    }
    file.close(); // Close the file after reading.
    cout << "Loaded raw image from '" << filename << "' (" << IMAGE_SIZE << " bytes)." << endl;
    return true; // Successfully loaded the raw image.
}

// Function to create a socket and connect to the server.
int connectToServer(const char* serverIP, int port) {
    // Create a TCP socket using IPv4.
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { // Check if socket creation failed.
        cerr << "Socket creation error." << endl;
        return -1;
    }
    sockaddr_in serv_addr;             // Structure to hold the server's address.
    serv_addr.sin_family = AF_INET;    // Set the address family to IPv4.
    serv_addr.sin_port = htons(port);  // Convert port number to network byte order.
    // Convert the server IP from text to binary form.
    if (inet_pton(AF_INET, serverIP, &serv_addr.sin_addr) <= 0) {
        cerr << "Invalid server address." << endl;
        return -1;
    }
    // Attempt to connect to the server using the specified IP and port.
    if (connect(sock, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "Connection to server failed." << endl;
        return -1;
    }
    cout << "Connected to server " << serverIP << " on port " << port << endl;
    return sock; // Return the connected socket descriptor.
}

// Function to send the raw image data over the socket.
bool sendImageData(int sock, const vector<unsigned char>& image) {
    int total_sent = 0; // Counter for the total number of bytes sent.
    // Loop until the entire image data is sent.
    while (total_sent < IMAGE_SIZE) {
        int bytes = send(sock, image.data() + total_sent, IMAGE_SIZE - total_sent, 0);
        if (bytes < 0) { // If send fails, print an error and return false.
            cerr << "Failed to send image data." << endl;
            return false;
        }
        total_sent += bytes; // Update total bytes sent.
    }
    cout << "Sent raw image (" << total_sent << " bytes) to server." << endl;
    return true; // Successfully sent all image data.
}

// Function to receive 8 buckets of processed (grayscale) data from the server.
bool receiveBuckets(int sock, vector<vector<unsigned char>> &buckets) {
    // Resize the buckets vector to hold 8 buckets, each of BUCKET_SIZE bytes.
    buckets.resize(BUCKETS, vector<unsigned char>(BUCKET_SIZE));
    // Loop over each bucket to receive its data.
    for (int i = 0; i < BUCKETS; i++) {
        int total_received = 0; // Counter for bytes received per bucket.
        // Continue receiving until the entire bucket is filled.
        while (total_received < BUCKET_SIZE) {
            int bytes = recv(sock, buckets[i].data() + total_received, BUCKET_SIZE - total_received, 0);
            if (bytes <= 0) { // If recv fails, print an error message for the specific bucket.
                cerr << "Failed to receive data for bucket " << i + 1 << endl;
                return false;
            }
            total_received += bytes; // Update total received bytes for current bucket.
        }
        cout << "Received bucket " << i + 1 << " (" << total_received << " bytes)." << endl;
    }
    return true; // All buckets received successfully.
}

// Function to print a summary (first 10 values) of each bucket.
void printBucketsSummary(const vector<vector<unsigned char>> &buckets) {
    cout << "Received grayscale buckets data:" << endl;
    // Loop through each bucket and print the first 10 grayscale values.
    for (int i = 0; i < BUCKETS; i++) {
        cout << "Bucket " << i + 1 << ": ";
        for (int j = 0; j < 10 && j < BUCKET_SIZE; j++) {
            cout << static_cast<int>(buckets[i][j]) << " "; // Cast to int for readable output.
        }
        cout << "..." << endl; // Indicate more data exists in the bucket.
    }
}

// Function to merge the 8 buckets into one contiguous grayscale image.
void mergeBuckets(const vector<vector<unsigned char>> &buckets, vector<unsigned char> &grayscaleImage) {
    grayscaleImage.clear(); // Clear any existing data in the grayscale image vector.
    // Concatenate each bucket's data into the grayscale image vector.
    for (int i = 0; i < BUCKETS; i++) {
        grayscaleImage.insert(grayscaleImage.end(), buckets[i].begin(), buckets[i].end());
    }
    cout << "Merged grayscale image data (" << grayscaleImage.size() << " bytes)." << endl;
}

// Function to save the merged grayscale image as a raw binary file.
bool saveGrayscaleRawImage(const vector<unsigned char> &grayscaleImage, const string &filename) {
    // Open an output file stream in binary mode.
    ofstream outFile(filename, ios::binary);
    if (!outFile.is_open()) { // Check if the file opened successfully.
        cerr << "Failed to open file for writing grayscale data." << endl;
        return false;
    }
    // Write the grayscale image data to the file.
    outFile.write(reinterpret_cast<const char*>(grayscaleImage.data()), grayscaleImage.size());
    outFile.close(); // Close the file after writing.
    cout << "Saved merged grayscale data to '" << filename << "'." << endl;
    return true; // Successfully saved the file.
}

// Function to convert the raw grayscale file to a JPG image using ImageMagick.
bool convertRawToJPG(const string &rawFilename, const string &jpgFilename) {
    // Construct the ImageMagick command to convert the raw grayscale file to a JPG.
    string command = "magick -size 800x600 -depth 8 gray:" + rawFilename + " " + jpgFilename;
    cout << "Converting raw grayscale binary to JPG using ImageMagick..." << endl;
    int ret = system(command.c_str());
    if (ret != 0) { // If the conversion fails, notify the user.
        cerr << "Conversion to JPG failed. Ensure ImageMagick is installed on Termux." << endl;
        return false;
    }
    cout << "Grayscale image converted to '" << jpgFilename << "'." << endl;
    return true; // Conversion successful.
}

int main(int argc, char* argv[]) {
    // If an image path is provided as an argument, use it; otherwise, use a default path.
    string inputImagePath;
    if (argc == 2) {
        inputImagePath = argv[1];
    } else {
        inputImagePath = "/storage/emulated/0/Download/input.jpg";
        cout << "No image path provided. Using default: " << inputImagePath << endl;
    }
    
    // Step 1: Convert the input image to raw binary format.
    if (!convertImageToRaw(inputImagePath))
        return -1;
    
    // Step 2: Load the raw image from the generated file ("temp_input.bin").
    vector<unsigned char> image;
    if (!loadRawImage("temp_input.bin", image))
        return -1;
    
    // Step 3: Connect to the server using the defined SERVER_IP and PORT.
    int sock = connectToServer(SERVER_IP, PORT);
    if (sock < 0)
        return -1;
    
    // Step 4: Send the raw image data over the established socket connection.
    if (!sendImageData(sock, image)) {
        close(sock);
        return -1;
    }
    
    // Step 5: Receive 8 buckets of processed grayscale data from the server.
    vector<vector<unsigned char>> buckets;
    if (!receiveBuckets(sock, buckets)) {
        close(sock);
        return -1;
    }
    close(sock); // Close the socket once data has been received.
    
    // Step 6: Print a brief summary of the received grayscale buckets.
    printBucketsSummary(buckets);
    
    // Step 7: Merge the individual buckets into a single grayscale image vector.
    vector<unsigned char> grayscaleImage;
    mergeBuckets(buckets, grayscaleImage);
    
    // Step 8: Save the merged grayscale image as a raw binary file.
    if (!saveGrayscaleRawImage(grayscaleImage, "gray_output.bin"))
        return -1;
    
    // Step 9: Convert the raw grayscale binary file into a JPG image using ImageMagick.
    if (!convertRawToJPG("gray_output.bin", "gray_output.jpg"))
        return -1;
    
    return 0; // End the program successfully.
}