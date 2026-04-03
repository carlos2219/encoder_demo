# Encoder + Motor Demo (STM32F103)

This project turns a basic encoder readout into a motor-control testbed for STM32F103.

It currently includes:

- Quadrature encoder capture on TIM3 (encoder mode)
- Shaft angle wrapping and signed RPM estimation
- Periodic telemetry over USART2 (CSV)
- Automatic motor sequence (forward, stop, reverse, stop)
- Activity LED indication while motion is detected

## Project Layout

- `Core/Src/main.c`: application behavior and state machine
- `Core/Inc/main.h`: pin aliases and board-level definitions
- `Core/Src/stm32f1xx_hal_msp.c`: GPIO/timer/UART low-level setup
- `encoder_demo.ioc`: CubeMX configuration source
- `cmake/stm32cubemx/CMakeLists.txt`: generated source integration

## Firmware Behavior

Default timing values:

- Telemetry publish period: 20 ms
- Encoder activity timeout (LED off): 250 ms
- LED blink period while active: 100 ms
- Demo sequence:
	- Forward: 2000 ms
	- Stop: 1000 ms
	- Reverse: 2000 ms
	- Stop: 1000 ms
	- Repeat

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

STM32F103                     Motor Driver / Encoder / USB-UART
-----------------------------------------------------------------
PA1  (TIM2_CH2 PWM) --------> ENA / PWM input
PA8  (D7, DO_IN3)      ---------> IN3 (direction)
PB10 (D6, DO_IN4)      ---------> IN4 (direction)
GND                ---------> Driver GND

PA6  (D12, TIM3_CH1)    <--------- Encoder A, yellow wire
PA7  (D11, TIM3_CH2)    <--------- Encoder B, green wire
3V3 or 5V*         ---------> Encoder VCC
GND                ---------> Encoder GND

PA2  (USART2_TX)   ---------> USB-UART RX
PA3  (USART2_RX)   <--------- USB-UART TX
GND                ---------> USB-UART GND

Motor supply (+Vm) ---------> Driver motor supply
Motor terminals     <-------> Driver motor outputs

* Use the level required by your encoder output stage.

Important:

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