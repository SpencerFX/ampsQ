#include "generator.hpp"

#include <amps/ampsplusplus.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>

using Clock = std::chrono::steady_clock;
using WallClock = std::chrono::system_clock;

class AmpsPublisher {
public:
    explicit AmpsPublisher(const std::string& name) : client_(name) {}

    void connect(const std::string& uri) {
        client_.connect(uri);
        client_.logon();
    }

    void publish(const std::string& topic, const std::string& data) {
        client_.publish(topic, data);
    }

    void flush() {
        client_.publishFlush(10000);
    }

private:
    AMPS::Client client_;
};

static std::string make_symbol(size_t i) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "SYM%04zu", i);
    return buf;
}

TradeQuoteGenerator::TradeQuoteGenerator(
    const GeneratorConfig& cfg,
    GeneratorCounters& counters,
    unsigned worker_id)
    : cfg_(cfg),
      counters_(counters),
      worker_id_(worker_id),
      rng_(cfg.seed + worker_id * 1000003ULL) {

    symbols_.reserve(cfg_.symbols);

    static const char *venues[] = {"XNAS", "XNYS", "BATS", "EDGX", "ARCX"};

    for (uint64_t i = 0; i < cfg_.symbols; ++i) {
        SymbolState s;
        s.symbol = make_symbol(i);
        s.venue = venues[i % 5];
        s.mid = 50.0 + static_cast<double>(i % 1000) * 0.25;
        s.bid_size = 100 + static_cast<int64_t>((i * 37) % 900);
        s.ask_size = 100 + static_cast<int64_t>((i * 53) % 900);
        symbols_.push_back(std::move(s));
    }
}

std::string TradeQuoteGenerator::iso_timestamp_ns() {
    const auto now = WallClock::now();
    const auto tt = WallClock::to_time_t(now);
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count() % 1000000000LL;

    struct tm tm{};
    gmtime_r(&tt, &tm);

    char buf[64];
    std::snprintf(buf, sizeof(buf),
                  "%04d-%02d-%02dT%02d:%02d:%02d.%09lldZ",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec,
                  static_cast<long long>(ns));
    return buf;
}

std::string TradeQuoteGenerator::json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);

    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string TradeQuoteGenerator::make_quote(uint64_t n) {
    auto& s = symbols_[n % symbols_.size()];

    const double move = (unit_(rng_) - 0.5) * 0.04;
    s.mid = std::max(0.01, s.mid + move);

    const double spread = 0.01 + unit_(rng_) * 0.04;
    const double bid = s.mid - spread / 2.0;
    const double ask = s.mid + spread / 2.0;

    s.bid_size = 100 + static_cast<int64_t>(unit_(rng_) * 1900);
    s.ask_size = 100 + static_cast<int64_t>(unit_(rng_) * 1900);

    std::ostringstream o;
    o.setf(std::ios::fixed);
    o.precision(6);

    o << "{\"ts\":\"" << iso_timestamp_ns()
      << "\",\"sym\":\"" << json_escape(s.symbol)
      << "\",\"bid\":" << bid
      << ",\"ask\":" << ask
      << ",\"bidSize\":" << s.bid_size
      << ",\"askSize\":" << s.ask_size
      << ",\"venue\":\"" << s.venue
      << "\",\"seq\":" << n
      << "}";

    return o.str();
}

std::string TradeQuoteGenerator::make_trade(uint64_t n) {
    auto& s = symbols_[n % symbols_.size()];

    const double spread = 0.01 + unit_(rng_) * 0.04;
    const double bid = s.mid - spread / 2.0;
    const double ask = s.mid + spread / 2.0;

    const bool buy = unit_(rng_) < 0.5;
    const double price = buy
        ? ask + unit_(rng_) * 0.005
        : bid - unit_(rng_) * 0.005;

    const int64_t size = 100 * (1 + static_cast<int64_t>(unit_(rng_) * 20));

    std::ostringstream o;
    o.setf(std::ios::fixed);
    o.precision(6);

    o << "{\"ts\":\"" << iso_timestamp_ns()
      << "\",\"sym\":\"" << json_escape(s.symbol)
      << "\",\"price\":" << price
      << ",\"size\":" << size
      << ",\"side\":\"" << (buy ? "B" : "S")
      << "\",\"venue\":\"" << s.venue
      << "\",\"seq\":" << n
      << "}";

    return o.str();
}

void TradeQuoteGenerator::connect() {
    publisher_ = std::make_unique<AmpsPublisher>(
        cfg_.client_prefix + "-" + std::to_string(worker_id_));
    publisher_->connect(cfg_.uri);
}

void TradeQuoteGenerator::publish_trade(uint64_t n) {
    try {
        publisher_->publish(cfg_.trade_topic, make_trade(n));
        counters_.trades.fetch_add(1, std::memory_order_relaxed);
        counters_.attempted.fetch_add(1, std::memory_order_relaxed);
    } catch (...) {
        counters_.publish_errors.fetch_add(1, std::memory_order_relaxed);
    }
}

void TradeQuoteGenerator::publish_quote(uint64_t n) {
    try {
        publisher_->publish(cfg_.quote_topic, make_quote(n));
        counters_.quotes.fetch_add(1, std::memory_order_relaxed);
        counters_.attempted.fetch_add(1, std::memory_order_relaxed);
    } catch (...) {
        counters_.publish_errors.fetch_add(1, std::memory_order_relaxed);
    }
}

void TradeQuoteGenerator::run(std::atomic<bool>& stop) {
    connect();

    const uint64_t target = cfg_.rate == 0
        ? 0
        : std::max<uint64_t>(1, cfg_.rate / std::max<uint64_t>(1, cfg_.threads));

    const auto start = Clock::now();
    auto next = start;
    uint64_t local = 0;

    while (!stop.load(std::memory_order_relaxed)) {
        if (cfg_.duration_seconds &&
            std::chrono::duration_cast<std::chrono::seconds>(
                Clock::now() - start).count() >=
                static_cast<long long>(cfg_.duration_seconds)) {
            break;
        }

        for (uint64_t b = 0; b < cfg_.burst &&
             !stop.load(std::memory_order_relaxed); ++b) {
            const bool quote = unit_(rng_) < cfg_.quote_ratio;
            if (quote)
                publish_quote(local);
            else
                publish_trade(local);
            ++local;
        }

        if (target == 0)
            continue;

        const auto elapsed = Clock::now() - start;
        const uint64_t produced = local;
        const auto target_ns =
            std::chrono::nanoseconds(
                static_cast<long long>((1e9 * produced) / target));

        next = start + target_ns;

        const auto now = Clock::now();
        if (next > now)
            std::this_thread::sleep_until(next);
    }

    try {
        publisher_->flush();
    } catch (...) {
    }
}
