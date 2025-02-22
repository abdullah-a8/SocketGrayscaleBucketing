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
const char* SERVER_IP = "192.168.100.24";  // Hard-coded server IP
const int PORT = 55000;

// Image constants
const int WIDTH = 800;
const int HEIGHT = 600;
const int CHANNELS = 3;
const int TOTAL_PIXELS = WIDTH * HEIGHT;
const int IMAGE_SIZE = TOTAL_PIXELS * CHANNELS; // 1,440,000 bytes (RGB)
const int GRAY_SIZE = TOTAL_PIXELS;               // 480,000 bytes (grayscale)
const int BUCKETS = 8;
const int BUCKET_SIZE = GRAY_SIZE / BUCKETS;        // 60,000 bytes

// Function to convert the input image (jpg/png/etc.) to raw RGB binary using ImageMagick.
// It outputs the file "temp_input.bin".
bool convertImageToRaw(const string &inputImagePath) {
    string command = "magick " + inputImagePath + " -resize 800x600 -depth 8 -colorspace RGB RGB:temp_input.bin";
    cout << "Converting input image to raw binary format using ImageMagick..." << endl;
    int ret = system(command.c_str());
    if (ret != 0) {
        cerr << "Image conversion failed. Ensure ImageMagick is installed on Termux." << endl;
        return false;
    }
    return true;
}

// Function to load the raw image from a file into a vector.
// Expects the file size to match IMAGE_SIZE.
bool loadRawImage(const string &filename, vector<unsigned char> &image) {
    ifstream file(filename, ios::binary | ios::ate);
    if (!file.is_open()) {
        cerr << "Failed to open the raw image file (" << filename << ")." << endl;
        return false;
    }
    streamsize fileSize = file.tellg();
    file.seekg(0, ios::beg);
    if (fileSize != IMAGE_SIZE) {
        cerr << "Raw image file size mismatch. Expected " << IMAGE_SIZE
             << " bytes, but got " << fileSize << " bytes." << endl;
        return false;
    }
    image.resize(IMAGE_SIZE);
    if (!file.read(reinterpret_cast<char*>(image.data()), IMAGE_SIZE)) {
        cerr << "Failed to read raw image data from " << filename << endl;
        return false;
    }
    file.close();
    cout << "Loaded raw image from '" << filename << "' (" << IMAGE_SIZE << " bytes)." << endl;
    return true;
}

// Function to create a socket and connect to the server.
int connectToServer(const char* serverIP, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cerr << "Socket creation error." << endl;
        return -1;
    }
    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, serverIP, &serv_addr.sin_addr) <= 0) {
        cerr << "Invalid server address." << endl;
        return -1;
    }
    if (connect(sock, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "Connection to server failed." << endl;
        return -1;
    }
    cout << "Connected to server " << serverIP << " on port " << port << endl;
    return sock;
}

// Function to send the raw image data over the socket.
bool sendImageData(int sock, const vector<unsigned char>& image) {
    int total_sent = 0;
    while (total_sent < IMAGE_SIZE) {
        int bytes = send(sock, image.data() + total_sent, IMAGE_SIZE - total_sent, 0);
        if (bytes < 0) {
            cerr << "Failed to send image data." << endl;
            return false;
        }
        total_sent += bytes;
    }
    cout << "Sent raw image (" << total_sent << " bytes) to server." << endl;
    return true;
}

// Function to receive 8 buckets of processed (grayscale) data from the server.
bool receiveBuckets(int sock, vector<vector<unsigned char>> &buckets) {
    buckets.resize(BUCKETS, vector<unsigned char>(BUCKET_SIZE));
    for (int i = 0; i < BUCKETS; i++) {
        int total_received = 0;
        while (total_received < BUCKET_SIZE) {
            int bytes = recv(sock, buckets[i].data() + total_received, BUCKET_SIZE - total_received, 0);
            if (bytes <= 0) {
                cerr << "Failed to receive data for bucket " << i + 1 << endl;
                return false;
            }
            total_received += bytes;
        }
        cout << "Received bucket " << i + 1 << " (" << total_received << " bytes)." << endl;
    }
    return true;
}

// Function to print a summary (first 10 values) of each bucket.
void printBucketsSummary(const vector<vector<unsigned char>> &buckets) {
    cout << "Received grayscale buckets data:" << endl;
    for (int i = 0; i < BUCKETS; i++) {
        cout << "Bucket " << i + 1 << ": ";
        for (int j = 0; j < 10 && j < BUCKET_SIZE; j++) {
            cout << static_cast<int>(buckets[i][j]) << " ";
        }
        cout << "..." << endl;
    }
}

// Function to merge the 8 buckets into one grayscale image.
void mergeBuckets(const vector<vector<unsigned char>> &buckets, vector<unsigned char> &grayscaleImage) {
    grayscaleImage.clear();
    for (int i = 0; i < BUCKETS; i++) {
        grayscaleImage.insert(grayscaleImage.end(), buckets[i].begin(), buckets[i].end());
    }
    cout << "Merged grayscale image data (" << grayscaleImage.size() << " bytes)." << endl;
}

// Function to save the merged grayscale image as a raw binary file.
bool saveGrayscaleRawImage(const vector<unsigned char> &grayscaleImage, const string &filename) {
    ofstream outFile(filename, ios::binary);
    if (!outFile.is_open()) {
        cerr << "Failed to open file for writing grayscale data." << endl;
        return false;
    }
    outFile.write(reinterpret_cast<const char*>(grayscaleImage.data()), grayscaleImage.size());
    outFile.close();
    cout << "Saved merged grayscale data to '" << filename << "'." << endl;
    return true;
}

// Function to convert the raw grayscale file to a JPG image using ImageMagick.
bool convertRawToJPG(const string &rawFilename, const string &jpgFilename) {
    string command = "magick -size 800x600 -depth 8 gray:" + rawFilename + " " + jpgFilename;
    cout << "Converting raw grayscale binary to JPG using ImageMagick..." << endl;
    int ret = system(command.c_str());
    if (ret != 0) {
        cerr << "Conversion to JPG failed. Ensure ImageMagick is installed on Termux." << endl;
        return false;
    }
    cout << "Grayscale image converted to '" << jpgFilename << "'." << endl;
    return true;
}

int main(int argc, char* argv[]) {
    // If an image path is provided as argument, use it; otherwise, use the default.
    string inputImagePath;
    if (argc == 2) {
        inputImagePath = argv[1];
    } else {
        inputImagePath = "/storage/emulated/0/Download/input.jpg";
        cout << "No image path provided. Using default: " << inputImagePath << endl;
    }
    
    // Step 1: Convert input image to raw binary.
    if (!convertImageToRaw(inputImagePath))
        return -1;
    
    // Step 2: Load the raw image from file.
    vector<unsigned char> image;
    if (!loadRawImage("temp_input.bin", image))
        return -1;
    
    // Step 3: Connect to the server.
    int sock = connectToServer(SERVER_IP, PORT);
    if (sock < 0)
        return -1;
    
    // Step 4: Send the raw image data.
    if (!sendImageData(sock, image)) {
        close(sock);
        return -1;
    }
    
    // Step 5: Receive the processed grayscale buckets.
    vector<vector<unsigned char>> buckets;
    if (!receiveBuckets(sock, buckets)) {
        close(sock);
        return -1;
    }
    close(sock);
    
    // Step 6: Display the summary of received buckets.
    printBucketsSummary(buckets);
    
    // Step 7: Merge the buckets into a single grayscale image.
    vector<unsigned char> grayscaleImage;
    mergeBuckets(buckets, grayscaleImage);
    
    // Step 8: Save the merged grayscale image as raw binary.
    if (!saveGrayscaleRawImage(grayscaleImage, "gray_output.bin"))
        return -1;
    
    // Step 9: Convert the raw grayscale image to a JPG.
    if (!convertRawToJPG("gray_output.bin", "gray_output.jpg"))
        return -1;
    
    return 0;
}