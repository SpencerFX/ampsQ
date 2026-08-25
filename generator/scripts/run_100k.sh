#!/usr/bin/env bash
set -euo pipefail

exec ./build/ampsq-generator \
  --uri "${AMPS_URI:-tcp://localhost:9007/amps/json}" \
  --symbols 100 \
  --rate 100000 \
  --duration "${DURATION:-60}" \
  --quote-ratio 0.80 \
  --threads "${THREADS:-1}" \
  --burst 10
