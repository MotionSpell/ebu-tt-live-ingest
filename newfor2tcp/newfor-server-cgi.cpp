// Newfor server simulator
#include <cstdarg>
#include <cstdio>
#include <string>
#include <stdexcept>

struct NewForServerMockup {
    NewForServerMockup() {
        read = fread(buf, 1, sizeof(buf), stdin);
        if (read < 1)
            throw std::runtime_error("TCP data empty");
        if (read == sizeof(buf))
            throw std::runtime_error(std::string("buffer may be too small, received " + std::to_string(read) + " bytes").c_str());
    }

    void processRequest() {
        switch(buf[i]) {
            case 0x0E:
                fprintf(stderr, "page init\n");
                if (nextByte() != 0x15)
                    throw std::runtime_error("misformed page init");
                i += processPageInit();
                break;
            case 0x0F:
                fprintf(stderr, "subtitle data\n");
                i += processSubtitleData();
                break;
            case 0x10:
                fprintf(stderr, "on air\n");
                break;
            case 0x18:
                fprintf(stderr, "off air\n");
                break;
            default:
                fprintf(stderr, "Unknown command %X\n", buf[0]);
                break;
        }
    }

private:
    uint8_t buf[2048] {};
    size_t read;
    int i = 0;

    enum {
        ACK = 0x06,
        NACK = 0x15
    };

    static void sendLine(const char *format, ...) {
        va_list args;
        va_start(args, format);
        vfprintf(stdout, format, args);
        va_end(args);
        fprintf(stdout, "\r\n");
    }

    uint8_t nextByte() {
        if (i + 1 < read)
            throw std::runtime_error("can parse next byte");

        ++i;
        return buf[i];
    };

    int processPageInit() {
        fprintf(stderr, "parsed %u", nextByte()); //hh
        fprintf(stderr, " %u", nextByte()); //tt
        fprintf(stderr, " %u", nextByte()); //uu
        fprintf(stderr, "\n");
        sendLine("%d", ACK);
        return 0;
    }

    int processSubtitleData() {
        //TODO - for the moment ignore the buffer
        sendLine("%d", ACK);
        return 0;
    }
};

int safeMain() {
    NewForServerMockup server;
    server.processRequest();
    return 0;
}

int main() {
    try {
        return safeMain();
    } catch(std::exception const& e) {
		fprintf(stderr, "Error: %s\n", e.what());
		return 1;
	}
}
