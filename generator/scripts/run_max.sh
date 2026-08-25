#!/usr/bin/env bash
set -euo pipefail

exec ./build/ampsq-generator \
  --uri "${AMPS_URI:-tcp://localhost:9007/amps/json}" \
  --symbols 1000 \
  --rate 0 \
  --duration "${DURATION:-60}" \
  --quote-ratio 0.80 \
  --threads "${THREADS:-4}" \
  --burst 100
