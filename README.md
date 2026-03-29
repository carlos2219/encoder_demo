# Encoder Demo

This project is an STM32 encoder test built from STM32CubeMX-generated sources and compiled with CMake.

The current firmware reads a quadrature encoder using `TIM3` in encoder mode and blinks the onboard LED when encoder movement is detected. The LED keeps blinking briefly after the last detected count change and then turns off when the encoder stops.

## Current Progress

What is already working:

- The project builds successfully with the existing CMake setup.
- `TIM3` is configured and started in encoder mode.
- The main loop polls the encoder counter using `__HAL_TIM_GET_COUNTER(&htim3)`.
- `USART2` is initialized and available for future debug output.
- The onboard LED (`LD2`) blinks when encoder counts change.
- A `.gitignore` file is in place to avoid committing build outputs and local IDE files.

Current LED behavior:

- Blink interval: `100 ms`
- Encoder activity timeout: `250 ms`
- If the encoder moves, the LED starts blinking.
- If the encoder stops, the LED is forced off after the timeout expires.

## Project Structure

- `Core/`: application code and startup logic
- `Drivers/`: STM32 HAL and CMSIS drivers
- `cmake/`: toolchain and CubeMX CMake integration
- `encoder_demo.ioc`: STM32CubeMX configuration file
- `CMakeLists.txt`: root build configuration
- `CMakePresets.json`: Debug and Release presets

## Build

This project uses CMake presets and Ninja.

Available presets:

- `Debug`
- `Release`

Typical configure/build flow:

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

The generated build output is placed under `build/Debug/`.

## Firmware Behavior

The current logic in `main.c` is intentionally simple:

1. Start `TIM3` in encoder mode.
2. Read the counter continuously in the main loop.
3. Compare the current count with the previous count.
4. If the count changed, record the current tick as recent encoder activity.
5. Blink `LD2` while encoder activity is still considered recent.

This is useful as a first hardware validation step because it confirms all of the following with minimal code:

- timer setup is valid
- encoder channels are being read
- the main loop is alive
- the LED GPIO is working

## Notes

- The current implementation is polling-based, not interrupt-based.
- The project already includes `USART2`, so serial angle or count reporting can be added next if needed.
- If the board LED does not match `LD2`, update the LED pin mapping in the CubeMX configuration or generated headers.

## Good Next Steps

1. Print encoder count and computed angle over `USART2` for easier debugging.
2. Handle counter rollover if continuous multi-turn tracking is needed.
3. Move from simple polling to a cleaner event or periodic sampling approach.
4. Document wiring for the encoder channels and target board.