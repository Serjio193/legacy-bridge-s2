# lg_apa102

ESP32-S2 Mini firmware inspired by `legacy-bridge`:

- main firmware on OTA_0
- recovery firmware in factory partition
- signed release packages
- configurable GPIO in the UI
- configurable signed-update source
- optional PIN protection for control and recovery APIs

## License

Original project code is licensed under the MIT License. The AWA parser is
adapted from `awawa-dev/HyperSerialWLED`, and the firmware/UI include other
open-source components. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.

## Hardware

- Wemos S2 Mini / ESP32-S2
- APA102-style output is wired through configurable DATA / CLOCK / OE / POWER pins

## Default pins

- POWER MOSFET: GPIO3, `LOW = OFF`, `HIGH = ON`
- OE SN74AHCT125N: GPIO11, `LOW = OFF`, `HIGH = ON`
- APA102 DATA: GPIO7, SPI DATA
- APA102 CLOCK: GPIO9, SPI CLOCK

GPIO46 is intentionally rejected.

## First setup AP

- SSID / device name: `apa102` + full device MAC without separators.
- AP password: `APA102` + last 4 hex characters of the device MAC.

## Recovery entry

The hardware RESET/EN pin cannot be read by firmware while it is held low. For
button-only recovery, press reset twice within 5 seconds after boot. The second
boot switches to the factory recovery partition.

## Build

```powershell
pio run -e lolin_s2_mini
pio run -e lolin_s2_mini_recovery
```

## Release flow

`scripts/build_release.ps1` builds both binaries, signs them, and writes:

- `release.txt`
- `firmware.bin`
- `firmware.sig`
- `recovery.bin`
- `recovery.sig`

The canonical release directory is:

`https://serjio193.github.io/lg_apa102/latest/`

`packBaseUrl` can be changed in Settings. It must point at an HTTP(S)
directory containing `release.txt` and the signed artifacts.

GitHub Actions installs both Python and Node dependencies, builds the React UI
from source, creates `dist/latest`, and publishes the whole `dist` directory.

## Web API protection

The device starts with API protection disabled so an existing installation is
not locked out. Configure a 4-16 character PIN in Settings. After that, all
state-changing endpoints, Wi-Fi scans, logs, and recovery actions require the
`X-API-PIN` header. The browser stores the PIN only in `sessionStorage`.

## Notes

- Transport can be HTTPS with `setInsecure()` because payload integrity is checked by RSA/SHA-256 signatures.
- The public key is embedded in `include/lb_public_key.h`.
- The private key stays in `keys/release_private.pem` and is ignored by Git.
