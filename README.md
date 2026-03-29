# Encoder Demo (STM32)

Small beginner-friendly project to read an encoder with STM32, compute shaft angle, and send values to a serial monitor.

## What This Firmware Does

- Reads encoder counts with TIM3 in encoder mode.
- Calculates wrapped angle (0 to 359.99 degrees).
- Sends serial telemetry on USART2 every 20 ms.
- Blinks onboard LED when encoder movement is detected.

Current angle scaling:

- Encoder counts per revolution (CPR): 2048

## Quick Start

1. Build

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

2. Flash the generated ELF to your board.

3. Open serial monitor with:

- Baud: 115200
- Data bits: 8
- Stop bits: 1
- Parity: None
- Flow control: None

## Serial Output Format

Each line is CSV:

```text
time_ms,count,angle_deg
```

Example:

```text
237120,402,70.66
```

## How To Check It Works

1. Rotate the shaft slowly.
2. `count` should increase or decrease.
3. `angle_deg` should move smoothly and wrap near 360 back to 0.
4. LED should blink while moving and turn off shortly after stopping.

## Most Important Files

- `Core/Src/main.c`: encoder read, angle math, UART telemetry, LED activity logic.
- `encoder_demo.ioc`: CubeMX hardware configuration.
- `CMakePresets.json`: build presets.

## If Values Look Wrong

- First verify your CPR value.
- If one full turn is not close to 360 degrees, update the CPR constant in `main.c`.
- If serial text is garbled, verify baud rate is 115200.