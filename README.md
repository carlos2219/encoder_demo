# Encoder + Motor Demo (STM32F103)

This branch extends the encoder demo into a closed-loop-ready motor test project:

- Reads a quadrature encoder using TIM3 in encoder mode.
- Computes wrapped shaft angle and signed motor RPM from encoder count.
- Streams telemetry over USART2 at a fixed period.
- Runs a repeating motor demo sequence (forward, stop, reverse, stop).
- Blinks the onboard LED while encoder movement is detected.

## Firmware Behavior

Main loop timing and behavior:

- UART publish period: 20 ms
- Encoder activity timeout for LED: 250 ms
- LED blink interval while active: 100 ms
- Motor demo sequence:
	- Forward for 2000 ms
	- Stop for 1000 ms
	- Reverse for 2000 ms
	- Stop for 1000 ms
	- Repeat

Control constants are defined at the top of `Core/Src/main.c`.

## Peripherals and Pins

Configured peripherals:

- TIM3: encoder interface
- TIM2 CH2: motor PWM output
- GPIO direction pins: motor H-bridge direction control
- USART2: serial telemetry

Pin map (from CubeMX-generated MSP and headers):

- Encoder A: PA6 (TIM3_CH1)
- Encoder B: PA7 (TIM3_CH2)
- PWM output: PA1 (TIM2_CH2)
- Motor direction IN3: PA8
- Motor direction IN4: PB10
- UART TX/RX: PA2 / PA3 (USART2)
- Onboard LED (LD2): PA5

## Wiring Diagram (Quick Reference)

Use this as a practical wiring map for a typical dual-input H-bridge driver and quadrature encoder.

STM32F103                     Motor Driver / Encoder / USB-UART
-----------------------------------------------------------------
PA1  (TIM2_CH2 PWM) --------> ENA / PWM input
PA8  (DO_IN3)      ---------> IN3 (direction)
PB10 (DO_IN4)      ---------> IN4 (direction)
GND                ---------> Driver GND

PA6  (TIM3_CH1)    <--------- Encoder A
PA7  (TIM3_CH2)    <--------- Encoder B
3V3 or 5V*         ---------> Encoder VCC
GND                ---------> Encoder GND

PA2  (USART2_TX)   ---------> USB-UART RX
PA3  (USART2_RX)   <--------- USB-UART TX
GND                ---------> USB-UART GND

Motor supply (+Vm) ---------> Driver motor supply input
Motor terminals     <-------> Driver motor outputs

* Use the voltage level required by your encoder output stage.

Notes:

- Keep MCU ground, motor driver ground, and USB-UART ground common.
- Do not power the motor directly from the STM32 board 5V/3V3 rail.
- If encoder outputs are open-collector, add pull-up resistors to a valid logic rail.

## Build

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

Generated artifacts are placed under `build/Debug`.

## Flash and Run

1. Flash the generated ELF to your STM32F103 target.
2. Connect serial monitor to USART2 with:
	 - Baud: 115200
	 - Data bits: 8
	 - Stop bits: 1
	 - Parity: None
	 - Flow control: None
3. Power the motor stage and verify shared ground between MCU and driver.

## Telemetry Format

Each line is CSV:

```text
time_ms,count,angle_deg,rpm
```

Example:

```text
237120,402,70.66,58.89
```

Field meanings:

- `time_ms`: HAL tick in milliseconds
- `count`: raw TIM3 counter value
- `angle_deg`: wrapped angle in degrees (0.00 to 359.99)
- `rpm`: signed shaft speed in revolutions per minute

## Quick Validation

1. Observe motor phase sequence: forward -> stop -> reverse -> stop.
2. Verify encoder `count` changes with shaft movement and reverses sign trend with direction.
3. Confirm `angle_deg` wraps from ~359.99 to ~0.00.
4. Confirm LED blinks when motion is present and turns off after motion stops.

## Tuning

- CPR scaling: `ENCODER_COUNTS_PER_REV`
- Motor demo duty: `MOTOR_PWM_DEMO_DUTY`
- Demo phase durations: `MOTOR_FORWARD_TIME_MS`, `MOTOR_STOP_TIME_MS`, `MOTOR_REVERSE_TIME_MS`
- Telemetry rate: `UART_PUBLISH_INTERVAL_MS`

## Key Files

- `Core/Src/main.c`: application logic (encoder read, angle math, UART publish, motor demo FSM)
- `Core/Inc/main.h`: board pin definitions used by the app
- `Core/Src/stm32f1xx_hal_msp.c`: peripheral GPIO/timer/UART low-level mapping
- `encoder_demo.ioc`: CubeMX source configuration