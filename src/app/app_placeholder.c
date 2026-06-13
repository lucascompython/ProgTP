#include "app.h"
#include "app_internal.h"

#include <stdio.h>
#include <string.h>

static const char *ModuleFileName(int module) {
    static const char *file_names[] = {
        "",
        "equipamentos.dat",
        "resultado_ping.txt / log_monitorizacao.txt",
        "sensores_rack.txt / log_sensores.txt",
        "incidentes.dat",
        "configuracoes.dat",
        "relatorio_estado_rede_mes_ano.txt",
        "tecnicos.dat",
        "settings.dat",
    };
    return module >= 1 && module <= 8 ? file_names[module] : "";
}

static const char *ModuleTableTitle(int module) {
    static const char *titles[] = {
        "",
        "Equipment",
        "Connectivity checks",
        "Sensor readings",
        "Incident queue",
        "Configuration history",
        "Generated reports",
        "Technician registry",
        "Runtime settings",
    };
    return module >= 1 && module <= 8 ? titles[module] : "Records";
}

static const char *ModulePrimaryAction(int module) {
    static const char *actions[] = {
        "",
        "Add",
        "Run Ping",
        "Read Sensors",
        "Queue",
        "Rollback",
        "Generate",
        "Assign",
        "Apply",
    };
    return module >= 1 && module <= 8 ? actions[module] : "Open";
}

static const char *ModuleRowText(int module, int row) {
    static const char *rows[9][4] = {
        { "", "", "", "" },
        { "", "", "", "" },
        {
            "192.168.1.1 | Core Router | reachable | 2 ms | 0% loss",
            "192.168.1.10 | Office AP 1 | reachable | 6 ms | 0% loss",
            "192.168.1.20 | NAS Backup | reachable | 1 ms | 0% loss",
            "192.168.1.30 | UPS Rack | reachable | 4 ms | 0% loss",
        },
        {
            "Rack A temperature | 24 C | normal",
            "Rack A humidity | 42% | normal",
            "UPS battery load | 66% | normal",
            "Switch fan speed | 3100 rpm | normal",
        },
        {
            "#1042 | Office AP 1 | maintenance | assigned",
            "#1043 | Printer VLAN | open | priority medium",
            "#1044 | NAS Backup | pending review | priority low",
            "#1045 | Core Router | closed | priority high",
        },
        {
            "2026-05-28 09:15 | Core Router | ACL updated",
            "2026-05-28 10:30 | Access Switch A | VLAN 20 added",
            "2026-05-28 11:40 | Office AP 1 | channel changed",
            "2026-05-28 15:10 | UPS Rack | SNMP threshold updated",
        },
        {
            "estado_rede_05_2026.txt | monthly state | ready",
            "incidentes_abertos.txt | open incidents | ready",
            "equipamentos_falha.txt | failed devices | ready",
            "monitorizacao_diaria.txt | daily monitoring | ready",
        },
        {
            "Ana Silva | networking | available",
            "Bruno Costa | infrastructure | assigned",
            "Carla Gomes | helpdesk | available",
            "Diogo Martins | security | standby",
        },
        {
            "HTTP mode | local/server | enabled",
            "Storage path | equipamentos.dat | enabled",
            "Theme | operator light | enabled",
            "Autosave | native/tui | enabled",
        },
    };
    if (module < 2 || module > 8 || row < 0 || row >= 4) {
        return "";
    }
    return rows[module][row];
}

static const char *ModuleDetailText(int module) {
    static const char *details[] = {
        "",
        "",
        "Run host checks, review ping results, and save the monitoring log.",
        "Track rack sensor values and flag readings outside the expected range.",
        "Manage incident ordering, assignment, and pending technical work.",
        "Review configuration changes and choose rollback points from the stack.",
        "Generate network state reports and export filtered operational lists.",
        "Keep technician records available for assignment and incident ownership.",
        "Switch local/server modes and keep platform-specific runtime settings.",
    };
    return module >= 2 && module <= 8 ? details[module] : "";
}

void PlaceholderModule(int module) {
    CLAY(CLAY_IDI("PlaceholderModule", (uint32_t)module), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            .childGap = 12,
        },
    }) {
        CLAY(CLAY_IDI("ModuleMetrics", (uint32_t)module), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 10,
            },
        }) {
            Metric("Module", ModuleName(module));
            Metric("Storage", ModuleFileName(module));
            Metric("Status", "Ready");
        }
        CLAY(CLAY_IDI("PlaceholderActions", (uint32_t)module), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 8,
            },
        }) {
            Button(200u + (uint32_t)module * 5u, ModulePrimaryAction(module), PROGTP_UI_MODULE_BASE + (uintptr_t)module, true, false);
            Button(201u + (uint32_t)module * 5u, "Add", PROGTP_UI_MODULE_BASE + (uintptr_t)module, false, false);
            Button(202u + (uint32_t)module * 5u, "Update", PROGTP_UI_MODULE_BASE + (uintptr_t)module, false, false);
            Button(203u + (uint32_t)module * 5u, "Save", PROGTP_APP_ACTION_SAVE, false, false);
            Button(204u + (uint32_t)module * 5u, "Load", PROGTP_APP_ACTION_LOAD, false, false);
        }
        CLAY(CLAY_IDI("ModuleContent", (uint32_t)module), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                .childGap = 12,
            },
        }) {
            CLAY(CLAY_IDI("ModuleTable", (uint32_t)module), {
                .layout = {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(.min = 220) },
                    .childGap = 2,
                },
                .backgroundColor = COLOR_SURFACE,
                .border = { .color = COLOR_LINE, .width = { 1, 1, 1, 1, 0 } },
                .cornerRadius = CLAY_CORNER_RADIUS(5),
            }) {
                CLAY(CLAY_IDI("ModuleTableHeader", (uint32_t)module), {
                    .layout = {
                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(34) },
                        .padding = { 10, 10, 0, 0 },
                        .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                    },
                    .backgroundColor = COLOR_SURFACE_DARK,
                }) {
                    TextLine(ModuleTableTitle(module), 13, COLOR_TEXT);
                }
                for (int row = 0; row < 4; ++row) {
                    CLAY(CLAY_IDI("ModuleRow", (uint32_t)module * 16u + (uint32_t)row), {
                        .layout = {
                            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(34) },
                            .padding = { 10, 10, 0, 0 },
                            .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                        },
                        .backgroundColor = row == 0 ? COLOR_ACCENT : (row % 2 == 0 ? COLOR_SURFACE : COLOR_SURFACE_ALT),
                    }) {
                        AttachInteraction(PROGTP_UI_MODULE_BASE + (uintptr_t)module);
                        TextLine(ModuleRowText(module, row), 12, row == 0 ? COLOR_WHITE : COLOR_TEXT);
                    }
                }
            }
            if (!progtp_ui_compact) {
                CLAY(CLAY_IDI("ModuleDetail", (uint32_t)module), {
                    .layout = {
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .sizing = { CLAY_SIZING_FIXED(300), CLAY_SIZING_GROW(0) },
                        .padding = CLAY_PADDING_ALL(14),
                        .childGap = 10,
                    },
                    .backgroundColor = COLOR_SURFACE,
                    .border = { .color = COLOR_LINE, .width = { 1, 1, 1, 1, 0 } },
                    .cornerRadius = CLAY_CORNER_RADIUS(5),
                }) {
                    TextLine("Selected record", 16, COLOR_TEXT);
                    TextLine(ModuleDetailText(module), 13, COLOR_MUTED);
                    TextLine(ModuleFileName(module), 13, COLOR_ACCENT_DARK);
                }
            }
        }
    }
}
