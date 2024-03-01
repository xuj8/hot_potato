#ifndef CLIENT
#define CLIENT
#include "common_defs.h"


class Client {
public:

    Client() = default;
    Client(std::function<void(std::string)> _callback, std::string _hostname, std::string _port) {
        callback = _callback;
        hostname = _hostname;
        port = _port;
        stop.store(false);
        initialized = true;
    }

    Client& operator=(Client&& client) {
        if (this->initialized)
            throw std::runtime_error("Trying to initialized an initialized client");

        if (!client.initialized)
            throw std::runtime_error("Trying to initialize client with uninitialized client");

        if (client.mainClientThread.joinable())
            throw std::runtime_error("Trying to initialize client with running client");
        
        hostname = client.hostname;
        port = client.port;
        master_socket = client.master_socket;
        callback = std::move(client.callback);
        stop.store(false);
        mainClientThread = std::move(client.mainClientThread);
        initialized = true;
        client.initialized = false;

        return *this;
    }
    
    ~Client() {
        if (!stop.load()) 
            stop.store(true);
        close(master_socket);
    }

    void shutdown() {
        stop.store(true);
    }

    void message(std::string message) {

        #ifdef DEBUG
        std::cout << "Going to write the message " << message << " as client\n";
        #endif
        status_t status = write(master_socket, message.c_str(), message.size()+1);

        #ifdef DEBUG
        std::cout << "Finished writing message, status " << status << '\n';
        #endif

        if (status <= 0) {
            std::cerr << "Error on write\n";
        }
    }

    int start() {
        if (!initialized)
            throw std::runtime_error("Not initialized!");
        status_t status;
        struct addrinfo host_info, *host_info_list;
        memset(&host_info, 0, sizeof(host_info));

        host_info.ai_family = AF_UNSPEC;
        host_info.ai_socktype = SOCK_STREAM;

        status = getaddrinfo(hostname.c_str(), port.c_str(), &host_info, &host_info_list);
        if (status != 0) {
            std::cerr << "Error getting address info\n";
            return -1;
        }

        master_socket = socket(host_info_list->ai_family,
                               host_info_list->ai_socktype,
                               host_info_list->ai_protocol);

        if (master_socket == -1) {
            std::cerr << "Error cannot create socket\n";
            return -1;
        }

        int retries = 10;


        #ifdef DEBUG
        std::cout << "Starting connection to " << host_info_list->ai_addr << "\n";
        #endif
        while(retries--) {

            #ifdef DEBUG
            std::cout << "Right before connection: \n";
            #endif
            status = connect(master_socket, host_info_list->ai_addr, host_info_list->ai_addrlen);

            #ifdef DEBUG
            std::cout << "status of connection " << status << "\n";
            std::cout << "Error message: " << strerror(errno) << '\n';
            #endif

            if (status == -1) std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            else break;
            #ifdef DEBUG
            std::cout << "unable to connect to " << host_info_list->ai_addr << ", retrying\n";
            #endif
        }

        if (status == -1) {
            std::cerr << "Cannot connect to server\n";
            return -1;
        }

        freeaddrinfo(host_info_list);

        mainClientThread = std::thread(std::bind(&Client::main, this));
        mainClientThread.detach();
        #ifdef DEBUG
        std::cout << "Main client thread started and detached, returning\n";
        #endif
        return 0;
    }

private:
    void main() {
        fd_set readfds;

        while(!stop.load()) {
            FD_ZERO(&readfds);
            FD_SET(master_socket, &readfds);
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 1000;

            select(master_socket+1, &readfds, NULL, NULL, &tv);

            if (FD_ISSET(master_socket, &readfds)) {
                status_t amount_read = read(master_socket, buffer, BUFFER_SIZE);
                if (amount_read == 0) {
                    close(master_socket);
                    return;
                } else if (amount_read == -1) {
                    if (stop.load()) break;
                } else {
                    buffer[amount_read] = '\0';
                    callback(std::string(buffer));
                }
            } else if (stop.load()) {
                break;
            } else continue;
        }
    }
    std::string hostname, port;
    socketfd_t master_socket;
    std::function<void(std::string)> callback;
    std::atomic<bool> stop;
    char buffer[BUFFER_SIZE+1];
    std::thread mainClientThread;
    bool initialized = false;
};

#endif