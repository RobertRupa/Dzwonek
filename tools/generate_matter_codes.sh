#!/usr/bin/env bash
# Generate pairing payloads from actual Matter endpoint provisioning values.
# The resulting code is usable ONLY when the running Matter firmware uses
# the exact same passcode, discriminator, VID, PID and commissioning parameters.
set -euo pipefail
: "${ESP_MATTER_PATH:=$HOME/esp/esp-matter}"
GEN="$ESP_MATTER_PATH/connectedhomeip/connectedhomeip/src/setup_payload/python/generate_setup_payload.py"
if [[ ! -f "$GEN" ]]; then
  echo "Missing official CHIP generator: $GEN" >&2
  echo "Install/initialize ESP-Matter and connectedhomeip first." >&2
  exit 1
fi
if [[ $# -ne 4 ]]; then
  echo "Usage: $0 PASSCODE DISCRIMINATOR VENDOR_ID PRODUCT_ID" >&2
  echo "Values must come from the running Matter endpoint's provisioning." >&2
  exit 2
fi
PASSCODE="$1"; DISC="$2"; VID="$3"; PID="$4"
echo "Generating onboarding payload; this does NOT provision any device."
python3 "$GEN" --discriminator "$DISC" --passcode "$PASSCODE" --vendor-id "$VID" --product-id "$PID" --commissioning-flow 0 --discovery-cap-bitmask 2
