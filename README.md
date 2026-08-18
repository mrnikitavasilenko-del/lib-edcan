# lib-edcan

Reusable CAN transport library ("EDCAN") used across the SI02 battery-stand STM32 firmware —
[`akb-maincontroller-si02`](https://github.com/mrnikitavasilenko-del/akb-maincontroller-si02),
[`akb-si02-ukpb`](https://github.com/mrnikitavasilenko-del/akb-si02-ukpb),
[`akb-voltage-cube`](https://github.com/mrnikitavasilenko-del/akb-voltage-cube).

## What it provides

- Frame encoding/decoding and buffering (`edcan.c`, `edcan_buffer.c`)
- A request/response and periodic-broadcast handler loop (`edcan_handler.c`,
  `edcan_handler_user.c.template` — copy and fill in per-project message handling)
- Bus-error detection and CAN peripheral recovery
- Basic logging hooks (`edcan_log.c`)

## Using it in a project

1. Vendor this repo (or symlink it) alongside the STM32CubeIDE project
2. Copy `edcan_config.h.template` → `edcan_config.h` and `edcan_handler_user.c.template` →
   `edcan_handler_user.c`, then fill in the project-specific message table
3. Call `EDCAN_Init()` once and `EDCAN_Loop()` from the main superloop

MCU-agnostic C, built against the ST HAL CAN driver — no RTOS dependency.
