#include "ringmaster.h"
#include <cstring>
#include <cstdlib>
#include <string>

int main(int argc, char* argv[]) {
    
    if (argc != 4) {
        std::cout << "Usage: <port> <num players> <num hops>";
        return 1;
    }

    std::string port = std::string(argv[1]);
    size_t numPlayers = std::stoi(argv[2]);
    size_t numHops = std::stoi(argv[3]);

    RingMaster rm(port, numPlayers, numHops);


    rm.start();

    #ifdef DEBUG
    std::cout << "Server started\n";
    #endif

    while(!rm.isDone()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

}