# Heathrow Flight Tracker

A C++ app for Raspberry Pi that shows incoming flight details on a 64×64 RGB LED matrix with an Adafruit Matrix Bonnet.

```
┌──────────────────────────────┐
│  BA178                       │  flight number
│  JFK New York                │  origin
│  ETA 14:45 +15m              │  arrival (colour = status)
│  T5 G A12 active             │  terminal / gate
│  British Airways             │  airline
└──────────────────────────────┘
```

Each flight is shown for a few seconds, then the display cycles to the next. Data refreshes on a configurable interval.

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

This installs build dependencies, clones [rpi-rgb-led-matrix](https://github.com/hzeller/rpi-rgb-led-matrix), downloads the JSON library, and builds the app.

## Configuration

Copy the example env file and add your API key:

```bash
cp .env.example .env
nano .env
```

| Variable | Default | Description |
|---|---|---|
| `AVIATIONSTACK_API_KEY` | — | Free key from [aviationstack.com](https://aviationstack.com/signup) |
| `POLL_INTERVAL_SEC` | `900` | Seconds between API refreshes |
| `DISPLAY_SECONDS` | `8` | Seconds to show each flight |
| `USE_MOCK` | `1` if no key | Set to `1` for demo data, `0` for live API |

**API quota:** the free Aviationstack plan allows 100 requests/month. At the default 15-minute poll interval that's ~2,880 requests/month — you'll need a paid plan for frequent live updates, or increase `POLL_INTERVAL_SEC` to ~26,000 (about one request every 7 hours) to stay within the free tier. For development, use `USE_MOCK=1`.

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

## Project structure

```
src/
  main.cpp        — main loop, config, signal handling
  flight_api.cpp  — Aviationstack HTTP client
  display.cpp     — matrix rendering
  flight.h        — flight data model
scripts/
  setup.sh        — one-shot Pi setup and build
```

## Status colours

| Status | Colour |
|---|---|
| Landed | Green |
| Active (in flight) | Blue |
| Delayed | Orange |
| Cancelled | Red |
| Scheduled | Grey |

## Troubleshooting

- **Blank display** — check E-line solder jumper and power supply.
- **Garbled pixels** — try `--led-multiplexing=0` and `--led-gpio-mapping=adafruit-hat`.
- **API errors** — verify your key in `.env`; check stderr for details.
- **Font not found** — run from the project root so `third_party/rpi-rgb-led-matrix/fonts/4x6.bdf` is found.

## License

This project uses [rpi-rgb-led-matrix](https://github.com/hzeller/rpi-rgb-led-matrix) (GPL-2.0). If you distribute a product using this code, GPL obligations apply to the combined work.
