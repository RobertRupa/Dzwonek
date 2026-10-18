# Integration plan: StickC Plus2 + H2 RCP + existing Thread network

## Current network (from ESP Thread BR web UI)

Network Name: OpenThread-ESP
Channel: 15
PAN ID: 0x1234
Extended PAN ID: 1111111122222222
Border Router role: leader

These public network identifiers **do not** include the operational network key.
Do not copy the Thread key, PSKc or entire Active Dataset into the public repository.

## Get the complete dataset

On the ESP Thread BR web UI, inspect the active dataset via its authenticated
management interface or documented REST endpoint `/node/dataset/active`.
The complete dataset is necessary when commissioning or manually attaching
an OpenThread device to an existing network. The Thread key is a secret.

Do NOT click Form Network to add the doorbell: doing so forms/replaces a network.

## What actually runs where

- H2 `firmware-h2-rcp`: radio only, Spinel/HDLC at 460800; no BLE/Matter PIN or verifier.
- StickC Plus2 `m5stickc_plus2_rcp`: Arduino Wi-Fi doorbell, RCP transport
  diagnostics and display of an imported Matter code from an external endpoint.
  **Not yet a Thread Host** and **not yet a Matter endpoint**.
- ESP Thread BR: provides Thread connectivity and border routing; it is not
  automatically the commissioning controller for the doorbell.

An operational Matter-over-Thread doorbell needs a new ESP-IDF build for the
StickC that integrates OpenThread Host through Spinel to H2, BLE commissioning,
Matter stack, endpoint/cluster/events, and device factory data. This is not
delivered by the existing Arduino diagnostic target. Classic ESP32 memory and
flash partition capacity must be validated before attempting a full port.
ESP-Matter examples using ESP32-S3 + H2 are a useful reference, not an
out-of-the-box binary for classic ESP32.

## Real provisioning and QR

Matter manual code and QR are derived from the *actual running Matter endpoint's*
setup passcode, discriminator and payload fields. The endpoint must possess
the matching SPAKE2+ verifier and correct commissioning parameters.
QR/manual codes are not equivalent to the Thread network key or PSKc.

The WebGUI must never advertise locally generated random decimal strings as
working Matter onboarding data. Its import field is only for display of codes
already provisioned on a real endpoint.

For a development endpoint use CHIP's official `generate_setup_payload.py`
with values actually provisioned in that endpoint, and verify them with CHIP
tools. Do not check generated passcodes/verifiers into git.

## Test sequence

1. Verify the RCP probes and returns valid Spinel frames in Stick Serial logs.
2. Verify the Thread host can initialize using H2 UART. A frame alone does not
   mean that a Thread network was joined.
3. Confirm that the host sees/joins OpenThread-ESP using the actual Active Dataset.
4. Confirm Matter commissioning over BLE using correct passcode, discriminator
   and SPAKE2+ verifier.
5. Only then expose the device's actual QR/manual pairing payload on the LCD
   and WebGUI.

## Security

The current setup AP is open and the diagnostic WebGUI is unauthenticated.
The Matter commissioning code is credential material and should not be shown
to arbitrary nearby clients. Before deployment, protect AP/WebGUI and add
physical or authenticated restrictions to commissioning and factory reset.
