# Mini NOC - User Manual

Mini NOC is a C application for the Programming I practical assignment: a small network operations center for a PME. It manages network equipment, runs connectivity checks, imports rack sensor readings, tracks technical incidents, keeps a configuration change history, and previews generated data files.

The same application state and UI are rendered through [Clay](https://github.com/nicbarker/clay) for three targets:

- `progtp-native`: desktop app using SDL3.
- `progtp-tui`: terminal UI using termbox2.
- `index.html` + `index.wasm`: browser app served by `progtp-server`.

The server target also exposes an optional HTTP API used by the browser and by remote native/TUI mode.

## 1. Build

Required tools:

- Meson and Ninja
- C compiler with C11 support
- Git, for Meson wraps/subprojects
- CMake, for CMake-based dependencies
- Emscripten `emcc`, only for the web target

### Full build

```sh
meson setup build -Dnative=enabled -Dtui=enabled -Dweb=enabled -Dserver=enabled
meson compile -C build
```

### Web + server only

```sh
meson setup build-web -Dnative=disabled -Dtui=disabled -Dweb=enabled -Dserver=enabled
meson compile -C build-web
```

### Release-size build

```sh
meson setup build-release -Drelease_size=true -Dnative=enabled -Dtui=enabled -Dweb=enabled -Dserver=enabled
meson compile -C build-release
```

Release mode enables size-oriented optimization (`-Os`/`-Oz`), LTO, section garbage collection, static subproject builds, and optional `wasm-opt -Oz` when available.

## 2. Run

### Native desktop

```sh
./build/progtp-native
```

### Terminal UI

```sh
./build/progtp-tui
```

Quit the TUI with `Esc`, `Ctrl-C`, or `q` when no text field/modal is active.

### Web app

```sh
./build/progtp-server --port 8000 --public build
```

Then open:

```text
http://localhost:8000/index.html
```

### Optional remote native/TUI mode

Start the server first:

```sh
./build/progtp-server --port 8000 --public build
```

Then run a client against it:

```sh
./build/progtp-native --remote http://localhost:8000
./build/progtp-tui --remote http://localhost:8000
```

Remote mode is an extra feature. Some operations are still more complete in local mode; see `PLAN.md` before relying on it for the assignment demo.

## 3. Persistent and generated files

The app uses binary files for main persistent data and text files for imports, logs, command output, and reports.

### Binary files

| File                    | Purpose                             |
| ----------------------- | ----------------------------------- |
| `equipamentos.dat`      | Equipment inventory.                |
| `incidentes.dat`        | Technical incidents.                |
| `configuracoes.dat`     | Configuration history / undo stack. |
| `leituras_sensores.dat` | Imported sensor readings.           |

### Text files

| File                                | Purpose                                                                        |
| ----------------------------------- | ------------------------------------------------------------------------------ |
| `sensores_rack.txt`                 | Sensor input file. Expected format: `codigo_sensor;tipo;valor;unidade;estado`. |
| `resultado_ping.txt`                | Raw output from required ping commands.                                        |
| `resultado_comando.txt`             | Raw output from optional custom commands.                                      |
| `log_monitorizacao.txt`             | Connectivity test log.                                                         |
| `log_sensores.txt`                  | Sensor import log.                                                             |
| `relatorio_estado_rede_mes_ano.txt` | Required network-state report; planned but not fully implemented yet.          |

If `equipamentos.dat` does not exist, the app seeds a default inventory with at least five devices.

## 4. General UI controls

The UI has a left sidebar with modules 1-6 and a main workspace. Native and web support mouse/touch interaction; the TUI also supports keyboard and mouse.

| Key         | Action                                                               |
| ----------- | -------------------------------------------------------------------- |
| `1`-`6`     | Open module 1-6.                                                     |
| `N` / `P`   | Select next / previous row in the active module.                     |
| `A`         | Add equipment in module 1. In module 4, use the `New` button or `Q`. |
| `U`         | Update selected equipment.                                           |
| `R`         | Remove selected equipment.                                           |
| `C`         | Search by code; in module 3 it searches sensor code.                 |
| `I`         | Search equipment by IP.                                              |
| `M`         | Search equipment by MAC.                                             |
| `W`         | Save inventory.                                                      |
| `L`         | Reload inventory.                                                    |
| `Enter`     | Submit active input or modal.                                        |
| `Backspace` | Delete text in active input.                                         |
| `Tab`       | Move to next form field.                                             |
| `Shift+Tab` | Move to previous form field on web/native where supported.           |
| `Esc`       | Cancel active modal/input, or quit TUI if nothing is active.         |

Some keys are context-sensitive. For example, `S` starts an incident in module 4, while equipment state is changed through the update form.

## 5. Module guide

### Module 1 - Equipment Inventory

Use this module to create, update, remove, filter, search, save, and load network equipment.

Stored fields:

- Internal code, assigned automatically
- Name
- Type
- Brand
- Model
- IP address
- MAC address
- Physical location
- Operational state: `Operational`, `Failed`, `Maintenance`, or `Disabled`
- Last check date
- Pending-incident flag

Main actions:

1. Click `Add` or press `A` to open the equipment form.
2. Fill fields and press `Save`/`Enter`.
3. Select rows with `Next`/`Prev`, mouse, or `N`/`P`.
4. Use `Update`/`U` to edit the selected equipment.
5. Use `Remove`/`R` to remove the selected equipment. Removal is blocked if the equipment has the pending-incident flag enabled.
6. Use state and type filters to list subsets of the inventory.
7. Use search controls or `C`/`I`/`M` to search by code, IP, or MAC.
8. Use `Save`/`W` and `Load`/`L` in local mode to persist or reload `equipamentos.dat`.

### Module 2 - Connectivity Tests

Use this module to run ping checks and optional custom network commands.

Main actions:

1. Select a target equipment row.
2. Click `Ping Selected` to ping the selected equipment.
3. Click `Ping All` to ping all equipment.
4. Raw ping output is written to `resultado_ping.txt`.
5. The app reads the raw output and decides whether the equipment responded.
6. Every connectivity run is appended to `log_monitorizacao.txt`.
7. If a ping fails, the app marks the equipment as `Failed`, updates its last-check date, flags pending incidents, and creates a technical incident in `incidentes.dat`.

Optional custom command:

- Type a command in the custom command field.
- Use `{ip}` as a placeholder for the selected equipment IP address, for example `nslookup {ip}`.
- Click `Run Custom`.
- Output is written to `resultado_comando.txt` and logged.

### Module 3 - Sensor Monitoring

Use this module to import rack sensor readings from text.

Input format in `sensores_rack.txt`:

```text
codigo_sensor;tipo;valor;unidade;estado
TEMP_RACK;Temperatura da rack;38.5;C;CRITICO
HUM_RACK;Humidade da rack;72;%;AVISO
UPS_BAT;Bateria da UPS;18;%;CRITICO
UPS_ENERGIA;Estado da energia;0;-;FALHA_REDE
```

Main actions:

1. Keep `sensores_rack.txt` in the working directory, or choose another path with `File`.
2. Click `Import` or press `G`.
3. Imported readings are stored in memory and saved to `leituras_sensores.dat`.
4. Each import is logged to `log_sensores.txt`.
5. Readings with state `AVISO`, `CRITICO`, or `FALHA_REDE` are highlighted as anomalies.
6. An anomalous reading automatically creates a technical incident.
7. Use `All` / `Anomalies` to filter readings.
8. Search by sensor code with the sensor search field or `C` while module 3 is active.

### Module 4 - Technical Incidents

Use this module to view and manage incidents generated by connectivity failures, sensor anomalies, or manual entry.

Incident fields:

- Sequential incident number
- Equipment code or sensor source
- Type
- Description
- Priority (`Low`, `Medium`, `High`)
- Created timestamp
- Completed timestamp
- Technician
- State: `Pending`, `In Progress`, or `Completed`

Main actions:

1. Click `New` or press `Q` to create a manual incident.
2. Click `Edit` or press `E` to edit the selected incident.
3. Click `Delete` or press `D` to remove the selected incident.
4. Click `Start` or press `S` to mark the selected incident as in progress.
5. Click `Complete` to complete the selected incident and set the completion timestamp.
6. Click `Auto-import` or press `T` to create incidents from failures logged in `log_monitorizacao.txt`.
7. Use filters to show all, pending, in-progress, or completed incidents.

Important current limitation: `Start` processes the selected incident, not necessarily the oldest pending incident. A true FIFO queue is planned in `PLAN.md`.

### Module 5 - Configuration Stack

Use this module to inspect configuration history and undo/redo recent equipment changes.

Current behavior:

- Equipment add/update/remove operations automatically record configuration history entries.
- The history is capped at the latest 25 entries.
- `Undo` reverts the latest applied entry.
- `Redo` reapplies the next undone entry.
- `Import` loads entries from a `configuracoes.dat` file.
- `Delete` removes the selected configuration entry.
- Entries are saved in `configuracoes.dat` when recorded/imported/deleted.

Important current limitation: the assignment asks for explicit configuration type, previous value, new value, and technician responsible. The current implementation stores before/after equipment snapshots and descriptions instead. See `PLAN.md` for the required completion work.

### Module 6 - Files and Reports

Use this module to inspect data files created by the app.

Main actions:

- `Refresh`: update file metadata.
- `All`, `Binary`, `Text`: filter file list.
- Select a file to view path, type, size, modification time, existence, and a preview.

## 7. Useful development commands

```sh
rtk meson compile -C build
rtk meson compile -C build lint
rtk ./build/progtp-native
rtk ./build/progtp-tui
rtk ./build/progtp-server --port 8000 --public build
```

## TODO:

- [ ] make the cursor look "clickable" on web and native
- [ ] make the buttons look clickable on web and native
- [ ] support real file uploads with file dialogs
- [ ] module 4 sort by priority
