#include "server.h"
#include "commands.h"
#include <mutex>

class RingMaster {

public:

    RingMaster(std::string _port, size_t _numPlayers, size_t _numHops) {
        // initialize the server
        port = _port;
        numPlayers = _numPlayers;
        numHops = _numHops;
        server = Server<MAX_PLAYERS>(std::bind(&RingMaster::onMessage, this, std::placeholders::_1, std::placeholders::_2), port);
        srand((unsigned int)time(NULL));
        done.store(false);
    }

    ~RingMaster() {
    }

    void start() {
        server.start();
        #ifdef DEBUG
        std::cout << "Server hostname: " << server.getServerInfo().first << '\n';
        std::cout << "Server port: " << server.getServerInfo().second << '\n';
        #endif
        std::cout << "Potato Ringmaster\n";
        std::cout << "Players = " << numPlayers << '\n';
        std::cout << "Hops = " << numHops << '\n';
    }

    void onMessage(size_t playerId, std::string message) {
        #ifdef DEBUG
        std::cout << "Received command from player " << playerId << '\n';
        std::cout << "Attempting to lock\n";
        #endif
        std::unique_lock<std::mutex> lock(forcedSerialReceive);

        #ifdef DEBUG
        std::cout << "Locked\n";
        #endif
        
        onCommand(playerId, CommandPacket::deserialize(message));
        #ifdef DEBUG
        std::cout << "Unlocked\n";
        #endif
    };

    void onCommand(size_t playerId, CommandPacket commandPacket) {
        switch(commandPacket.commandType) {
            case CommandType::PLAYER_REGISTER: 
                onPlayerRegister(playerId, std::move(commandPacket));
                break;

            case CommandType::PLAYER_READY:
                onPlayerReady(playerId, std::move(commandPacket));
                break;
            
            case CommandType::PLAYER_REPORT_ADDR:
                onPlayerReportAddr(playerId, std::move(commandPacket));
                break;
            
            case CommandType::GIVE_POTATO:
                onReceivePotato(playerId, std::move(commandPacket));
                break;
            
            case CommandType::RINGMASTER_SET_NEXT:
            case CommandType::RINGMASTER_ASSIGN_ID_PORT:
            case CommandType::RINGMASTER_SHUTDOWN:
                throw std::runtime_error("Error, received ringmaster command");
                break;

            default:
                throw std::runtime_error("Error, unhandled command.");
                break;
        }
    };

    void onPlayerRegister(size_t playerId, CommandPacket commandPacket) {

        #ifdef DEBUG
        std::cout << "Processing player debug\n";
        #endif 

        numConnectedPlayers++;

        // assign a port to the player
        CommandPacket packet;

        packet.author = -1;
        packet.commandType = CommandType::RINGMASTER_ASSIGN_ID_PORT;

        int curPort = stoi(port);
        int playerPort = curPort + 1 + playerId;
        size_t prevId = (playerId + numPlayers - 1) % numPlayers;

        packet.commandArgs.push_back(std::to_string(playerId));
        packet.commandArgs.push_back(std::to_string(playerPort));
        packet.commandArgs.push_back(std::to_string(prevId));
        packet.commandArgs.push_back(std::to_string(numPlayers));

        
        server.message(playerId, packet.serialize());
    }

    void onReceivePotato(size_t playerId, CommandPacket commandPacket) {
        // if hops is non-zero, there is a bug, throw error

        Potato potato = Potato::parsePotato(commandPacket.commandArgs);

        if (potato.numHops > 0)
            throw std::runtime_error("Got passed a still hot potato");

        // if hops is zero, print messages, and shutdown

        std::cout << "Trace of potato:\n";

        bool first = true;
        for(auto id: potato.ids) {
            if (!first) std::cout << ",";
            first = false;
            std::cout << id;
        }
        std::cout << '\n';

        shutdown();
    }

    void onPlayerReady(size_t playerId, CommandPacket commandPacket) {

        // on the last player ready, start the game
        numPlayersReady++;

        std::cout << "Player " << playerId << " is ready to play\n";
        

        if (numPlayersReady == numPlayers) {
            size_t playerId = rand() % numPlayers;

            std::cout << "Ready to start the game, sending the potato to player " << playerId << '\n';

            if (numHops == 0) {
                shutdown();
                std::cout << "Trace of potato:\n\n";
                return;
            } else {
                CommandPacket packet;
                packet.author = -1;
                packet.commandType = CommandType::GIVE_POTATO;
                Potato potato;
                potato.numHops = numHops;
                packet.commandArgs = potato.serialize_to_vec();
                
                server.message(playerId, packet.serialize());
            }
        }
    }

    void onPlayerReportAddr(size_t playerId, CommandPacket commandPacket) {
        // on the last player report address, send out connection information
        numConnectedPlayersReadyServers++;

        std::pair<std::string, std::string> clientInfo = server.getClientInfo(playerId);
        playerHostNames[playerId] = commandPacket.commandArgs[0];
        playerPorts[playerId] = commandPacket.commandArgs[1];

        // to all players on the next player

        if (numConnectedPlayersReadyServers == numPlayers) {
            for(size_t curPlayerId = 0; curPlayerId < numPlayers; curPlayerId++) {
                size_t nextPlayerId = (curPlayerId+1) % numPlayers;

                CommandPacket packet;
                packet.author = -1;
                packet.commandType = CommandType::RINGMASTER_SET_NEXT;
                packet.commandArgs.push_back(std::to_string(nextPlayerId));
                
                packet.commandArgs.push_back(playerHostNames[nextPlayerId]);
                packet.commandArgs.push_back(playerPorts[nextPlayerId]);

                server.message(curPlayerId, packet.serialize());
            }
        }
    }

    bool isDone() {
        return done.load();
    }

private:

    void shutdown() {
        CommandPacket shutdownPacket;
        shutdownPacket.author = -1;
        shutdownPacket.commandType = CommandType::RINGMASTER_SHUTDOWN;
        for(size_t playerId = 0; playerId < numPlayers; playerId++) {
            server.message(playerId, shutdownPacket.serialize());
        }
        done.store(true);
        server.shutdown();
        #ifdef DEBUG
        std::cout << "Finished shutdown messaging\n";
        #endif
    }

    size_t numPlayersReady = 0;
    size_t numConnectedPlayersReadyServers = 0;
    size_t numConnectedPlayers = 0;
    size_t numPlayers;
    size_t numHops;

    std::string port;
    
    static constexpr size_t MAX_PLAYERS = 169;
    Server<MAX_PLAYERS> server;
    std::string playerHostNames[MAX_PLAYERS], playerPorts[MAX_PLAYERS];
    std::atomic<bool> done;

    std::mutex forcedSerialReceive;
};

