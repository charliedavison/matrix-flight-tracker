# Heathrow Approach Tracker

A C++ app for Raspberry Pi that shows the flight passing nearest your location on approach to Heathrow, on a 64×64 RGB LED matrix with an Adafruit Matrix Bonnet.

```
┌──────────────────────────────┐
│  BA258                       │  flight number
│  BOS Boston                  │  origin
│  1.2km 2800ft                │  distance + altitude
│  320km/h ETA15:08            │  speed + ETA
│  British Airways             │  airline
└──────────────────────────────┘
```

The display tracks the nearest active LHR arrival with live position data within range of your coordinates, refreshing every poll interval.

## Hardware

- Raspberry Pi Zero (W/WH recommended)
- 64×64 RGB LED matrix panel (HUB75)
- [Adafruit RGB Matrix Bonnet](https://www.adafruit.com/product/3211)

### Bonnet setup for 64×64

1. **E-line jumper** — solder the middle pad to pin **8** on the bottom of the bonnet (required for 64×64 panels).
2. **Quality jumper** (optional, reduces flicker) — bridge GPIO 4 and GPIO 18.
3. Connect the panel power supply (5V, adequate amperage for your panel).
4. Plug the HUB75 ribbon cable into the bonnet.

## Software setup (on the Pi)

```bash
git clone <your-repo-url> pi-rgb
cd pi-rgb
chmod +x scripts/setup.sh
./scripts/setup.sh
```

This installs build dependencies, clones [rpi-rgb-led-matrix](https://github.com/hzeller/rpi-rgb-led-matrix), downloads the JSON library, and builds the app. The matrix library is built with its own Makefile (not CMake) because upstream's CMakeLists breaks when used as a subdirectory.

## Configuration

Copy the example env file and add your API key:

```bash
cp .env.example .env
nano .env
```

| Variable | Default | Description |
|---|---|---|
| `AVIATIONSTACK_API_KEY` | — | Free key from [aviationstack.com](https://aviationstack.com/signup) |
| `OBSERVER_LAT` | `51.4465501` | Your latitude |
| `OBSERVER_LON` | `-0.2407212` | Your longitude |
| `MAX_DISTANCE_KM` | `25` | Search radius around your location |
| `MAX_ALTITUDE_FT` | `12000` | Ignore aircraft above this (cruise) |
| `MIN_ALTITUDE_FT` | `500` | Ignore aircraft below this (on ground) |
| `POLL_INTERVAL_SEC` | `60` | Seconds between API refreshes |
| `USE_MOCK` | `1` if no key | Set to `1` for demo data, `0` for live API |

**API quota:** the free Aviationstack plan allows 100 requests/month. At the default 60-second poll interval that's ~43,200 requests/month — you'll need a paid plan for live tracking, or increase `POLL_INTERVAL_SEC` significantly. For development, use `USE_MOCK=1`.

## Running

The matrix library needs root access and bonnet-specific flags:

```bash
sudo ./build/heathrow-flights \
  --led-rows=64 \
  --led-cols=64 \
  --led-gpio-mapping=adafruit-hat
```

If the display looks wrong (blank rows, garbled image), try adding:

```bash
  --led-multiplexing=0 \
  --led-slowdown-gpio=4
```

On Pi Zero, `--led-slowdown-gpio=0` or `1` may also help — experiment with values 0–4.

Press `Ctrl+C` to stop.

## How it works

1. Fetches all **active** flights arriving at LHR with live position data from Aviationstack.
2. Filters to aircraft within your configured distance and altitude band (approach phase).
3. Picks the one **nearest** your `OBSERVER_LAT` / `OBSERVER_LON`.
4. Displays it until the next poll.

If no aircraft match (e.g. nothing overhead right now), the display shows "NO FLIGHT".

## Project structure

```
src/
  main.cpp        — main loop, config, signal handling
  flight_api.cpp  — Aviationstack client + nearest-flight search
  display.cpp     — matrix rendering
  geo.h           — haversine distance calculation
  flight.h        — flight data model
scripts/
  setup.sh        — one-shot Pi setup and build
```

## Troubleshooting

- **Blank display** — check E-line solder jumper and power supply.
- **Garbled pixels** — try `--led-multiplexing=0` and `--led-gpio-mapping=adafruit-hat`.
- **"NO FLIGHT"** — normal when nothing is on approach near you; try widening `MAX_DISTANCE_KM` or `MAX_ALTITUDE_FT`.
- **API errors** — verify your key in `.env`; check stderr for details.
- **Font not found** — run from the project root so `third_party/rpi-rgb-led-matrix/fonts/4x6.bdf` is found.

## License

This project uses [rpi-rgb-led-matrix](https://github.com/hzeller/rpi-rgb-led-matrix) (GPL-2.0). If you distribute a product using this code, GPL obligations apply to the combined work.
