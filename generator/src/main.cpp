#include "generator.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

static std::atomic<bool> stop_requested{false};

static void on_signal(int) {
    stop_requested.store(true, std::memory_order_relaxed);
}

static void usage(const char *p) {
    std::cout <<
R"(Usage:
  )" << p << R"( [options]

Options:
  --uri URI                 AMPS URI
  --client-prefix NAME      AMPS client-name prefix
  --trade-topic NAME        Trade topic (default trades)
  --quote-topic NAME        Quote topic (default quotes)
  --rate N                  Total target messages/sec, 0=max throughput
  --duration N              Seconds, 0=run until Ctrl-C
  --threads N               Publisher clients
  --symbols N               Number of symbols
  --quote-ratio X           Quote fraction [0,1]
  --seed N                  Random seed
  --stats-ms N              Statistics interval
  --burst N                 Messages per scheduler tick
  --help                    Show help
)";
}

static uint64_t u64(const char *s) {
    return std::strtoull(s, nullptr, 10);
}

static double dbl(const char *s) {
    return std::strtod(s, nullptr);
}

int main(int argc, char **argv) {
    GeneratorConfig cfg;

    for (int i = 1; i < argc; ++i) {
        std::string a(argv[i]);

        auto value = [&](const char *name) -> const char * {
            if (a == name && i + 1 < argc)
                return argv[++i];
            return nullptr;
        };

        if (const char *v = value("--uri")) cfg.uri = v;
        else if (const char *v = value("--client-prefix")) cfg.client_prefix = v;
        else if (const char *v = value("--trade-topic")) cfg.trade_topic = v;
        else if (const char *v = value("--quote-topic")) cfg.quote_topic = v;
        else if (const char *v = value("--rate")) cfg.rate = u64(v);
        else if (const char *v = value("--duration")) cfg.duration_seconds = u64(v);
        else if (const char *v = value("--threads")) cfg.threads = u64(v);
        else if (const char *v = value("--symbols")) cfg.symbols = u64(v);
        else if (const char *v = value("--quote-ratio")) cfg.quote_ratio = dbl(v);
        else if (const char *v = value("--seed")) cfg.seed = u64(v);
        else if (const char *v = value("--stats-ms")) cfg.stats_ms = u64(v);
        else if (const char *v = value("--burst")) cfg.burst = u64(v);
        else if (a == "--help") {
            usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << a << "\n";
            usage(argv[0]);
            return 2;
        }
    }

    if (cfg.threads == 0 || cfg.symbols == 0 || cfg.burst == 0 ||
        cfg.quote_ratio < 0.0 || cfg.quote_ratio > 1.0) {
        std::cerr << "Invalid configuration\n";
        return 2;
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    GeneratorCounters counters;
    std::vector<std::thread> workers;
    workers.reserve(cfg.threads);

    const auto start = std::chrono::steady_clock::now();

    for (uint64_t i = 0; i < cfg.threads; ++i) {
        workers.emplace_back([&, i] {
            TradeQuoteGenerator g(cfg, counters, static_cast<unsigned>(i));
            g.run(stop_requested);
        });
    }

    uint64_t last = 0;
    uint64_t last_errors = 0;

    while (!stop_requested.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(cfg.stats_ms));

        const uint64_t total =
            counters.attempted.load(std::memory_order_relaxed);
        const uint64_t errors =
            counters.publish_errors.load(std::memory_order_relaxed);

        const auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();

        const uint64_t delta = total - last;
        const uint64_t error_delta = errors - last_errors;

        std::cout << std::fixed << std::setprecision(2)
                  << "elapsed=" << elapsed
                  << " total=" << total
                  << " rate=" << (delta / (cfg.stats_ms / 1000.0))
                  << " trades="
                  << counters.trades.load(std::memory_order_relaxed)
                  << " quotes="
                  << counters.quotes.load(std::memory_order_relaxed)
                  << " errors=" << errors
                  << " intervalErrors=" << error_delta
                  << "\n";

        last = total;
        last_errors = errors;

        if (cfg.duration_seconds &&
            elapsed >= static_cast<double>(cfg.duration_seconds)) {
            stop_requested.store(true, std::memory_order_relaxed);
            break;
        }
    }

    for (auto& t : workers)
        t.join();

    const auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();

    const uint64_t total =
        counters.attempted.load(std::memory_order_relaxed);

    std::cout << "\n=== Final ===\n"
              << "duration: " << elapsed << " s\n"
              << "messages: " << total << "\n"
              << "rate: " << (elapsed > 0 ? total / elapsed : 0) << " msg/s\n"
              << "trades: "
              << counters.trades.load(std::memory_order_relaxed) << "\n"
              << "quotes: "
              << counters.quotes.load(std::memory_order_relaxed) << "\n"
              << "publish errors: "
              << counters.publish_errors.load(std::memory_order_relaxed) << "\n";

    return 0;
}
