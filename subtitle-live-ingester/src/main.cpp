#include "lib_utils/format.hpp"
#include "lib_utils/profiler.hpp"
#include "lib_pipeline/pipeline.hpp"
#include "lib_appcommon/options.hpp"
#include "options.hpp"
#include <iostream>

const char *g_appName = "subtitle-live-ingester";

std::unique_ptr<Pipelines::Pipeline> buildPipeline(Config&);
static Pipelines::Pipeline *g_Pipeline = nullptr;

namespace {
Config parseCommandLine(int argc, char const* argv[]) {
	Config cfg;
	cfg.sockInCfg.isTcp = true;

	CmdLineOptions opt;
	opt.add("o", "output", &cfg.output, "Output filename. The parent directory must exist.");
	//opt.add("s", "segment-duration-in-ms", &cfg.segDurInMs, "Segment duration in milliseconds");
	opt.add("f", "subtitle-format", &cfg.format, "Output subtitle format: \"ttml\", \"webvtt\", or \"both\"");
	opt.addFlag("h", "help", &cfg.help, "Print usage and exit.");

	auto urls = opt.parse(argc, argv);

	if(cfg.help) {
		opt.printHelp();
		return cfg;
	}

	if (cfg.output.empty())
		throw std::runtime_error("no output: invalid command line, use --help");

	if (urls.size() != 1)
		throw std::runtime_error(format("%s inputs found but shall be only 1: invalid command line, use --help", urls.size()).c_str());

	if (cfg.segDurInMs <= 0)
		throw std::runtime_error("invalid segment duration, must be positive: invalid command line, use --help");

	if (cfg.format != "ttml" && cfg.format != "webvtt")
		throw std::runtime_error("invalid subtitle format, only \"ttml\" is implemented");

	sscanf(urls[0].c_str(), "%d.%d.%d.%d:%d",
	    &cfg.sockInCfg.ipAddr[0],
	    &cfg.sockInCfg.ipAddr[1],
	    &cfg.sockInCfg.ipAddr[2],
	    &cfg.sockInCfg.ipAddr[3],
	    &cfg.sockInCfg.port);

	std::cerr << "Detected options:\n"
	    "\tinput                 =\"" << urls[0] << "\"\n"
	    "\toutput                =" << cfg.output << "\n"
	    "\tsegment-duration-in-ms=" << cfg.segDurInMs << "\n"
	    "\tsubtitle format       =" << cfg.format << "\n";

	return cfg;
}
}

void safeStop() {
	if (g_Pipeline) {
		g_Pipeline->exitSync();
		g_Pipeline = nullptr;
	}
}

void safeMain(int argc, const char* argv[]) {
	auto cfg = parseCommandLine(argc, argv);
	if(cfg.help)
		return;

	auto pipeline = buildPipeline(cfg);
	g_Pipeline = pipeline.get();

	{
		Tools::Profiler profilerProcessing(format("%s - processing time", g_appName));
		pipeline->start();
		pipeline->waitForEndOfStream();
	}
}
