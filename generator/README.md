# ampsq-generator

High-throughput theoretical trade/quote generator for testing:

C++ generator -> AMPS -> q/ampsq -> kdb+ tickerplant/RDB/HDB

The generator uses the AMPS C++ client and deliberately avoids JSON DOM parsing or other heavyweight dependencies. It creates JSON directly into reusable buffers.

## Build

Set:

    export AMPS_HOME=$HOME/amps-client
    export AMPS_LIB=$AMPS_HOME/lib/linux64

Then from the repository root:

    make generator

The executable is:

    build/ampsq-generator

## Example

    ./build/ampsq-generator \
      --uri tcp://localhost:9007/amps/json \
      --symbols 100 \
      --rate 100000 \
      --duration 60 \
      --quote-ratio 0.80

This means approximately 100,000 messages/sec total, with 80% quotes and 20% trades.

## Modes

--rate N
    Target total messages/sec. 0 means maximum throughput.

--duration N
    Run for N seconds. 0 means until Ctrl-C.

--threads N
    Number of independent AMPS publisher clients.

--symbols N
    Number of generated symbols.

--quote-ratio X
    Fraction of messages that are quotes, 0.0 to 1.0.

--seed N
    Deterministic PRNG seed.

--topic-trade NAME
--topic-quote NAME
    AMPS topics.

--uri URI
    AMPS connection URI.

--client-prefix NAME
    Prefix used for AMPS client names.

--stats-ms N
    Statistics interval.

--burst N
    Optional number of messages emitted per scheduler tick before sleeping.

## Benchmarking

There are two useful tests:

1. AMPS-only throughput:

    --rate 0 --threads 1

2. End-to-end ingestion:

    generator
       -> AMPS
       -> ampsq
       -> q
       -> tickerplant

For the latter, compare generator publication counters with the q-side ingestion counters.

AMPS `publish()` is asynchronous: the API returns without waiting for an AMPS acknowledgement. Use publish flush/failed-write handling when you need a stronger delivery boundary. The generator therefore reports "publish calls submitted", not "messages durably accepted by the server".

For a production benchmark, add an AMPS publish store and/or explicit `publishFlush()` at the end of the run.
