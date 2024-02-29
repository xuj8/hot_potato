
#include "server.h"
#include "client.h"
#include "commands.h"
#include <mutex>
#include <chrono>
#include <cstdlib>

class Player {

public:

    Player(std::string _hostname, std::string _port) {
        // initialize the server
        char tmp[1024];
        memset(tmp, 0, sizeof(tmp));
        gethostname(tmp, 1023);
        selfHostName = std::string(tmp);
        #ifdef DEBUG
        std::cout << "hostname obtained as " << selfHostName << '\n';
        #endif
        ringmasterPort = _port;
        ringmasterHostName = _hostname;
        ringmasterClient = Client(std::bind(&Player::onMessage, this, std::placeholders::_1), ringmasterHostName, ringmasterPort);
        done.store(false);
    }

    ~Player() {
        
    }

    void start() {
        ringmasterClient.start();

        CommandPacket initialPacket = initialPlayerMessage();

        ringmasterClient.message(initialPacket.serialize());
    }

    void onServerMessage(size_t playerId, std::string message) {
        onMessage(message);
    }

    void onMessage(std::string message) {
        std::unique_lock<std::mutex> lock(forcedSerialReceive);
        onCommand(CommandPacket::deserialize(message));
    };

    void onCommand(CommandPacket commandPacket) {
        switch(commandPacket.commandType) {
            case CommandType::RINGMASTER_SET_NEXT:
                onRingmasterSetNext(std::move(commandPacket));
                break;

            case CommandType::RINGMASTER_ASSIGN_ID_PORT:
                onRingmasterAssignIdPort(std::move(commandPacket));
                break;
            
            case CommandType::RINGMASTER_SHUTDOWN:
                onRingmasterShutdown(std::move(commandPacket));
                break;
            
            case CommandType::GIVE_POTATO:
                onReceivePotato(std::move(commandPacket));
                break;
            
            case CommandType::PLAYER_REGISTER: 
            case CommandType::PLAYER_READY:
            case CommandType::PLAYER_REPORT_ADDR:
                throw std::runtime_error("Error, received player command");
                break;

            default:
                throw std::runtime_error("Error, unhandled command.");
                break;
        }
    };

    static CommandPacket initialPlayerMessage() {
        return CommandPacket{69420, CommandType::PLAYER_REGISTER, {}};
    }

    void onRingmasterSetNext(CommandPacket commandPacket) {
        
        // set the next host name, and next port
        nextPlayerPort = commandPacket.commandArgs[2];
        nextPlayerHostName = commandPacket.commandArgs[1];
        nextId = stoi(commandPacket.commandArgs[0]);
        
        #ifdef DEBUG
        std::cout << "Attempting to connect to next player hostname: " << nextPlayerHostName << ":" << nextPlayerPort << '\n';
        #endif

        // connect to next
        nextPlayerClient = Client(std::bind(&Player::onMessage, this, std::placeholders::_1),
                                  nextPlayerHostName,
                                  nextPlayerPort);

        if (nextPlayerClient.start() != 0)
            throw std::runtime_error("Unable to connect to next client");

        // if connection successful, then wait for previous to connect
        while(selfServer.getNumConnections() < 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // upon previous connecting, report ready
        CommandPacket packet;

        packet.author = id;
        packet.commandType = CommandType::PLAYER_READY;

        ringmasterClient.message(packet.serialize());
    }

    void onReceivePotato(CommandPacket commandPacket) {

        Potato potato = Potato::parsePotato(commandPacket.commandArgs);

        // if hops is zero, that's an error
        // otherwise, decrement hops and add self
        // if hops = 0, send to ringmaster
        // else randomly choose next or previous and send it to them
        
        

        if (potato.numHops == 0) {
            throw std::runtime_error("Player should not be receiving cold potato\n");
        } else {
            potato.numHops--;
            potato.ids.push_back(id);

            CommandPacket packet;
            packet.author = id;
            packet.commandType = CommandType::GIVE_POTATO;
            packet.commandArgs = potato.serialize_to_vec();

            if (potato.numHops == 0) {
                std::cout << "I'm it\n";
                ringmasterClient.message(packet.serialize());
            } else {
                bool forward = (bool) (rand() % 2);

                if (forward) {
                    std::cout << "Sending potato to " << nextId << '\n';
                    nextPlayerClient.message(packet.serialize());

                } else {
                    std::cout << "Sending potato to " << prevId << '\n';
                    selfServer.message(0, packet.serialize());
                }
            }
            
        }
    }

    void onRingmasterAssignIdPort(CommandPacket commandPacket) {

        // assign self id
        id = stoi(commandPacket.commandArgs[0]);
        selfPort = commandPacket.commandArgs[1];
        prevId = stoi(commandPacket.commandArgs[2]);
        totNumPlayers = stoi(commandPacket.commandArgs[3]);

        std::cout << "Connected as player " << id << " out of " << totNumPlayers << " total players\n";
        
        // start a server at the port
        selfServer = Server<1>(std::bind(&Player::onServerMessage, this, std::placeholders::_1, std::placeholders::_2), selfPort);

        if (selfServer.start() != 0) {
            throw std::runtime_error("Did not succesfully start self server");
        };

        // obtain actual port and ip, and send back to ringmaster
        std::pair<std::string, std::string> ipAndPort = selfServer.getServerInfo();

        CommandPacket packet;

        packet.author = this->id;
        packet.commandType = CommandType::PLAYER_REPORT_ADDR;

        #ifdef DEBUG
        std::cout << "Attempting to get host name\n";

        #endif
        
        // char hostname[] = "127.0.0.1";
        // char hostname[] = "vcm-39463.vm.duke.edu";

    
        packet.commandArgs.push_back(selfHostName);
        packet.commandArgs.push_back(ipAndPort.second);

        #ifdef DEBUG
        std::cout << "Attempting to send message..." << '\n';
        #endif

        ringmasterClient.message(packet.serialize());

        #ifdef DEBUG
        std::cout << "Self hostname and port sent to ringmaster\n";
        #endif
    }

    void onRingmasterShutdown(CommandPacket commandPacket) {

        #ifdef DEBUG
        std::cout << "Ringmaster requested shutdown\n";
        #endif

        // set done and return
        ringmasterClient.shutdown();
        selfServer.shutdown();
        nextPlayerClient.shutdown();

        done.store(true);
    }

    bool isDone() {
        return done.load();
    }

private:
    size_t id, nextId, prevId, totNumPlayers;
    std::string ringmasterHostName, nextPlayerHostName, selfHostName;
    std::string ringmasterPort, nextPlayerPort, selfPort;

    Server<1> selfServer;
    Client ringmasterClient, nextPlayerClient;
    std::atomic<bool> done;

    std::mutex forcedSerialReceive;
};

