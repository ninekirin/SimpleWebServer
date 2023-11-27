#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <direct.h>
#include <io.h>
#include <stdio.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>

#pragma comment(lib, "Ws2_32.lib")

#define BUFFER_LEN 8192
#define MAX_CLIENTS 32

using namespace std;
using namespace std::chrono;

auto startTime = high_resolution_clock::now(); // Start measuring time

const char *DEFAULT_SERVER_HOME_DIR = "C:\\Users\\Kirin\\source\\repos\\SimpleWebServer\\Server\\public_page"; // Server home directory
const char *DEFAULT_FILE = "index.html";                                                                       // Default file to serve
const unsigned int DEFAULT_PORT = 8080;                                                                        // Default port for HTTP server

string serverHomeDir(DEFAULT_SERVER_HOME_DIR);
int listeningPort(DEFAULT_PORT);

// Mutex for printing to cout
mutex cout_mutex;

// Thread-safe print function
void safePrint(const string &message)
{
    lock_guard<mutex> guard(cout_mutex);
    // cout << message << endl;
    // Output with timestamp (format: [seconds.milliseconds] message)
    auto currentTime = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(currentTime - startTime);
    // Format ms to 3 digits
    string ms = to_string(duration.count() % 1000);
    while (ms.size() < 3)
    {
        ms += "0";
    }
    cout << "[" << duration.count() / 1000 << "." << ms << "] " << message << endl;
}

struct Client
{
    SOCKET socket;
    string requestedResource;
};

string getCurrentWorkingDirectory()
{
    char buff[BUFFER_LEN];
    if (_getcwd(buff, BUFFER_LEN) == NULL)
    {
        perror("getcwd() error");
        return "";
    }
    string current_working_directory(buff);
    return current_working_directory;
}

// Function to read the entire file into a string
string readFileIntoString(const string &path)
{
    ifstream input_file(path);
    if (!input_file.is_open())
    {
        return "";
    }
    return string((istreambuf_iterator<char>(input_file)), istreambuf_iterator<char>());
}

// Function to determine the MIME type based on file extension
string getContentType(const string &filename)
{
    if (filename.find(".htm") != string::npos || filename.find(".html") != string::npos)
    {
        return "text/html";
    }
    else if (filename.find(".jpg") != string::npos || filename.find(".jpeg") != string::npos)
    {
        return "image/jpeg";
    }
    else if (filename.find(".png") != string::npos)
    {
        return "image/png";
    }
    else if (filename.find(".bmp") != string::npos)
    {
        return "image/bmp";
    }
    else if (filename.find(".svg") != string::npos)
    {
        return "image/svg+xml";
    }
    else if (filename.find(".webp") != string::npos)
    {
        return "image/webp";
    }
    else if (filename.find(".gif") != string::npos)
    {
        return "image/gif";
    }
    else if (filename.find(".css") != string::npos)
    {
        return "text/css";
    }
    else if (filename.find(".js") != string::npos)
    {
        return "application/javascript";
    }
    else if (filename.find(".ico") != string::npos)
    {
        return "image/x-icon";
    }
    else if (filename.find(".json") != string::npos)
    {
        return "application/json";
    }
    else if (filename.find(".pdf") != string::npos)
    {
        return "application/pdf";
    }
    else if (filename.find(".zip") != string::npos)
    {
        return "application/zip";
    }
    else if (filename.find(".doc") != string::npos || filename.find(".docx") != string::npos)
    {
        return "application/msword";
    }
    else if (filename.find(".xls") != string::npos || filename.find(".xlsx") != string::npos)
    {
        return "application/vnd.ms-excel";
    }
    else if (filename.find(".ppt") != string::npos || filename.find(".pptx") != string::npos)
    {
        return "application/vnd.ms-powerpoint";
    }
    else if (filename.find(".mp3") != string::npos)
    {
        return "audio/mpeg";
    }
    else if (filename.find(".mp4") != string::npos)
    {
        return "video/mp4";
    }
    else if (filename.find(".ogg") != string::npos)
    {
        return "audio/ogg";
    }
    else if (filename.find(".ogv") != string::npos)
    {
        return "video/ogg";
    }
    else if (filename.find(".ogx") != string::npos)
    {
        return "application/ogg";
    }
    else if (filename.find(".wav") != string::npos)
    {
        return "audio/wav";
    }
    else if (filename.find(".webm") != string::npos)
    {
        return "video/webm";
    }
    else if (filename.find(".xml") != string::npos)
    {
        return "application/xml";
    }
    else if (filename.find(".woff") != string::npos)
    {
        return "font/woff";
    }
    else if (filename.find(".woff2") != string::npos)
    {
        return "font/woff2";
    }
    else if (filename.find(".ttf") != string::npos)
    {
        return "font/ttf";
    }
    else if (filename.find(".otf") != string::npos)
    {
        return "font/otf";
    }
    else if (filename.find(".eot") != string::npos)
    {
        return "application/vnd.ms-fontobject";
    }
    else if (filename.find(".sfnt") != string::npos)
    {
        return "application/font-sfnt";
    }
    else if (filename.find(".txt") != string::npos)
    {
        return "text/plain";
    }
    else
    {
        return "text/plain";
    }
}

// Function to send the entire file content
void sendFileContent(const SOCKET &socket, const string &filePath, const string &contentType)
{
    ifstream fileStream(filePath, ios::in | ios::binary);
    if (!fileStream.is_open())
    {
        // File not found, send 404 response
        string response = "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\n\r\n<html><body><h1>404 Not Found</h1></body></html>";
        send(socket, response.c_str(), response.size(), 0);
        return;
    }

    // Get the length of the file
    fileStream.seekg(0, fileStream.end);
    size_t fileLength = fileStream.tellg();
    fileStream.seekg(0, fileStream.beg);

    // Send HTTP headers
    stringstream headers;
    headers << "HTTP/1.0 200 OK\r\n";
    headers << "Content-Type: " << contentType << "\r\n";
    headers << "Content-Length: " << fileLength << "\r\n";
    headers << "Connection: keep-alive\r\n";
    headers << "\r\n";
    string headersStr = headers.str();
    send(socket, headersStr.c_str(), headersStr.size(), 0);

    // Send the file content in chunks
    char buffer[BUFFER_LEN];
    while (fileStream.read(buffer, sizeof(buffer)) || fileStream.gcount())
    {
        send(socket, buffer, fileStream.gcount(), 0);
    }
    fileStream.close();
}

// Function to parse the URL into the requested resource and query string
string parseUrl(const string &url)
{
    string resource = url;
    string queryString = "";

    size_t pos = url.find('?');
    if (pos != string::npos)
    {
        resource = url.substr(0, pos);
        queryString = url.substr(pos);
    }

    // If resource points to a directory, then default to index.html inside that directory
    if (resource.back() == '/')
    {
        resource += DEFAULT_FILE;
    }
    // Without the trailing slash, if the URL is '/about', then the resource will be about/index.html
    else
    {
        size_t lastSlash = resource.find_last_of('/');
        if (lastSlash != string::npos)
        {
            string lastPart = resource.substr(lastSlash + 1);
            if (lastPart.find('.') == string::npos)
            {
                resource = resource + "/" + DEFAULT_FILE;
            }
        }
    }

    // Default resource to index.html if empty
    if (resource.empty() || resource == "/")
    {
        resource = DEFAULT_FILE;
    }
    else
    {
        resource = resource.substr(1); // Remove the leading '/'
    }

    // Translate '/' to '\\' for Windows paths
    for (size_t i = 0; i < resource.size(); ++i)
    {
        if (resource[i] == '/')
        {
            resource[i] = '\\';
        }
    }

    // Translate URI encoded characters
    string uriDecodedResource;
    for (size_t i = 0; i < resource.size(); ++i)
    {
        if (resource[i] == '%')
        {
            string hex = resource.substr(i + 1, 2);
            char decodedChar = stoul(hex, nullptr, 16);
            uriDecodedResource += decodedChar;
            i += 2;
        }
        else
        {
            uriDecodedResource += resource[i];
        }
    }

    resource = uriDecodedResource;

    return resource + queryString;
}

// Handles the HTTP request and sends a response with keep-alive support
void handleHttpRequest(Client &client)
{
    char buffer[BUFFER_LEN];
    int receivedBytes = recv(client.socket, buffer, BUFFER_LEN - 1, 0);

    if (receivedBytes < 0)
        return;
    buffer[receivedBytes] = '\0';
    string request(buffer);

    // Parse the HTTP request
    istringstream requestStream(request);
    string requestLine;
    getline(requestStream, requestLine);

    istringstream lineStream(requestLine);
    string method;
    string url; // This will hold the raw URL, which may include a query string or URI parameters
    string httpVersion;
    lineStream >> method >> url >> httpVersion;

    
    // Now we only support GET requests
    if (method != "GET")
    {
        string response = "HTTP/1.0 501 Not Implemented\r\nConnection: close\r\n\r\n";
        send(client.socket, response.c_str(), response.size(), 0);
        return;
    }
    // If the request line is invalid, then send a 400 response
    else if (method.empty() || url.empty() || httpVersion.empty())
    {
        string response = "HTTP/1.0 400 Bad Request\r\nConnection: close\r\n\r\n";
        send(client.socket, response.c_str(), response.size(), 0);
        // Debug: print the response
        safePrint("<- Response: 400 Bad Request");
        return;
    }

    // Parse the 'Connection' header
    bool keepAlive = false;
    string connectionHeader;
    while (getline(requestStream, connectionHeader))
    {
        if (connectionHeader.find("Connection: ") != string::npos)
        {
            connectionHeader.erase(0, 12);
            if (connectionHeader == "keep-alive")
            {
                keepAlive = true;
            }
            break;
        }
    }

    string resource = parseUrl(url); // Use the URL parser function to get resource path

    string filePath = string(serverHomeDir) + "\\" + resource; // Consider backslash for Windows paths
    string contentType = getContentType(resource);             // Get the correct content type based on processed resource

    // Debug: print the request
    safePrint(method + " " + url + " " + httpVersion);
    // Debug: print the resource
    safePrint("-> Resource: " + resource);

    // Check if the file exists
    ifstream testStream(filePath, ios::in | ios::binary);
    if (!testStream.is_open())
    {
        string response = "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\n\r\n<html><body><h1>404 Not Found</h1></body></html>";
        send(client.socket, response.c_str(), response.size(), 0);
        // Debug: print the response
        safePrint("<- Response: 404 Not Found");
    }
    else
    {
        testStream.close();
        // For large files, use a function that streams the file contents
        sendFileContent(client.socket, filePath, contentType);
        // Debug: print the response
        safePrint("<- Response: 200 OK");
    }
}

void handleClient(Client client)
{
    handleHttpRequest(client);
    closesocket(client.socket);
}

int main(int argc, char *argv[])
{
    // Parse command-line arguments
    for (int i = 1; i < argc; ++i)
    {
        string arg(argv[i]);
        if ((arg == "-p" || arg == "--port") && i + 1 < argc)
        {
            listeningPort = stoi(argv[++i]);
            // If the user enters an empty string or invalid port number, use default value
            if (listeningPort <= 0 || listeningPort > 65535)
            {
                // cout << "Invalid port number: " << listeningPort << ", using default value: " << DEFAULT_PORT << endl;
                safePrint("Invalid port number: " + to_string(listeningPort) + ", using default value: " + to_string(DEFAULT_PORT));
                listeningPort = DEFAULT_PORT;
            }
        }
        else if ((arg == "-P" || arg == "--path") && i + 1 < argc)
        {
            serverHomeDir = argv[++i];
            // If the user enters an empty string or invalid path, use default value
            if (serverHomeDir != "")
            {
                // Check if the path exists
                int result = _access(serverHomeDir.c_str(), 0);
                if (result == -1)
                {
                    // cout << "Invalid path: " << serverHomeDir << ", using default value: " << DEFAULT_SERVER_HOME_DIR << endl;
                    safePrint("Invalid path: " + serverHomeDir + ", using default value: " + DEFAULT_SERVER_HOME_DIR);
                    serverHomeDir = DEFAULT_SERVER_HOME_DIR;
                }
                else
                {
                    // If the path end with a backslash, delete it
                    if (serverHomeDir.back() == '\\')
                    {
                        serverHomeDir.pop_back();
                    }
                }
            }
            else
            {
                serverHomeDir = DEFAULT_SERVER_HOME_DIR;
            }
        }
        else
        {
            cerr << "Unknown parameter: " << arg << endl;
            return 1;
        }
    }

    // If no arguments are passed, use default values
    // if (argc == 1) {
    //     // Get port number
    //     string listeningPortStr = to_string(listeningPort);
    //     cout << "Enter port number: (default: " << listeningPort << ") ";
    //     scanf_s(" %[^\n]s", &listeningPortStr);
    //     // If the user enters an empty string or invalid port number, use default value
    //     if (listeningPortStr != "" || stoi(listeningPortStr) <= 0 || stoi(listeningPortStr) > 65535) {
    //         listeningPort = stoi(listeningPortStr);
    //     }
    //     else {
    //         listeningPort = DEFAULT_PORT;
    //     }
    //     // Get server path
    //     cout << "Enter server path: (default: " << DEFAULT_SERVER_HOME_DIR << ") ";
    //     scanf_s(" %[^\n]s", &serverHomeDir);
    //     // If the user enters an empty string or invalid path, use default value
    //     if (serverHomeDir != "") {
    //         // Check if the path exists
    //         ifstream testStream(serverHomeDir, ios::in | ios::binary);
    //         if (!testStream.is_open())
    //         {
    //             cout << "Invalid path: " << serverHomeDir << ", using default value: " << DEFAULT_SERVER_HOME_DIR << endl;
    //             serverHomeDir = DEFAULT_SERVER_HOME_DIR;
    //         }
    //     }
    //     else {
    //         serverHomeDir = DEFAULT_SERVER_HOME_DIR;
    //     }
    // }

    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        cerr << "WSAStartup failed: " << iResult << endl;
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET)
    {
        cerr << "Socket creation failed: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(listeningPort); // Default port for HTTP

    if (bind(serverSocket, (sockaddr *)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
    {
        cerr << "Bind failed: " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, MAX_CLIENTS) == SOCKET_ERROR)
    {
        cerr << "Listen failed: " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Debug: getCurrentWorkingDirectory()
    // cout << "Current working directory: " << getCurrentWorkingDirectory() << endl;
    // safePrint("Current working directory: " + getCurrentWorkingDirectory());
    safePrint("Server home directory: " + serverHomeDir);

    // cout << "Server is listening on port " << ntohs(serverAddress.sin_port) << endl;
    safePrint("Server is listening on port " + to_string(ntohs(serverAddress.sin_port)));

    while (true)
    {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET)
        {
            cerr << "Accept failed: " << WSAGetLastError() << endl;
            continue;
        }

        Client client;
        client.socket = clientSocket;

        // Create a new thread and pass the client socket to it
        thread clientThread(handleClient, client);
        clientThread.detach(); // detach the thread so that it runs independently from the main thread
    }

    // Cleanup
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
