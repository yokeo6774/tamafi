# Security Policy

## Supported Scope

This repository contains hobby firmware for ESP32 development boards. Security
support is best-effort and focused on issues that could affect users flashing or
running this firmware on their own hardware.

## Reporting A Vulnerability

Do not open a public issue for a vulnerability.

For a private repository, report the vulnerability directly to the repository
owner through the private channel used for the project. Include:

- affected commit or release,
- affected board profile,
- reproduction steps,
- expected impact,
- suggested fix if known.

## Out Of Scope

- Attacks requiring physical access to an already-flashed hobby board.
- WiFi scanning behavior that only reads public beacon metadata.
- Problems in upstream Arduino, ESP32 core, or third-party libraries unless this
  firmware uses them unsafely.

## Disclosure

Security fixes should be merged before publishing public details.
