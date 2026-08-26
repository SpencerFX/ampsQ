# ampsq — AMPS client extension for q/kdb+

`ampsq` is a Linux x86-64 q extension that embeds the 60East AMPS C++ client behind a small C ABI and exposes it to q.

The design deliberately keeps AMPS callbacks off the q thread:

AMPS receive thread
    -> C++ queue
    -> Linux eventfd
    -> q `sd1()` main event loop
    -> q callback

This is important because AMPS invokes subscription handlers on its client receive thread, while KX documents `sd1()` as the mechanism for deferring work onto q's main event loop. See:
- https://code.kx.com/q/interfaces/c-client-for-q/
- https://devnull.crankuptheamps.com/documentation/api/cpp/5.3.5.5/classAMPS_1_1Client.html

## Requirements

- Linux x86-64
- kdb+ 64-bit Linux (`l64`)
- GCC/G++ with C++17 support
- 60East AMPS C/C++ client SDK, version 5.3.x or compatible
- An AMPS server

The AMPS SDK is not included in this repository. Set `AMPS_HOME` to the root of your AMPS client distribution.

The Makefile expects:

    $AMPS_HOME/include/amps/ampsplusplus.hpp
    $AMPS_HOME/lib/.../libamps.so

The exact library directory/name differs between AMPS distributions. Set `AMPS_LIB` if required.

## Build

Example:

    export QHOME=$HOME/q
    export AMPS_HOME=$HOME/amps-client
    export AMPS_LIB=$AMPS_HOME/lib/linux64

Then:

    make

The output is:

    build/libampsq.so

Run:

    export LD_LIBRARY_PATH=$AMPS_LIB:$LD_LIBRARY_PATH
    q examples/subscribe.q

or:

    q examples/publish.q

## q API

Load:

    \l q/amps.q

Connect:

    h:.amps.connect["tcp://localhost:9007/amps/json";"q-amps"]

Publish:

    .amps.publish[h;`trades;"{\"sym\":\"AAPL\",\"px\":231.42,\"qty\":100}"]

Subscribe:

    .amps.on[h;{[m] 0N!m};"trades";""]

Unsubscribe:

    .amps.unsubscribe[h]

Close:

    .amps.close h

The callback receives a dictionary:

    topic     | `trades
    command   | `publish
    data      | "{\"sym\":\"AAPL\",...}"
    timestamp | "..."
    bookmark  | "..."
    sequence  | "..."

`data` is a q char vector, not a symbol, so large JSON payloads do not get interned.

## Testing

The C/C++ backend (`src/`, `generator/`) needs a Linux host, a built
`libampsq.so`, and a real AMPS server, so it isn't covered by an automated
suite here. What *is* covered automatically, using the
[q-test](../q-test) framework (a sibling repo, pure q, no shell
dependency):

- `tests/generator_schema_test.q` - `generator/config/schema.q` loads and
  produces `trade`/`quote` with the documented columns and kdb+ types.
- `tests/amps_api_test.q` - the pure-q wrapper layer in `q/amps.q`. Since
  loading `q/amps.q` directly requires `build/libampsq.so` (which needs
  the AMPS SDK), this file regenerates just the wrapper-defining prefix
  of the real source into a scratch file, mocks the native `.amps._*`
  entry points, and checks the wrappers forward their arguments
  correctly. Its "regression guards" suite also pins down two real
  defects this test suite caught and that are now fixed: the wrapper
  names used to be dotted (`.connect:{...}`, etc) under `\d .amps`,
  which in q is an absolute reference to root and ignores the current
  `\d` context - so every wrapper actually landed at root instead of
  under `.amps`, meaning `.amps.connect[...]` and friends, exactly as
  written in every example below and in `examples/*.q`, would have
  raised "undefined variable" even against a correctly built
  `libampsq.so`. And the `AMPSQ_SO` override guard used to check
  `` `AMPSQ_SO in key `.Q.env `` - `.Q.env` isn't a real q namespace, so
  `key` on it was always empty and the guard always overwrote
  `AMPSQ_SO` with the hardcoded default, even when a caller pre-set it;
  it now checks `` key `. `` instead.

Run everything from this directory:

    q ../q-test/bin/qtest.q tests

Expect `11 total 11 passed 0 failed 0 errored`.

## Scope

This first implementation intentionally focuses on:

- connect
- logon
- publish
- asynchronous subscribe
- server-side filter
- unsubscribe
- q-main-thread callback dispatch
- SOW query through a callback
- clean shutdown

It does not yet implement HAClient failover, durable publish stores, bookmark stores, queue acknowledgements, TLS configuration helpers, or typed JSON -> q table conversion.

Those are natural next additions.


## Why there is a C++ backend

The requested q-facing entry point is `src/ampsq.c`, but the implementation uses the 60East C++ client because its high-level API provides the subscription and SOW operations directly, while the q extension boundary remains a C ABI. The C++ client itself is built on the AMPS C client.

The AMPS receive callback never calls q. It copies the message into a C++ queue and signals `eventfd`. `ampsq.c` registers that eventfd with q using `sd1()`. q then drains the queue on its own main thread and invokes the q callback.

This follows the threading model documented by both projects: AMPS subscription handlers run on the AMPS client receive thread, while KX documents `sd1()`/`eventfd` as a way to defer work to q's main event loop.


## Trade/quote benchmark generator

The `generator/` directory contains a C++ AMPS publisher for testing the complete ingestion path.

Example:

    make generator

    ./build/ampsq-generator \
      --uri tcp://localhost:9007/amps/json \
      --symbols 100 \
      --rate 100000 \
      --duration 60 \
      --quote-ratio 0.80

It generates theoretical `trade` and `quote` messages with timestamps, symbols, prices, sizes, sides, venues and sequence numbers.

The generator supports:
- rate-controlled publishing
- maximum-throughput publishing (`--rate 0`)
- multiple publisher threads
- configurable symbol universe
- deterministic random seed
- configurable trade/quote mix
- burst publishing
- periodic throughput statistics
- final throughput/error statistics

The q schemas are in `generator/config/schema.q`.

For a true end-to-end benchmark:

    C++ generator
        -> AMPS
        -> ampsq
        -> q callback
        -> typed q table
        -> tickerplant/RDB

Because AMPS `publish()` is asynchronous, generator counts represent publish calls submitted by the client. They are not, by themselves, proof of durable server acceptance. The AMPS client documentation recommends a failed-write handler and provides `publishFlush()` when an application needs a delivery boundary.
