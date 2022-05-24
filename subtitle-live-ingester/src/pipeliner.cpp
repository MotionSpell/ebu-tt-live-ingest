#include "lib_pipeline/pipeline.hpp"
#include "lib_utils/os.hpp" // dirExists, mkdir
#include "lib_utils/time.hpp"
#include "lib_media/common/attributes.hpp"
#include "options.hpp"
#include <cassert>
#include <thread>

// modules
#include "lib_media/out/file.hpp"
#include "plugins/SubtitleEncoder/subtitle_encoder.hpp"
#include "plugins/TtmlDecoder/ttml_decoder.hpp"

using namespace Modules;
using namespace Pipelines;
using namespace std;

extern const char *g_appName;

namespace {
void ensureDir(const std::string &path) {
	if(!path.empty() && !dirExists(path))
		mkdir(path);
}

struct Logger : LogSink {
		void send(Level level, const char *msg) override {
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
			: playlistFn(playlistFn), segDurInMs(segDurInMs) {
			auto const subdir = playlistFn.substr(0, playlistFn.find_last_of("/") + 1);
			ensureDir(subdir);
		}

		void processOne(Data data) override {
			char buf[512] = {};

			// write data
			{
				auto subdir = playlistFn.substr(0, playlistFn.find_last_of("/") + 1);
				if (subdir.empty())
					subdir = ".";
				snprintf(buf, sizeof(buf), "%s/default_msg_%d.xml", subdir.c_str(), seqCounter);

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
				fprintf(f, "%s", buf);
				fclose(f);
			}

			seqCounter++;
		}

	private:
		std::string playlistFn;
		int segDurInMs = 0;
		int seqCounter = 0;
};

struct HeartBeat : Module {
		HeartBeat(KHost *host) {
			host->activate(true);
			addOutput();
		}
		void process() override {
			auto const now = g_SystemClock->now();
			if (now - lastNow >= maxRefresh) {
				auto out = std::make_shared<DataRaw>(0);
				out->set(PresentationTime{fractionToClock(now)});
				outputs[0]->post(out);
				lastNow = now;
			}

			std::this_thread::sleep_for(20ms);
		}

	private:
		Fraction const maxRefresh = Fraction(500, 1000); /*500ms*/
		Fraction lastNow;
};
}

std::unique_ptr<Pipeline> buildPipeline(Config &cfg) {
	static Logger logger;
	logger.m_logLevel = Info;
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
	subEncCfg.maxDelayBeforeEmptyInMs = subEncCfg.splitDurationInMs = cfg.segDurInMs;
	//Romain: subEncCfg.lang = ???;
	subEncCfg.forceEmptyPage = true;
	if (cfg.format == "ttml") {
		subEncCfg.timingPolicy = SubtitleEncoderConfig::AbsoluteUTC;
	} else {
		assert(cfg.format == "webvtt");
		subEncCfg.isWebVTT = true;
		subEncCfg.timingPolicy = SubtitleEncoderConfig::RelativeToMedia;
	}
	subEncCfg.utcStartTime = &utcStartTime;
	auto subEnc = pipeline->add("SubtitleEncoder", &subEncCfg);
	pipeline->connect(source, subEnc);
	source = subEnc;

	// heartbeat to flush output subtitles even when there is no input
	auto heartbeater = pipeline->addModule<HeartBeat>();
	pipeline->connect(heartbeater, subEnc, true);

	// push to the ever-growing file
	auto sink = pipeline->addModule<EverGrowingPlaylistSink>(cfg.output, cfg.segDurInMs);
	pipeline->connect(source, sink);

	utcStartTime.startTime = fractionToClock(getUTC());

	return pipeline;
}
