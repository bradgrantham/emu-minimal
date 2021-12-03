#include <algorithm>
#include <unordered_map>
#include <map>
#include <string>
#include <array>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <chrono>
#include <random>
#include <numeric>
#include <thread>

#ifdef __APPLE__
#define XCODE_MISSING_FILESYSTEM_FOR_YEARS
#include <libgen.h>
#else
#include <filesystem>
#endif

#include <MiniFB.h>

typedef uint64_t clk_t;

struct Clock
{
    clk_t rate;
    clk_t clocks;

    Clock(clk_t rate) :
        rate(rate),
        clocks(0)
    {}

    Clock(const Clock& clock) :
        rate(clock.rate), 
        clocks(clock.clocks)
    {}

    Clock(const Clock& clock, clk_t newClocks) :
        rate(clock.rate), 
        clocks(newClocks)
    {}

    Clock& operator=(const Clock& clock)
    {
        clocks = clock.clocks;
        rate = clock.rate;
        return *this;
    }

    Clock& operator+=(clk_t inc)
    {
        clocks += inc;
        return *this;
    }

    Clock operator+(clk_t inc) const
    {
        return Clock(rate, clocks + inc);
    }

    // operator clk_t() const { return clocks; }
};

constexpr int UIUpdateFrequency = 30;

// template <class MEMORY, class INTERFACE>
// struct Chip8Interpreter
// 
    // StepResult step(MEMORY& memory, INTERFACE& interface, const Clock& systemClock)

// struct Memory
// {
    // std::array<uint8_t, 65536> memory;

struct Interface
{
    bool succeeded{false};
    bool attemptIterate()
    {
        return true;
    }

    Interface()
    {
        succeeded = true;
    }

//     clk_t calculateNextActivity()
    // {
        // clk_t next = (mostRecentSystemClock.clocks + audioOutputSampleLengthInSystemClocks - 1) / audioOutputSampleLengthInSystemClocks * audioOutputSampleLengthInSystemClocks;
        // // XXX debug printf("interface next is %llu\n", next);
        // return next;
    // }

// Do not repeat work if called twice with same clock.
    // void updatePastClock(const Clock& systemClock)
};

void usage(const char *name)
{
    fprintf(stderr, "usage: %s flash.bin\n", name);
    // fprintf(stderr, "usage: %s [options] flash.bin\n", name);
    // fprintf(stderr, "options:\n");
    // fprintf(stderr, "\t--rate N           - issue N instructions per 60Hz field\n");
    // --clock mhz
}

int main(int argc, char **argv)
{
    const char *progname = argv[0];
    argc -= 1;
    argv += 1;

    while((argc > 1) && (argv[0][0] == '-')) {
        if(
            (strcmp(argv[0], "-help") == 0) ||
            (strcmp(argv[0], "-h") == 0) ||
            (strcmp(argv[0], "-?") == 0))
        {
            usage(progname);
            exit(EXIT_SUCCESS);
	} else {
	    fprintf(stderr, "unknown parameter \"%s\"\n", argv[0]);
            usage(progname);
	    exit(EXIT_FAILURE);
	}
    }

    if(argc < 1) {
        usage(progname);
        exit(EXIT_FAILURE);
    }

    std::string flash_file = argv[0];

    Clock systemClock(3686400);

    Interface interface{};
    if(!interface.succeeded) {
        fprintf(stderr, "opening the user interface failed.\n");
        exit(EXIT_FAILURE);
    }

    // Memory memory(platform);

    FILE *fp = fopen(flash_file.c_str(), "rb");
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> flashMemory;
    flashMemory.resize(size);
    fread(flashMemory.data(), size, 1, fp);
    fclose(fp);

    // Chip8Interpreter<Memory,Interface> chip8(0x200, platform, quirks, cpuClockRate, systemClock);

    std::chrono::time_point<std::chrono::system_clock> interfaceThen = std::chrono::system_clock::now();

    bool done = false;
    while(!done) {

        std::chrono::time_point<std::chrono::system_clock> interfaceNow = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(interfaceNow - interfaceThen);
        float dt = elapsed.count();
        if(dt > (.9f * 1.0f / UIUpdateFrequency)) {
            done = !interface.attemptIterate();
            interfaceThen = interfaceNow;
        }
    }
}
