#ifndef SERVER
#define SERVER

#include "common_defs.h"

template<size_t N>
class Server {
public:
    
    Server() = default;
    Server(std::function<void(size_t, std::string)> _callback, std::string _port) {
        callback = _callback;
        port = _port;
        stop.store(false);
        initialized = true;
    }

    Server& operator=(Server&& server) {
        if (this->initialized)
            throw std::runtime_error("Trying to initialize an initialized server");
        if (!server.initialized)
            throw std::runtime_error("Trying to initialize w/ uninitialized server");
        if (server.mainServerThread.joinable()) 
            throw std::runtime_error("Trying to initialize w/ a running server");

        server.initialized = false;
        port = server.port;
        callback = std::move(server.callback);
        initialized = true;
        stop.store(false);

        return *this;
    }

    ~Server() {    
        if (!stop.load())
            stop.store(true);
        
        close(masterSocket);
        for(size_t i = 0; i < N; i++)
            if (clientSockets[i] != 0) close(clientSockets[i]);
    }

    void shutdown() {
        stop.store(true);
    }

    void message(size_t client_id, std::string message) {
        #ifdef DEBUG
        std::cout << "Attempting to send message to " << client_id << "\n";
        #endif 
        
        status_t status = write(clientSockets[client_id], message.c_str(), message.size()+1);
        if (status <= 0) {
            std::cerr << "Error on write\n";
        }
    }

    int start() {
        if (!initialized)
            throw std::runtime_error("Not initialized!");
        status_t status;
        struct addrinfo host_info, *host_info_list;
        // reset client sockets
        memset(clientSockets, 0, N*sizeof(clientSockets[0]));
        memset(&host_info, 0, sizeof(host_info));

        // get host information
        host_info.ai_family = AF_UNSPEC;
        host_info.ai_socktype = SOCK_STREAM;
        host_info.ai_flags = AI_PASSIVE;

        status = getaddrinfo(NULL, port.c_str(), &host_info, &host_info_list);
        if (status != 0) {
            std::cerr << "Error: Cannot get address info for host" << std::endl;
            return -1;
        }

        // make master socket
        masterSocket = socket(host_info_list->ai_family,
                           host_info_list->ai_socktype,
                           host_info_list->ai_protocol);
        if (masterSocket < 0) {
            std::cerr << "Error on getting master socket with error: " << strerror(errno) <<"\n";
            std::cerr << "masterSocket number: " << masterSocket << '\n';
            return -1;
        }

        int yes = 1;
        status = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (status != 0) {
            std::cerr << "Error on set socket options\n";
            return -1;
        }
        status = bind(masterSocket, host_info_list->ai_addr, host_info_list->ai_addrlen);

        if (status == -1) {
            std::cerr << "Error: Cannot bind socket" << std::endl;
            return -1;
        }

        status = listen(masterSocket, N);
        if (status == -1) {
            std::cerr << "Error: cannot listen on socket" << std::endl;
            return -1;
        }

        freeaddrinfo(host_info_list);
        mainServerThread = std::thread(std::bind(&Server::main, this));
        mainServerThread.detach();
        return 0;
    }

    std::pair<std::string, std::string> getServerInfo() {
        if (!initialized || masterSocket < 0) {
            throw std::runtime_error("Server is not initialized or not started.");
        }

        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        // Use getsockname to fill addr with the server's socket address
        if (getsockname(masterSocket, (struct sockaddr*)&addr, &len) == -1) {
            throw std::runtime_error("Failed to get socket name: " + std::string(strerror(errno)));
        }

        // Convert IP address to string
        char ipStr[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &addr.sin_addr, ipStr, sizeof(ipStr)) == nullptr) {
            throw std::runtime_error("Failed to convert IP address to string: " + std::string(strerror(errno)));
        }

        // Convert port number from network byte order to host byte order
        auto port = ntohs(addr.sin_port);
        
        return {ipStr, std::to_string(port)};
    }

    // returns (ip, port)
    std::pair<std::string, std::string> getClientInfo(size_t clientId) {
        if (!initialized || masterSocket < 0) {
            throw std::runtime_error("Server is not initialized or not started");
        }

        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        memset(&clientAddr, 0, sizeof(clientAddr));

        int clientSock = clientSockets[clientId];

        if (getpeername(clientSock, (struct sockaddr*)&clientAddr, &clientAddrLen) == -1) {
            throw std::runtime_error("Failred to get client info.");
        }

        char ipStr[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr)) == nullptr) {
            throw std::runtime_error("Failed to convert IP address");
        }

        int port = ntohs(clientAddr.sin_port);
        return {ipStr, std::to_string(port)};
    }

    size_t getNumConnections() const {
        return numConnections.load();
    }
    
private:

    void main() {
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        status_t status;
        socketfd_t max_socket;
        fd_set readfds;

        // #ifdef DEBUG
        // int cnter = 0;
        // #endif

        while(!stop.load()) {
            // #ifdef DEBUG
            // cnter++;
            // if (cnter % 1000 == 0)
            //     std::cout << "Checking connection\n";
            // #endif
            FD_ZERO(&readfds);

            FD_SET(masterSocket, &readfds);

            max_socket = masterSocket;

            for(size_t i = 0; i < N; i++) {
                if (clientSockets[i] > 0) {
                    FD_SET(clientSockets[i], &readfds);
                    if (clientSockets[i] > max_socket)
                        max_socket = clientSockets[i];
                }
            }

            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 1000;

            status = select(max_socket+1, &readfds, NULL, NULL, &timeout);

            if (status == -1) {
                if (stop.load()) {
                    break;
                } else {
                    continue;
                }
            }

            // new connection
            if (FD_ISSET(masterSocket, &readfds)) {
                socketfd_t new_socket = accept(masterSocket,
                                               (struct sockaddr *)&address, (socklen_t*)&addrlen);
                if (new_socket < 0) {
                    std::cerr << "Error accepting new socket\n";
                    break;
                }
                
                for(size_t i = 0; i < N; i++) {
                    if (clientSockets[i] == 0) {
                        #ifdef DEBUG
                        std::cout << "new client accepted with socket " << new_socket << '\n';
                        #endif 
                        
                        clientSockets[i] = new_socket;
                        numConnections++;
                        break;
                    }
                    if (i == N-1) {
                        throw std::runtime_error("Server connection limit exceeded");
                    }
                }
            }

            // existing connection IO
            for(size_t i = 0; i < N; i++) {
                if (clientSockets[i] > 0 && FD_ISSET(clientSockets[i], &readfds)) {

                    #ifdef DEBUG
                    std::cout << "Attempting to read socket " << clientSockets[i] << '\n';
                    #endif
                    status_t amount_read = read(clientSockets[i], buffer, BUFFER_SIZE);
                    #ifdef DEBUG
                    std::cout << "Finished reading socket " << clientSockets[i] << " with " << amount_read << " bytes\n";
                    #endif
                    if (amount_read == 0) {
                        close(clientSockets[i]);
                        clientSockets[i] = 0;
                        numConnections--;
                    } else if (amount_read < 0) {
                        if (stop.load()) break;
                    } else {
                        #ifdef DEBUG
                        std::cout << "Processing read content, attempting to callback\n";
                        #endif
                        buffer[amount_read] = char(0);

                        callback(i, std::string(buffer));
                    }
                }
            }
        }
    }
    socketfd_t masterSocket, clientSockets[N];
    std::string port;
    char buffer[BUFFER_SIZE+1];

    std::function<void(size_t, std::string)> callback;
    std::atomic<bool> stop;
    std::atomic<size_t> numConnections;

    std::thread mainServerThread;
    bool initialized = false;
};

#endif