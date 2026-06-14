# Mini NOC - System Architecture

This document provides a high-level overview of the Mini NOC application's architecture.

## 1. Core Architecture Pattern

The application follows a **Model-View-Update (MVU)** and **State Machine** architecture driven by the [Clay](https://github.com/nicbarker/clay) UI library. The core logic is completely decoupled from rendering mechanisms, allowing the exact same application code to run across three different environments:

1. **Native Desktop**: Rendered via SDL3.
2. **Terminal UI (TUI)**: Rendered via Termbox2.
3. **Web Browser**: Rendered via WebAssembly + HTML5 Canvas.

## 2. Directory Structure

- `src/app/`: The core application state machine, view routing, and UI layout code. Contains the main `ProgTP_AppState`.
- `src/domain/`: The core business logic and data structures required by the assignment.
  - `inventory/`: Equipment inventory management.
  - `connectivity/`: Ping commands, subprocess execution, and connectivity parsing.
  - `sensors/`: Importing and parsing rack sensors from text files.
  - `incidents/`: Technical incidents tracking and queuing.
  - `config/`: Configuration stack and history tracking.
- `src/common/`: Common utilities (time formatting, strings, error handling).
- `src/client/` & `src/server/`: (Optional) Remote mode execution and REST-like API structures for running operations via a separated backend server.

## 3. Data Structures

The system manages entities using custom data structures written in C11. _Note: As identified in `plan.md`, some of these are currently implemented as dynamic arrays and must be refactored to explicit explicit Pointers/Linked Lists/Stacks/Queues for maximum assignment grades._

- **Inventário (Inventory)**: Currently an array of Equipment structs.
- **Incidentes (Incidents)**: Represents the "Fila de Atendimento".
- **Configurações (Configs)**: Represents the "Pilha de Reversão".
- **Sensores (Sensors)**: Array of parsed sensor readings.

## 4. Rendering and Input

The UI is built purely in C using Clay's macro-based layout syntax (e.g., `CLAY()`, `CLAY_RECT()`, `CLAY_TEXT()`).

- All inputs are funneled into `ProgTP_AppHandleAction` and `ProgTP_AppHandleTextInput`.
- The layout is built frame-by-frame by `ProgTP_AppBuildLayout`, generating a render command array that the specific target (SDL/Termbox/Web) translates to pixels or terminal characters.

## 5. Persistence

- **Binary Files**: Domain stores (`.dat` files) are serialized using raw struct memory dumps (or structured binary formats) for speed and simplicity.
- **Text Files**: Outputs from ping, sensor imports, and logs are strictly manipulated using `fopen`/`fprintf`/`fgets` per the assignment minimum requirements.
