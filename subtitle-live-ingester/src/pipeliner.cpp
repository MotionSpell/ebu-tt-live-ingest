#include "lib_pipeline/pipeline.hpp"
#include "lib_utils/time.hpp"
#include "options.hpp"
#include <cassert>

// modules
#include "lib_media/out/file.hpp"
#include "plugins/SubtitleEncoder/subtitle_encoder.hpp"
#include "plugins/TtmlDecoder/ttml_decoder.hpp"

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

struct EverGrowingPlaylistSink : ModuleS {
		EverGrowingPlaylistSink(KHost*, const std::string &playlistFn, int segDurInMs)
			: playlistFn(playlistFn), segDurInMs(segDurInMs) {}

		void processOne(Data data) override {
			char buf[64] = {};

			// write data
			{
				snprintf(buf, sizeof(buf), "default_msg_%d.xml", seqCounter);

				auto f = fopen(buf, "wt");
				if (!f) {
					std::string msg("Can't open data sequence file: ");
					msg += playlistFn;
					throw error(msg.c_str());
				}
				fwrite(data->data().ptr, 1, data->data().len, f);
				fclose(f);
			}

			// add to playlist
			{
				int64_t t = seqCounter * segDurInMs;
				auto const msec = int(t % 1000);
				t /= 1000;
				auto const sec = int(t % 60);
				t /= 60;
				auto const min = int(t % 60);
				t /= 60;
				auto const hour = int(t);

				snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d,default_msg_%d.xml\n", hour, min, sec, msec, seqCounter);

				auto f = fopen(playlistFn.c_str(), "at");
				if (!f) {
					std::string msg("Can't open output playlist file: ");
					msg += playlistFn;
					throw error(msg.c_str());
				}
				fprintf(f, buf);
				fclose(f);
			}
		}

	private:
		std::string playlistFn;
		int segDurInMs = 0;
		int seqCounter = 0;
};
}

std::unique_ptr<Pipeline> buildPipeline(Config &cfg) {
	static Logger logger;
	auto pipeline = std::make_unique<Pipeline>(&logger);
	IFilter *source = nullptr;

	// input
	cfg.sockInCfg.isTcp = true;
	cfg.sockInCfg.isMulticast = false;
	source = pipeline->add("SocketInput", &cfg.sockInCfg);

	// extract text and add realtime timestamp
	TtmlDecoderConfig ttmlDecCfg;
	auto ttmlDec = pipeline->add("TTMLDecoder", &ttmlDecCfg);
	pipeline->connect(source, ttmlDec);
	source = ttmlDec;

	// segment and serialize
	SubtitleEncoderConfig subEncCfg;
	subEncCfg.splitDurationInMs = cfg.segDurInMs;
	subEncCfg.maxDelayBeforeEmptyInMs = 30000; //Romain: given by Andreas, but shouldn't we put sth shorter here to avoid missing content? // Romain: is present in EBU TT Live source
	//Romain: subEncCfg.lang = ???;
	if (cfg.format == "ttml") {
		subEncCfg.timingPolicy = SubtitleEncoderConfig::AbsoluteUTC; //Romain: should be, right?
	} else {
		assert(cfg.format == "webvtt");
		subEncCfg.isWebVTT = true;
		subEncCfg.timingPolicy = SubtitleEncoderConfig::RelativeToMedia;
	}
	subEncCfg.utcStartTime = &utcStartTime;
	auto subEnc = pipeline->add("SubtitleEncoder", &subEncCfg);
	pipeline->connect(source, subEnc);
	source = subEnc;

	// push to the ever-growing file
	//TODO add test: 00:00:00.000,default_msg_1.xml
	auto sink = pipeline->addModule<EverGrowingPlaylistSink>(cfg.output, cfg.segDurInMs);
	pipeline->connect(source, sink);

	utcStartTime.startTime = fractionToClock(getUTC());

	return pipeline;
}
