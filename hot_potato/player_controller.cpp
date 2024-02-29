#include "player.h"


int main(int argc, char* argv[]) {

    if (argc != 3) {
        std::cout << "Usage: ./player <host machine name> <ringmaster port>\n";
        return 0;
    }

    std::string hostname = std::string(argv[1]);
    std::string hostPort = std::string(argv[2]);

    Player player(hostname, hostPort);

    player.start();

    while(!player.isDone()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    #ifdef DEBUG
    std::cout << "Player is done\n";
    #endif
}