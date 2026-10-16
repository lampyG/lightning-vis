# lightning-vis

Simple terminal audio visualizer in C (ALSA + ncurses). Inspired by cava.

Features
- Real-time audio capture from ALSA ("default" device)
- FFT-based frequency analysis
- Terminal visualization (vertical and horizontal)
- Controls:
  - `c` — cycle color (simple schemes)
  - `f` — toggle orientation (horizontal/vertical)
  - `p` — pause/unpause
  - `q` — quit

Requirements
- Linux (ALSA)
- libasound2-dev
- libncurses-dev
- build tools (gcc, make)

Install dependencies (Debian/Ubuntu)
```bash
sudo apt install build-essential libasound2-dev libncurses-dev