#ifndef COMMANDS
#define COMMANDS

#include "client.h"
#include "server.h"
#include <string>
#include <iostream>
#include <sstream>

#define TOKEN_DELIM '_'
#define VECTOR_DELIM ','

/*
Command conventions:

PlayerRegister:
    player id is by default 69420
    Args: None

PlayerReady:
    Args: None

Player_Report_Addr:
    Args: IP: string - string
          port: string - string

Give_Potato:
    Args: num hops left: string - string
          history: vector<int> - string

Ringmaster_Set_Next:
    Args: next id: size_t - string
          next hostname: string - string
          next port: string - string

Ringmaster_Assign_Id_Port:
    Args: player id: size_t - string
          player port: string - string
          prev id: size_t - string
          tot_players: size_t - string

Ringmaster_Shutdown:
    Args: None
*/

enum class CommandType {
    PLAYER_REGISTER = 1,
    PLAYER_READY = 2,
    PLAYER_REPORT_ADDR = 3,
    GIVE_POTATO = 4,
    RINGMASTER_SET_NEXT = 5,
    RINGMASTER_ASSIGN_ID_PORT = 6,
    RINGMASTER_SHUTDOWN = 7,
};

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    if (str.back() == delimiter)
        tokens.push_back("");
    return tokens;
}

std::string concatenate(const std::vector<std::string>& tokens, char delimiter) {
    if (tokens.empty()) return "";

    std::string result = tokens[0];
    for(size_t i = 1; i < tokens.size(); i++) {
        result += delimiter;
        result += tokens[i];
    }

    return result;
}

std::string serialize_vector(const std::vector<size_t>& ids, char delimiter) {
    if (ids.empty()) return "";
    std::ostringstream oss;
    for (size_t i = 0; i < ids.size(); ++i) {
        oss << ids[i];
        if (i < ids.size() - 1) {
            oss << delimiter;
        }
    }
    return oss.str();
}

std::vector<size_t> deserialize_vector(const std::string& str, char delimiter) {
    if (str.empty()) return {};
    std::vector<size_t> ids;
    std::istringstream iss(str);
    std::string token;
    while (std::getline(iss, token, delimiter)) {
        ids.push_back(std::stoull(token));
    }
    return ids;
}

struct CommandPacket {
    int author;
    CommandType commandType;
    std::vector<std::string> commandArgs;

    std::string serialize() {
        std::vector<std::string> tokens;

        tokens.resize(2 + commandArgs.size());
        tokens[0] = std::to_string(author);
        tokens[1] = std::to_string((int) commandType);
        copy(commandArgs.begin(), commandArgs.end(), tokens.begin()+2);

        return concatenate(tokens, TOKEN_DELIM);
    }

    static CommandPacket deserialize(std::string str) {
        CommandPacket packet;

        std::vector<std::string> tokens = split(str, TOKEN_DELIM);

        packet.author = stoi(tokens[0]);
        packet.commandType = (CommandType) stoi(tokens[1]);
        packet.commandArgs = std::vector<std::string>(tokens.begin()+2, tokens.end());

        return packet;
    }
};

// Give_Potato:
//     Args: num hops left: string - string
//           history: vector<int> - string

struct Potato {
    size_t numHops;
    std::vector<size_t> ids;

    static Potato parsePotato(const std::vector<std::string>& args) {
        Potato potato;
        potato.numHops = stoi(args[0]);
        potato.ids = deserialize_vector(args[1], VECTOR_DELIM);

        return potato;
    }

    std::vector<std::string> serialize_to_vec() {
        std::vector<std::string> potatoSerialized;

        potatoSerialized.push_back(std::to_string(numHops));
        potatoSerialized.push_back(serialize_vector(ids, VECTOR_DELIM));

        return potatoSerialized;
    }

};

#endif



