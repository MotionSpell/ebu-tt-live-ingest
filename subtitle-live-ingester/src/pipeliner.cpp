#include "lib_pipeline/pipeline.hpp"
#include "lib_utils/system_clock.hpp"
#include "lib_utils/time.hpp"
#include "options.hpp"

// modules
#include "lib_media/out/file.hpp"
#include "plugins/TtmlEncoder/ttml_encoder.hpp"

using namespace Modules;
using namespace Pipelines;

extern const char *g_appName;

namespace {
struct Logger : LogSink {
	void send(Level level, const char *msg) override {
		if (level == Level::Debug)
			return;

		auto const now = (double)g_SystemClock->now();
		fprintf(stderr, "[%s][%.1f][%s#%p][%s]%s\n", getTime().c_str(), now,
				g_appName, this, getLogLevelName(level), msg);
		fflush(stderr);
	}

private:
	const char *getLogLevelName(Level level) {
		switch (level) {
		case Level::Debug:   return "debug";
		case Level::Info:    return "info";
		case Level::Warning: return "warning";
		case Level::Error:   return "error";
		default:             return "internal error";
		}
	}

	std::string getTime() {
		char szOut[255];
		const std::time_t t = std::time(nullptr);
		const std::tm tm = *std::gmtime(&t);
		auto const size = strftime(szOut, sizeof szOut, "%Y/%m/%d %H:%M:%S", &tm);
		auto timeString = std::string(szOut, size);
		snprintf(szOut, sizeof szOut, "%s", timeString.c_str());
		return szOut;
	}
};

struct UtcStartTime : IUtcStartTimeQuery {
	uint64_t query() const override {
		return startTime;
	}
	uint64_t startTime;
};
UtcStartTime utcStartTime;
}

std::unique_ptr<Pipeline> buildPipeline(Config &cfg) {
	static Logger logger;
	auto pipeline = std::make_unique<Pipeline>(&logger);
	IFilter *source = nullptr;

	// input
	SocketInputConfig mcast;
	mcast.isTcp = false;
	mcast.isMulticast = true;
	source = pipeline->add("SocketInput", &mcast);

	//Romain: do we need to ensure framing?
	//Romain: we need to wait for a 30s timeout? does that forces us to stay awake

	// extract text and add realtime timestamp
	//Romain: this means: map it to Page
	//#include "lib_media/common/subtitle.hpp"

	//Romain: send heartbeat from clock?
	// segment and serialize
	TtmlEncoderConfig ttmlCfg;
	ttmlCfg.splitDurationInMs = cfg.segDurInMs;
	ttmlCfg.maxDelayBeforeEmptyInMs = 30000; //Romain: given by Andreas, but shouldn't we put sth shorter here to avoid missing content?
	ttmlCfg.timingPolicy = TtmlEncoderConfig::AbsoluteUTC; //Romain: should be, right?
	ttmlCfg.utcStartTime = &utcStartTime;
	auto ttmlEnc = pipeline->add("TTMLEncoder", &ttmlCfg);
	pipeline->connect(source, ttmlEnc);
	source = ttmlEnc;

	// push to the ever-growing file
	//Romain
	auto sink = pipeline->addModule<Out::File>(cfg.output);
	pipeline->connect(source, sink);

	utcStartTime.startTime = fractionToClock(getUTC());

	return pipeline;
}
