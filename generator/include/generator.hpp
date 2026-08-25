#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

struct GeneratorConfig {
    std::string uri = "tcp://localhost:9007/amps/json";
    std::string client_prefix = "ampsq-generator";
    std::string trade_topic = "trades";
    std::string quote_topic = "quotes";

    uint64_t rate = 100000;
    uint64_t duration_seconds = 60;
    uint64_t symbols = 100;
    uint64_t threads = 1;
    uint64_t seed = 42;

    double quote_ratio = 0.80;
    uint64_t stats_ms = 1000;
    uint64_t burst = 1;
};

struct GeneratorCounters {
    std::atomic<uint64_t> attempted{0};
    std::atomic<uint64_t> trades{0};
    std::atomic<uint64_t> quotes{0};
    std::atomic<uint64_t> publish_errors{0};
};

struct SymbolState {
    std::string symbol;
    std::string venue;
    double mid;
    int64_t bid_size;
    int64_t ask_size;
};

class TradeQuoteGenerator {
public:
    TradeQuoteGenerator(const GeneratorConfig& cfg,
                        GeneratorCounters& counters,
                        unsigned worker_id);

    void run(std::atomic<bool>& stop);

private:
    void connect();
    void publish_trade(uint64_t n);
    void publish_quote(uint64_t n);

    std::string make_trade(uint64_t n);
    std::string make_quote(uint64_t n);

    static std::string iso_timestamp_ns();
    static std::string json_escape(const std::string& s);

    GeneratorConfig cfg_;
    GeneratorCounters& counters_;
    unsigned worker_id_;

    std::unique_ptr<class AmpsPublisher> publisher_;
    std::vector<SymbolState> symbols_;

    std::mt19937_64 rng_;
    std::uniform_real_distribution<double> unit_{0.0, 1.0};
};
