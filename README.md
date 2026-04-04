# Encoder + Motor Demo (STM32F103)

This project turns a basic encoder readout into a closed-loop motor-control testbed for STM32F103.

It currently includes:

- Quadrature encoder capture on TIM3 (encoder mode)
- Signed RPM estimation from encoder count delta
- PID velocity controller (anti-windup, derivative low-pass filter)
- UART command interface to arm the experiment and set RPM setpoints
- Periodic telemetry over USART2 (CSV: `rpm,setpoint,pwm_normalized`)

## Hardware

| Component | Model / Details |
|-----------|-----------------|
| MCU board | STM32F103 (Nucleo-F103RB or compatible) |
| Motor | [GM 25-370 — 12 V DC, 140 RPM, with quadrature encoder](https://uelectronics.com/producto/gm-25-370-motor-con-encoder-12v-dc-140rpm-330rpm/) |
| Motor driver | L298N or compatible H-bridge |
| USB cable (ST-link)

## Project Layout

- `Core/Src/main.c`: application loop, PID integration, UART command parser, telemetry
- `Core/Inc/main.h`: pin aliases and board-level definitions
- `Core/Src/PID.c` / `Core/Inc/PID.h`: reusable PID controller module
- `Core/Src/stm32f1xx_hal_msp.c`: GPIO/timer/UART low-level setup
- `encoder_demo.ioc`: CubeMX configuration source
- `cmake/stm32cubemx/CMakeLists.txt`: generated source integration

## Firmware Behavior

### UART Command Interface

Send commands over USART2 (newline-terminated):

| Command | Effect |
|---------|--------|
| `s` | Arm the experiment — resets PID state, clears setpoint, starts telemetry |
| `<integer>` | Set the RPM target (e.g. `120` sets 120 RPM forward) |

**Note:** The motor only spins in the forward direction. A setpoint of `0` stops the motor.

### Telemetry

Once armed, the firmware publishes a CSV line every 20 ms:

```
<measured_rpm>,<setpoint_rpm>,<pwm_normalized>
```

`pwm_normalized` is in the range `[-1.000, 1.000]` (sign reflects direction pin state).

### PID Tuning Constants

Declared near the top of `Core/Src/main.c`:

| Constant | Default | Notes |
|----------|---------|-------|
| `PID_KP` | 8.0 | Proportional gain |
| `PID_KI` | 40.0 | Integral gain |
| `PID_KD` | 0.0 | Derivative gain |
| `PID_TAU` | 0.020 s | Derivative filter time constant |
| `PID_SAMPLE_TIME_S` | 0.020 s | Controller sample period |
| `UART_PUBLISH_INTERVAL_MS` | 20 ms | Telemetry publish period |
| `ENCODER_COUNTS_PER_REV` | 2048 | Encoder PPR (quadrature counts) |

> **Important:** The gains above are tuned for the **raw PWM output range (0 – 3199)**. If you redesign the controller with a normalized output in `[0, 1]`, scale the gains accordingly — for reference, the equivalent normalized values for this motor are approximately `Kp_norm ≈ 0.004` and `Ki_norm ≈ 0.040`. //KP = MOTOR_PWM_DUTY_MAX*Kp_norm, KI = MOTOR_PWM_DUTY_MAX*Ki_norm 

Main tunable constants are declared near the top of `Core/Src/main.c`.

## Peripherals and Pin Map

Configured peripherals:

- TIM3: encoder interface
- TIM2 CH2: PWM output to motor driver
- GPIO outputs: motor direction control
- USART2: serial telemetry

Pin mapping:

- PA6: Encoder A (TIM3_CH1)
- PA7: Encoder B (TIM3_CH2)
- PA1: PWM output (TIM2_CH2)
- PA8: Motor direction IN3
- PB10: Motor direction IN4
- PA2 / PA3: USART2 TX / RX
- PA5: Onboard LED (LD2)

## Wiring (Quick Reference)

### Motor Driver

| STM32F103 Pin | Direction | Motor Driver Pin |
|---|:---:|---|
| PA1 — TIM2_CH2 (PWM) | → | ENA / PWM input |
| PA8 — D7 (DO_IN3) | → | IN3 (direction) |
| PB10 — D6 (DO_IN4) | → | IN4 (direction) |
| GND | → | Driver GND |
| Motor supply (+Vm) | → | Driver motor supply |
| Motor terminals | ↔ | Driver motor outputs |

### Encoder

| STM32F103 Pin | Direction | Encoder Wire |
|---|:---:|---|
| PA6 — D12 (TIM3_CH1) | ← | Channel A (yellow) |
| PA7 — D11 (TIM3_CH2) | ← | Channel B (green) |
| 3V3 or 5V* | → | Encoder VCC |
| GND | → | Encoder GND |

> \* Use the voltage level required by your encoder output stage.

### USB-UART Adapter

| STM32F103 Pin | Direction | USB-UART Pin |
|---|:---:|---|
| PA2 — USART2_TX | → | RX |
| PA3 — USART2_RX | ← | TX |
| GND | → | GND |

### Important Notes

- Share ground between MCU, motor driver, and USB-UART adapter.
- Do not power the motor from the STM32 board rail.
- If encoder outputs are open-collector, add pull-ups to a valid logic rail.

## Prerequisites

- Arm GNU Toolchain (`arm-none-eabi-gcc`)
- CMake 3.22+ and Ninja
- ST-LINK (or compatible) programmer/debugger
- Optional: serial terminal (`PuTTY`, `Tera Term`, `minicom`, etc.)

## Build

From the repository root:

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

Build outputs are generated under `build/Debug`.

## Flash and Run

1. Program the generated ELF to the STM32F103 target with your preferred tool (STM32CubeProgrammer, ST-LINK utility, or VS Code launch task).
2. Open USART2 in a serial terminal:
	 - Baud: 115200
	 - Data bits: 8
	 - Stop bits: 1
	 - Parity: None
	 - Flow control: None
3. Power the motor stage and verify common ground.

## Telemetry

CSV schema:

```text
time_ms,count,angle_deg,rpm
```

Example:

```text
237120,402,70.66,58.89
```

Fields:

- `time_ms`: HAL tick (ms)
- `count`: raw TIM3 counter
- `angle_deg`: wrapped angle in degrees (0.00 to 359.99)
- `rpm`: signed shaft speed in revolutions per minute

## Quick Validation Checklist

1. Verify motion sequence: forward -> stop -> reverse -> stop.
2. Verify encoder `count` changes with motion and trend flips by direction.
3. Verify `angle_deg` wraps near 359.99 -> 0.00.
4. Verify LED blinks on movement and turns off after timeout.

## Tuning Parameters

Adjust in `Core/Src/main.c`:

- `ENCODER_COUNTS_PER_REV`
- `MOTOR_PWM_DEMO_DUTY`
- `MOTOR_FORWARD_TIME_MS`
- `MOTOR_STOP_TIME_MS`
- `MOTOR_REVERSE_TIME_MS`
- `UART_PUBLISH_INTERVAL_MS`

## Troubleshooting

- No serial data:
	- Confirm USART2 wiring is crossed (MCU TX -> adapter RX, MCU RX -> adapter TX).
	- Confirm terminal settings are 115200 8N1.
- Motor does not move:
	- Confirm motor supply is present at the driver.
	- Confirm PWM pin and direction pins match your driver inputs.
- ST-LINK cannot connect:
	- Verify target power/VTref, SWDIO, SWCLK, GND, and NRST wiring.
	- Try connect-under-reset and reduce SWD frequency.