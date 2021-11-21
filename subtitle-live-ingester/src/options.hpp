#pragma once

#include <string>
#include "plugins/SocketInput/socket_input.hpp"

struct Config {
	bool help = false;
	SocketInputConfig sockInCfg {};
	std::string output;
	std::string format;
	int segDurInMs = 2000;
};
