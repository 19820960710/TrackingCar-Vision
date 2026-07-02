---
name: mspm0-keil
description: Keil uVision agent rules for TI MSPM0 development with SysConfig and DriverLib. Use when an agent needs to inspect or modify Keil-based MSPM0 projects, edit .syscfg configuration, build via UV4.exe command line, flash via CMSIS-DAP, edit Component modules, or work on MSPM0 embedded firmware.
---

# MSPM0 Keil Agent Skill

Use this skill for TI MSPM0 firmware projects that use **Keil uVision**, SysConfig, and DriverLib. Supports bare-metal and FreeRTOS projects.

## Default Workflow

1. Locate the project `.syscfg`, editable source files (`main.c`, `Component/`), generated `ti_msp_dl_config.h`, and the Keil `.uvprojx` entrypoint.
2. Read `.syscfg` metadata: device, package, SDK product, modules, instances, pins, clocks, interrupts.
3. Inspect `ti_msp_dl_config.h` for macro names, IRQ handlers, instance names, and the exact init function (`SYSCFG_DL_init()`).
4. Modify the smallest relevant `.syscfg` and application-code surface.
5. Build: `UV4.exe -b <project>.uvprojx -t <target> -j0`
6. Flash: `UV4.exe -f <project>.uvprojx -t <target>`

## Project Architecture

- **`main.c` is a glue layer only.** Calls `SYSCFG_DL_init()`, initializes components, starts main loop or FreeRTOS scheduler. No driver code or ISRs here.
- **Modules under `Component/<name>/`.** Each is a self-contained `.c`/`.h` pair using DriverLib + SysConfig macros.
- **ISRs live in the owning Component.** e.g. `UART0_IRQHandler` in `Component/UART/uart0.c`. FreeRTOS ISRs use `xQueueSendFromISR()` + `portYIELD_FROM_ISR()`.
- **When adding a module**, also add a new Group in `.uvprojx` XML (`<GroupName>Component/XXX</GroupName>`) and `.uvoptx`. Include path `../Component` already exists.

## Keil Build & Flash

### Build

```bash
D:/keil/UV4/UV4.exe -b <project>.uvprojx -t <target> -j0
```

- `-b`: build target
- `-t`: target name (from uvprojx TargetName)
- `-j0`: suppress GUI output to file

Check `Objects/<target>.build_log.htm` for errors/success.

### Flash

```bash
D:/keil/UV4/UV4.exe -f <project>.uvprojx -t <target>
```

### Required Project Settings

In Keil GUI or `.uvprojx`:
- **Target → MicroLIB**: required when using `vsnprintf`/`printf` (avoids semihosting → flash conflict)
- **C/C++ → One ELF Section per Function**: improves flash compatibility
- **Debug → CMSIS-DAP Debugger**: selected in uvoptx via `<pMon>BIN\CMSIS_AGDI.dll</pMon>` + `nTsel=3` + `TargetDriverDllRegistry`

### uvoptx MUST include

```xml
<TargetDriverDllRegistry>
  <SetRegEntry>
    <Key>UL2CM3</Key>
    <Name>UL2CM3(-S0 -C0 -P0 -FD20200000 -FC8000 -FN1 
      -FF0MSPM0G1X0X_G3X0X_MAIN_128KB -FS00 -FL020000 
      -FP0($$Device:MSPM0G3507$02_Flash_Programming\FlashARM\MSPM0G1X0X_G3X0X_MAIN_128KB.FLM))</Name>
  </SetRegEntry>
</TargetDriverDllRegistry>
```

Without this, flash erase works but programming fails with "Cortex-M0+".

### Flash Failure Recovery

If `Flash Download failed - "Cortex-M0+"`:
1. Check MicroLIB is enabled
2. Power-cycle the board (unplug USB, wait 5s, replug)
3. If chip was mass-erased and locked: hold RST button while plugging USB, then flash

## Core Rules

- `.syscfg` is the source of truth for pinmux, peripherals, clocks, interrupts.
- **All external/peripheral pin initialization must be represented in SysConfig**, including GPIO used for software/bit-banged protocols such as software I2C/SPI, display reset pins, chip selects, interrupts, and manually toggled SCL/SDA lines.
- Application drivers must use generated SysConfig macros from `ti_msp_dl_config.h` for ports, pins, IOMUX, IRQn, and instance names; never hard-code `DL_GPIO_PIN_x`, `IOMUX_PINCMx`, or peripheral instances in component code when the resource belongs to board pinmux.
- Use SysConfig + DriverLib for GPIO, UART, PWM, Timer, ADC, I2C, SPI, DMA, and clock setup.
- Never edit generated files: `ti_msp_dl_config.c/h`, `Objects/`, `Listings/`, object files, maps. Update `.syscfg` and regenerate when possible; if generated files must be patched temporarily, state that it is provisional and must be regenerated from SysConfig.
- Preserve `.syscfg` metadata (`@cliArgs`, `@v2CliArgs`, `@versions`, `--device`, `--package`).
- Read `ti_msp_dl_config.h` for all generated macro names; do not guess.
- Do not invent SysConfig fields, device metadata, or tool versions.
- Preserve existing user code, comments, project layout, and `.syscfg` settings.
- Do not change device, package, SDK, board without user confirmation.
- Report SysConfig warnings separately from build/flash success.

## SysConfig Editing Rules

- Prefer editing `.syscfg` using patterns copied from the project's existing file or SDK examples for the same module.
- For GPIO groups with multiple pins, create the pins before assigning index 1+:
  ```js
  GPIOx.associatedPins.create(2);
  GPIOx.associatedPins[0].$name = "PIN_NAME_0";
  GPIOx.associatedPins[0].pin.$assign = "PA28";
  GPIOx.associatedPins[1].$name = "PIN_NAME_1";
  GPIOx.associatedPins[1].pin.$assign = "PA31";
  ```
- Use `.pin.$assign = "PAxx"/"PBxx"` for pin assignment. Avoid legacy/guessed fields like `.assignedPin = "28"` unless the current project already generated them and SysConfig accepts them.
- After editing `.syscfg`, verify it opens or regenerates before relying on generated macro names.
- To regenerate without Keil GUI, prefer calling SysConfig's node CLI directly from the project root:
  ```bash
  /d/tool/CCS/sysconfig_desktop/nodejs/node.exe /d/tool/CCS/sysconfig_desktop/dist/cli.js -o . -s /d/tool/CCS/mspm0_sdk_2_10_00_04/.metadata/product.json --compiler keil main.syscfg
  ```
- Do not manually patch `ti_msp_dl_config.c/h` to change SysConfig-controlled values such as UART baud rate. If they are out of sync, fix `.syscfg` and regenerate.

## Board-Specific: Tianmengxing MSPM0G3507

- Avoid A21/PA21, A23/PA23, A02/PA02, A18/PA18, A10/PA10, A11/PA11 unless requested.
- PB22 is onboard LED (low-active: `clearPins` = ON).
- PA5/PA6 are 40MHz HFXT pins.
- PA10/PA11 are UART0 default (TX/RX).
- PA19/PA20 are SWD debug pins.

## FreeRTOS Projects

When `FreeRTOSConfig.h` and `task.h` are present:

- Config must match: `configCPU_CLOCK_HZ` = SysConfig CPUCLK (80000000 for 80MHz)
- Heap: `configTOTAL_HEAP_SIZE` ≥ 5KB
- UART pattern: ISR → `xQueueSendFromISR()` → task `xQueueReceive()` → process
- TX mutex: `xSemaphoreCreateMutex()` to serialize multi-task printf/UART writes
- Libc redirect: override `write()` to route `printf` to UART via mutex-protected blocking TX
- Task stack: 128 words minimum, 256 for UART tasks

## Ambiguous Requests

- For low-risk defaults, use skill examples or SDK examples; tell the user what was applied.
- For important parameters (pin, peripheral instance, baud rate, timer period, PWM frequency, etc.), ask before choosing.
- Example: "add a timer interrupt" → ask which timer and period.

## External Modules

When driving sensors, motors, displays, etc.:

- Ask for datasheet, schematic, pin map, voltage levels, protocol when unavailable.
- Verify wiring before blaming code: power, ground, pull-ups, level shifting, TX/RX crossover, I2C address, SPI mode.
- Separate "firmware looks correct" from "hardware proved correct".

## Tools

Skill directory scripts (run with project Python):

- `python scripts/check_syscfg.py <project-dir>`: analyze .syscfg, generated files, pin config
- `python scripts/detect_probe.py`: list connected debug probes
- `python scripts/list_examples.py`: list packaged examples
- `python scripts/index_syscfg_examples.py <sdk-root> --board LP_MSPM0G3507 --module UART`: search SDK examples
- `python scripts/serial_console.py --list`: list serial ports
- `python scripts/serial_console.py -p COM6 -b 115200 --timestamp --duration 10`: monitor UART output
