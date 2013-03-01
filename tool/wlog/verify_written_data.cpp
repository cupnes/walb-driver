/**
 * @file
 * @brief verify data written by write_random_data.
 * @author HOSHINO Takashi
 *
 * (C) 2013 Cybozu Labs, Inc.
 */
#include <string>
#include <cstdio>
#include <stdexcept>
#include <cstdint>
#include <queue>
#include <memory>
#include <deque>
#include <algorithm>
#include <utility>
#include <set>
#include <limits>

#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <getopt.h>

#include "util.hpp"
#include "walb/common.h"
#include "walb/block_size.h"
#include "io_recipe.hpp"

/**
 * Command line configuration.
 */
class Config
{
private:
    unsigned int bs_; /* block size [byte] */
    bool isVerbose_;
    bool isHelp_;
    std::string recipePath_; /* recipe file path. */
    std::string targetPath_; /* device or file path. */
    std::vector<std::string> args_;

public:
    Config(int argc, char* argv[])
        : bs_(LOGICAL_BLOCK_SIZE)
        , isVerbose_(false)
        , isHelp_(false)
        , recipePath_("-")
        , targetPath_()
        , args_() {
        parse(argc, argv);
    }

    unsigned int blockSize() const { return bs_; }
    bool isVerbose() const { return isVerbose_; }
    bool isHelp() const { return isHelp_; }
    const std::string& targetPath() const { return targetPath_; }
    const std::string& recipePath() const { return recipePath_; }

    bool isDirect() const {
#if 1
        return false;
#else
        return blockSize() % LOGICAL_BLOCK_SIZE == 0;
#endif
    }

    void print() const {
        FILE *fp = ::stderr;
        ::fprintf(fp, "blockSize: %u\n"
                  "verbose: %d\n"
                  "isHelp: %d\n"
                  "recipe: %s\n"
                  "targetPath: %s\n",
                  blockSize(), isVerbose(), isHelp(),
                  recipePath_.c_str(), targetPath().c_str());
        int i = 0;
        for (const auto& s : args_) {
            ::fprintf(fp, "arg%d: %s\n", i++, s.c_str());
        }
    }

    static void printHelp() {
        ::printf("%s", generateHelpString().c_str());
    }

    void check() const {
        if (blockSize() == 0) {
            throwError("blockSize must be non-zero.");
        }
        if (targetPath().size() == 0) {
            throwError("specify target device or file.");
        }
    }

    class Error : public std::runtime_error {
    public:
        explicit Error(const std::string &msg)
            : std::runtime_error(msg) {}
    };

private:
    /* Option ids. */
    enum Opt {
        BLOCKSIZE = 1,
        RECIPEPATH,
        VERBOSE,
        HELP,
    };

    void throwError(const char *format, ...) const {
        va_list args;
        std::string msg;
        va_start(args, format);
        try {
            msg = walb::util::formatStringV(format, args);
        } catch (...) {}
        va_end(args);
        throw Error(msg);
    }

    template <typename IntType>
    IntType str2int(const char *str) const {
        return static_cast<IntType>(walb::util::fromUnitIntString(str));
    }

    void parse(int argc, char* argv[]) {
        while (1) {
            const struct option long_options[] = {
                {"blockSize", 1, 0, Opt::BLOCKSIZE},
                {"recipe", 1, 0, Opt::RECIPEPATH},
                {"verbose", 0, 0, Opt::VERBOSE},
                {"help", 0, 0, Opt::HELP},
                {0, 0, 0, 0}
            };
            int option_index = 0;
            int c = ::getopt_long(argc, argv, "b:i:vh", long_options, &option_index);
            if (c == -1) { break; }

            switch (c) {
            case Opt::BLOCKSIZE:
            case 'b':
                bs_ = str2int<unsigned int>(optarg);
                break;
            case Opt::RECIPEPATH:
            case 'i':
                recipePath_ = optarg;
                break;
            case Opt::VERBOSE:
            case 'v':
                isVerbose_ = true;
                break;
            case Opt::HELP:
            case 'h':
                isHelp_ = true;
                break;
            default:
                throwError("Unknown option.");
            }
        }

        while(optind < argc) {
            args_.push_back(std::string(argv[optind++]));
        }

        if (!args_.empty()) {
            targetPath_ = args_[0];
        }
    }

    static std::string generateHelpString() {
        return walb::util::formatString(
            "verify_written_data: verify data written by write_random_data.\n"
            "Usage: verify_written_data [options] [DEVICE|FILE]\n"
            "Options:\n"
            "  -b, --blockSize SIZE:  block size [byte]. (default: %u)\n"
            "  -i, --recipe PATH:     recipe file path. '-' for stdin. (default: '-')\n"
            "  -v, --verbose:         verbose messages to stderr.\n"
            "  -h, --help:            show this message.\n",
            LOGICAL_BLOCK_SIZE);
    }
};

class ReadDataVerifier
{
private:
    const Config &config_;
    walb::util::BlockDevice bd_;
    walb::util::Rand<unsigned int> randUint_;
    std::shared_ptr<char> block_;
public:
    ReadDataVerifier(const Config &config)
        : config_(config)
        , bd_(config.targetPath(), O_RDONLY | (config.isDirect() ? O_DIRECT : 0))
        , randUint_()
        , block_(getBlockStatic(config.blockSize(), config.isDirect())) {
        assert(block_);
    }

    void run() {
        const unsigned int bs = config_.blockSize();

        /* Get IO recipe parser. */
        std::shared_ptr<walb::util::FileOpener> fop;
        if (config_.recipePath() != "-") {
            fop.reset(new walb::util::FileOpener(config_.recipePath(), O_RDONLY));
        }
        int fd = 0;
        if (fop) { fd = fop->fd(); }
        walb::util::IoRecipeParser recipeParser(fd);

        /* Read and verify for each IO recipe. */
        while (!recipeParser.isEnd()) {
            walb::util::IoRecipe r = recipeParser.get();
            uint32_t csum = 0;
            for (uint64_t off = r.offsetB(); off < r.offsetB() + r.ioSizeB(); off++) {
                bd_.read(off * bs, bs, block_.get());
                csum = walb::util::checksumPartial(block_.get(), bs, csum);
            }
            csum = walb::util::checksumFinish(csum);
            ::printf("%s\t%s\t%08x\n",
                     (csum == r.csum() ? "OK" : "NG"), r.toString().c_str(), csum);
        }
    }

private:
    static std::shared_ptr<char> getBlockStatic(unsigned int blockSize, bool isDirect) {
        assert(0 < blockSize);
        if (isDirect) {
            return walb::util::allocateBlock<char>(blockSize, blockSize);
        } else {
            return std::shared_ptr<char>(reinterpret_cast<char *>(::malloc(blockSize)));
        }
    }
};

int main(int argc, char* argv[])
{
    try {
        Config config(argc, argv);
        /* config.print(); */
        if (config.isHelp()) {
            Config::printHelp();
            return 0;
        }
        config.check();

        ReadDataVerifier rdv(config);
        rdv.run();
        return 0;

    } catch (Config::Error& e) {
        ::printf("Command line error: %s\n\n", e.what());
        Config::printHelp();
        return 1;
    } catch (std::runtime_error& e) {
        LOGe("Error: %s\n", e.what());
        return 1;
    } catch (std::exception& e) {
        LOGe("Exception: %s\n", e.what());
        return 1;
    } catch (...) {
        LOGe("Caught other error.\n");
        return 1;
    }
}

/* end file. */