#include <Winsock2.h>
#include <Ws2tcpip.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <direct.h>

#pragma comment(lib, "Ws2_32.lib")

#define BUFFER_LEN 8192
#define MAX_CLIENTS 32

using namespace std;

const char *SERVER_HOME_DIR = "C:\\Users\\Kirin\\source\\repos\\SimpleWebServer\\Server\\public_page"; // Server home directory
const char *DEFAULT_FILE = "index.html";       // Default file to serve

struct Client
{
    SOCKET socket;
    string requestedResource;
};

string getCurrentWorkingDirectory()
{
    char buff[1024];
    if (_getcwd(buff, 1024) == NULL)
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
    else
    {
        return "text/plain";
    }
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

    // if resource points to a directory, then default to index.html inside that directory
    // that is, if the URL is http://localhost:8080/about/, then the resource will be about/index.html
    // also if the URL is http://localhost:8080/about, then the resource will be about/index.html
    if (resource.back() == '/')
    {
        resource += DEFAULT_FILE;
    }

    // default resource to index.html if empty
    if (resource.empty() || resource == "/")
    {
        resource = DEFAULT_FILE;
    }
    else
    {
        resource = resource.substr(1); // Remove the leading '/'
    }

    return resource + queryString;

}

// Handles the HTTP request and sends a response
void handleHttpRequest(Client &client)
{
    char buffer[BUFFER_LEN];
    int receivedBytes = recv(client.socket, buffer, BUFFER_LEN - 1, 0);

    if (receivedBytes > 0)
    {
        buffer[receivedBytes] = '\0';
        string request(buffer);

        // Parse the HTTP request
        istringstream requestStream(request);
        string requestLine;
        getline(requestStream, requestLine);

        istringstream lineStream(requestLine);
        string method;
        string url;
        string httpVersion;
        lineStream >> method >> url >> httpVersion;

        // We only support GET requests
        if (method != "GET")
        {
            string response = "HTTP/1.0 501 Not Implemented\r\n\r\n";
            send(client.socket, response.c_str(), response.size(), 0);
            return;
        }

        string filePath = string(SERVER_HOME_DIR) + "/" + url;
        string fileContent = readFileIntoString(filePath);
        string contentType = getContentType(url);

        // Debug: print the request
        cout << method << " " << url << " " << httpVersion << endl;
        // Debug: 输出磁盘上的文件路径，需要绝对路径

        if (fileContent.empty())
        {
            // File not found, send 404 response
            string response = "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\n\r\n<html><body><h1>404 Not Found</h1></body></html>";
            send(client.socket, response.c_str(), response.size(), 0);
        }
        else
        {
            // File found, send 200 response with file content
            ostringstream responseStream;
            responseStream << "HTTP/1.0 200 OK\r\n";
            responseStream << "Content-Type: " << contentType << "\r\n";
            responseStream << "Content-Length: " << fileContent.size() << "\r\n";
            responseStream << "\r\n";
            responseStream << fileContent;

            string response = responseStream.str();
            send(client.socket, response.c_str(), response.size(), 0);
        }
    }
}

int main(int argc, char *argv[])
{

    // Debug: getCurrentWorkingDirectory()
    cout << "Current working directory: " << getCurrentWorkingDirectory() << endl;

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
    serverAddress.sin_port = htons(8080); // Default port for HTTP

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

    cout << "Server is listening on port " << ntohs(serverAddress.sin_port) << endl;

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
        handleHttpRequest(client);

        closesocket(client.socket);
    }

    // Cleanup
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}