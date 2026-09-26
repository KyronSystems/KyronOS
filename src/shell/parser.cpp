#include "shell/parser.hpp"

namespace kyron::shell {
std::vector<std::string> parse(const std::string& input) {
    std::vector<std::string> result;
    std::string token;
    bool quoted = false;
    for (char character : input) {
        if (character == '"') { quoted = !quoted; continue; }
        if (character == ' ' && !quoted) {
            if (!token.empty()) { result.push_back(token); token.clear(); }
        } else token += character;
    }
    if (!token.empty()) result.push_back(token);
    return result;
}
}
