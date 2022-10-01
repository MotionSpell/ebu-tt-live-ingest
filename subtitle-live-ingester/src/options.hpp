#pragma once

#include <string>
#include "plugins/SocketInput/socket_input.hpp"

struct Config {
	bool help = false;
	SocketInputConfig sockInCfg {};
	std::string output;
	std::string format;
	bool legacy = false;
	int segDurInMs = 2000;
};
