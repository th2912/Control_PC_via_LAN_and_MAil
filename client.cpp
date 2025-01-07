#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
#include <ctime>
#include <sstream>
#include <regex>
#include <thread>
#include <chrono>
#include <curl/curl.h>
#include <ws2tcpip.h>
#include <direct.h>
#include <CkImap.h>
#include <CkMessageSet.h>
#include <CkEmailBundle.h>
#include <CkEmail.h>

#define BUFFER_SIZE 1024
#define FROM_MAIL "" //admin mail "name <admin@gmail.com>"
#define TO_MAIL   "" //client mail "name <client@gmail.com>" 
#define USERNAME "" //cliet user "client@gmail.com" 
#define PASSWORD "" //client password or client app password

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

const string AUTHORIZED_SENDER = ""; // admin mail "admin@gmail.com"

//--------------------------------------------------- ĐỌC EMAIL ----------------------------------------------------------------//

string removeHtmlTags(const string& html) {
    regex htmlTagPattern("<[^>]*>");
    return regex_replace(html, htmlTagPattern, "");
}

string readMail() {
    CkImap imap;
    imap.put_Ssl(true);
    imap.put_Port(993);
    bool success = imap.Connect("imap.gmail.com");
    if (!success) {
        cout << imap.lastErrorText() << "\r\n";
        return "";
    }

    success = imap.Login(USERNAME, PASSWORD);
    if (!success) {
        cout << imap.lastErrorText() << "\r\n";
        return "";
    }

    success = imap.SelectMailbox("Inbox");
    if (!success) {
        cout << imap.lastErrorText() << "\r\n";
        return "";
    }

    CkMessageSet* messageSet = imap.Search("ALL", true);
    if (!imap.get_LastMethodSuccess()) {
        cout << imap.lastErrorText() << "\r\n";
        return "";
    }

    CkEmailBundle* bundle = imap.FetchBundle(*messageSet);
    if (!imap.get_LastMethodSuccess()) {
        delete messageSet;
        cout << imap.lastErrorText() << "\r\n";
        return "";
    }

    int numEmails = bundle->get_MessageCount();
    for (int i = 0; i < numEmails; i++) {
        CkEmail* email = bundle->GetEmail(i);

        string from = email->ck_from();
        string plainTextBody = removeHtmlTags(email->body());

        if (from.find(AUTHORIZED_SENDER) != string::npos) {
            delete email;
            delete messageSet;
            delete bundle;
            imap.Disconnect();
            return plainTextBody;
        }
        delete email;
    }

    success = imap.Disconnect();
    delete messageSet;
    delete bundle;

    return "";
}

//-------------------------------------------------XONG ĐỌC EMAIL --------------------------------------------------------------//


//--------------------------------------------------  ĐIỀU KHIỂN ---------------------------------------------------------------//
const string RESOURCE_FOLDER = "Resource Files/";

void EnsureResourceFolder() {
    if (_mkdir(RESOURCE_FOLDER.c_str()) == 0 || errno == EEXIST) {
    }
    else {
        cerr << "Failed to create Resource Files folder." << endl;
    }
}

void ReceiveFile(SOCKET server, const string& filename) {
    ofstream outputFile(RESOURCE_FOLDER + filename, ios::binary);
    if (!outputFile.is_open()) {
        cerr << "Failed to open file for writing: " << filename << endl;
        return;
    }

    streamsize fileSize;
    if (recv(server, reinterpret_cast<char*>(&fileSize), sizeof(fileSize), 0) <= 0) {
        cerr << "Failed to receive file size for: " << filename << endl;
        return;
    }

    char buffer[1024];
    streamsize totalReceived = 0;
    while (totalReceived < fileSize) {
        int bytesReceived = recv(server, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            cerr << "Error receiving file: " << filename << endl;
            break;
        }
        outputFile.write(buffer, bytesReceived);
        totalReceived += bytesReceived;
    }
    outputFile.close();

    if (totalReceived == fileSize) {
        cout << "File received and saved: " << RESOURCE_FOLDER + filename << endl;
    }
    else {
        cerr << "Incomplete file received: " << filename << endl;
    }
}

void ReceiveMessage(SOCKET server, string& content) {
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    int bytesReceived = recv(server, buffer, sizeof(buffer), 0);
    if (bytesReceived > 0) {
        content = string(buffer, bytesReceived);
        cout << "Server: " << content << endl;
    }
}
//------------------------------------------------ XONG ĐIỀU KHIỂN -------------------------------------------------------------//

//--------------------------------------------------- GỬI EMAIL ----------------------------------------------------------------//
string getCurrentTimeRFC5322() {
    time_t now = time(NULL);
    char date_buffer[100];
    struct tm tm_now;
    gmtime_s(&tm_now, &now);
    strftime(date_buffer, sizeof(date_buffer), "%a, %d %b %Y %H:%M:%S %z", &tm_now);
    return string(date_buffer);
}

string encodeBase64(const string& input) {
    static const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    string encoded;
    size_t i = 0;
    size_t input_len = input.size();

    while (i < input_len) {
        uint32_t octet_a = i < input_len ? (unsigned char)input[i++] : 0;
        uint32_t octet_b = i < input_len ? (unsigned char)input[i++] : 0;
        uint32_t octet_c = i < input_len ? (unsigned char)input[i++] : 0;

        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        encoded += base64_chars[(triple >> 18) & 0x3F];
        encoded += base64_chars[(triple >> 12) & 0x3F];
        encoded += (i > input_len + 1) ? '=' : base64_chars[(triple >> 6) & 0x3F];
        encoded += (i > input_len) ? '=' : base64_chars[triple & 0x3F];
    }

    return encoded;
}

string createPayload(const string& content, const string& attachment_path = "") {
    const string boundary = "----=_Part_123456789";
    ostringstream payload;

    payload << "Date: " << getCurrentTimeRFC5322() << "\r\n";
    payload << "To: " << TO_MAIL << "\r\n";
    payload << "From: " << FROM_MAIL << "\r\n";
    payload << "Subject: Control PC via LAN and Gmail\r\n";

    if (attachment_path.empty()) {
        payload << "\r\n";
        payload << content << "\r\n";
    }
    else {
        payload << "MIME-Version: 1.0\r\n";
        payload << "Content-Type: multipart/mixed; boundary=\"" << boundary << "\"\r\n";
        payload << "\r\n";

        payload << "--" << boundary << "\r\n";
        payload << "Content-Type: text/plain; charset=\"utf-8\"\r\n";
        payload << "Content-Transfer-Encoding: 7bit\r\n";
        payload << "\r\n";
        payload << content << "\r\n";

        ifstream file(attachment_path, ios::binary);
        if (file.is_open()) {
            ostringstream fileContent;
            fileContent << file.rdbuf();
            string fileData = fileContent.str();
            file.close();

            payload << "--" << boundary << "\r\n";
            payload << "Content-Type: application/octet-stream; name=\"" << attachment_path << "\"\r\n";
            payload << "Content-Transfer-Encoding: base64\r\n";
            payload << "Content-Disposition: attachment; filename=\"" << attachment_path << "\"\r\n";
            payload << "\r\n";
            payload << encodeBase64(fileData) << "\r\n";
        }
        payload << "--" << boundary << "--\r\n";
    }
    return payload.str();
}

struct upload_status {
    size_t bytes_read;
    string payload_text;
    };

static size_t payload_source(char* ptr, size_t size, size_t nmemb, void* userp) {
    upload_status* upload_ctx = (upload_status*)userp;
    const char* data = upload_ctx->payload_text.c_str() + upload_ctx->bytes_read;
    size_t room = size * nmemb;
    size_t len = strlen(data);

    if (len > room) len = room;
    if (len > 0) {
        memcpy(ptr, data, len);
        upload_ctx->bytes_read += len;
    }

    return len;
}

void sendMail(const string& content, const string& attachment_path = "") {
    CURL* curl;
    CURLcode res = CURLE_OK;
    struct curl_slist* recipients = NULL;
    upload_status upload_ctx;

    if (attachment_path.empty())
        upload_ctx.payload_text = createPayload(content);
    else
        upload_ctx.payload_text = createPayload(content, attachment_path);
    upload_ctx.bytes_read = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "smtps://smtp.gmail.com:465");
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, "");// client mail <client@gmail.com>
        curl_easy_setopt(curl, CURLOPT_USERNAME, USERNAME);
        curl_easy_setopt(curl, CURLOPT_PASSWORD, PASSWORD);
        recipients = curl_slist_append(recipients, "");// admin mail <admin@gmail.com>
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
        curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
}
//---------------------------------------------------XONG GỬI EMAIL ---------------------------------------------------------//

//------------------------------------------------------- MAIN --------------------------------------------------------------//
int main() {
    WSADATA wsaData;
    SOCKET client;
    sockaddr_in serverAddr;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed with error: " << WSAGetLastError() << endl;
        return 1;
    }

    client = socket(AF_INET, SOCK_STREAM, 0);
    if (client == INVALID_SOCKET) {
        cerr << "Socket creation failed with error: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);


    if (inet_pton(AF_INET, "", &serverAddr.sin_addr) <= 0) { //server IP
        cerr << "Invalid address/ Address not supported" << endl;
        closesocket(client);
        WSACleanup();
        return 1;
    }

    if (connect(client, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "Connection failed with error: " << WSAGetLastError() << endl;
        closesocket(client);
        WSACleanup();
        return 1;
    }

    EnsureResourceFolder();

  
    while (true) {
        string command = readMail();
        command.erase(remove(command.begin(), command.end(), '\r'), command.end());
        command.erase(remove(command.begin(), command.end(), '\n'), command.end());
        cout << "|" << command << "|" << endl;
        if (command.empty()) {
            cerr << "Failed to read mail or unauthorized sender." << endl;
            continue;
        }

        send(client, command.c_str(), command.length(), 0);

        string content;
        if (command == "SCREENSHOT") {
            ReceiveFile(client, "screenshot.bmp");
            content = "Screenshot saved and received.";
            sendMail(content, RESOURCE_FOLDER + "screenshot.bmp");
        }
        else if (command == "LIST_PROCESSES") {
            ReceiveFile(client, "process_list.txt");
            content = "Process list saved and received.";
            sendMail(content, RESOURCE_FOLDER + "process_list.txt");
        }
        else if (command.rfind("COPY_FILE", 0) == 0) {
            size_t firstSpace = command.find(' ');
            size_t secondSpace = command.find(' ', firstSpace + 1);
            if (firstSpace != string::npos && secondSpace != string::npos) {
                string destination = command.substr(secondSpace + 1);
                ReceiveFile(client, destination);
                content = "File copied and received.";
                sendMail(content, RESOURCE_FOLDER + destination);
            }
        }
        else {
            ReceiveMessage(client, content);
            sendMail(content);
        }
        this_thread::sleep_for(std::chrono::seconds(30));
    }


    closesocket(client);
    WSACleanup();
    return 0;
}



