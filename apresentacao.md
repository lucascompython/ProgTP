# ProgTp - Apresentação do Projeto

Sistema Mini NOC para Monitorização da Rede de uma PME. Aplicação em C com
interface TUI (Terminal UI), nativa (SDL3) e Web (WASM).

──

## Arquitetura Geral

```
src/
├── app/            # UI (Clay layout) + state machine
├── client/         # Client-side connectivity + sensor import
├── server/         # Servidor HTTP para acesso remoto
├── domain/         # Business logic (inventory, incidents, config, sensors, reports, connectivity)
├── protocol/       # Serialização JSON para cliente↔servidor
├── platform/       # Backends: native (SDL3), tui (Termbox2), web (WASM)
└── common/         # Utilities (text, time, error)
```

**Entry point (TUI):** [`src/platform/tui/tui_termbox2.c`](src/platform/tui/tui_termbox2.c#L1)
**Entry point (Native):** [`src/platform/native/native_sdl3.c`](src/platform/native/native_sdl3.c#L1)
**State machine:** [`src/app/app.h`](src/app/app.h#L136) - `ProgTP_AppState`
**Module dispatch:** [`src/app/app_ui.c`](src/app/app_ui.c#L15) - `HandleUiInteraction()`

──

## Módulo 1 - Inventário de Equipamentos (Linked List)

### Estrutura de dados: Lista Ligada

| Elemento                 | Ficheiro                                                                           |
| ------------------------ | ---------------------------------------------------------------------------------- |
| Nó da lista              | [`ProgTP_EquipmentNode`](src/domain/inventory/equipment_inventory.h#L50)           |
| Estrutura do equipamento | [`ProgTP_Equipment`](src/domain/inventory/equipment_inventory.h#L24)               |
| Cabeça da lista          | [`ProgTP_EquipmentInventory.head`](src/domain/inventory/equipment_inventory.h#L61) |

### Operações CRUD

| Operação              | Localização                                                                              |
| --------------------- | ---------------------------------------------------------------------------------------- |
| Adicionar equipamento | [`ProgTP_EquipmentInventoryAdd`](src/domain/inventory/equipment_inventory.c#L142)        |
| Remover equipamento   | [`ProgTP_EquipmentInventoryRemove`](src/domain/inventory/equipment_inventory.c#L186)     |
| Atualizar equipamento | [`ProgTP_EquipmentInventoryUpdate`](src/domain/inventory/equipment_inventory.c#L157)     |
| Pesquisar por código  | [`ProgTP_EquipmentInventoryFindByCode`](src/domain/inventory/equipment_inventory.c#L220) |
| Contar equipamentos   | [`ProgTP_EquipmentInventoryGetCount`](src/domain/inventory/equipment_inventory.c#L210)   |

### Filtros e Pesquisa

| Funcionalidade              | Localização                                       |
| --------------------------- | ------------------------------------------------- |
| Filtrar por estado          | [`SetStateFilter`](src/app/app.c#L696)            |
| Filtrar por tipo            | [`SetTypeFilter`](src/app/app.c#L709)             |
| Pesquisar por código/IP/MAC | [`SubmitInput`](src/app/app.c#L1068)              |
| UI do módulo                | [`InventoryModule`](src/app/app_inventory.c#L509) |

### Requisitos cumpridos

- [x] Adicionar, remover, alterar equipamentos
- [x] Alterar estado (Operacional, Em Falha, Em Manutenção, Desativado)
- [x] Listar todos / por tipo / por estado
- [x] Pesquisar por código, IP ou MAC
- [x] Código interno único atribuído automaticamente
- [x] Verificação de incidentes pendentes antes de remover

──

## Módulo 2 - Testes de Conectividade

### Execução de comandos

| Funcionalidade               | Localização                                                                       |
| ---------------------------- | --------------------------------------------------------------------------------- |
| Executar ping                | [`ProgTP_ConnectivityExecute`](src/domain/connectivity/connectivity.c#L30)        |
| Analisar resultado ping      | [`PingOutputIndicatesResponse`](src/domain/connectivity/connectivity.c#L110)      |
| Executar comando customizado | [`ProgTP_ConnectivityExecuteCustom`](src/domain/connectivity/connectivity.c#L150) |
| Ping a todos os equipamentos | [`ProgTP_AppHandleAction` case `CONNECTIVITY_PING_ALL`](src/app/app.c#L770)       |
| UI do módulo                 | [`ConnectivityModule`](src/app/app_connectivity.c#L238)                           |

### Guardar e analisar resultados

| Funcionalidade                          | Localização                                                                            |
| --------------------------------------- | -------------------------------------------------------------------------------------- |
| Guardar output em ficheiro              | [`fwrite` em connectivity.c](src/domain/connectivity/connectivity.c#L90)               |
| Ler ficheiro de resultado               | [`fread` em connectivity.c](src/domain/connectivity/connectivity.c#L126)               |
| Log de monitorização                    | [`log_monitorizacao.txt`](src/domain/connectivity/connectivity.c#L60)                  |
| Criar incidente automático (ping falha) | [`ProgTP_IncidentStoreAppendPingFailure`](src/domain/connectivity/connectivity.c#L255) |

### Requisitos cumpridos

- [x] Selecionar equipamento e executar ping
- [x] Guardar resultado bruto em ficheiro de texto
- [x] Analisar ficheiro e determinar resposta
- [x] Atualizar data da última verificação
- [x] Alterar estado para "Em Falha" quando sem resposta
- [x] Registar teste em `log_monitorizacao.txt`
- [x] Criar incidente automático quando equipamento não responde
- [x] Teste geral da rede (ping a todos)

──

## Módulo 3 - Monitorização de Sensores

### Estrutura de dados

| Elemento                           | Ficheiro                                                        |
| ---------------------------------- | --------------------------------------------------------------- |
| Leitura de sensor                  | [`ProgTP_SensorReading`](src/domain/sensors/sensor_store.h#L18) |
| Store de sensores (array dinâmico) | [`ProgTP_SensorStore`](src/domain/sensors/sensor_store.h#L39)   |

### Importação e processamento

| Funcionalidade                     | Localização                                                              |
| ---------------------------------- | ------------------------------------------------------------------------ |
| Importar de ficheiro de texto      | [`ProgTP_SensorStoreImportText`](src/domain/sensors/sensor_store.c#L302) |
| Parse de cada linha                | [`ImportLines`](src/domain/sensors/sensor_store.c#L220)                  |
| Caminho customizado (Clear button) | [`HandleModalAction` case `SENSOR_CLEAR_PATH`](src/app/app_modal.c#L326) |
| Importar da API (extra)            | [`ProgTP_FetchSensorApi`](src/client/command_client.c#L250)              |
| UI do módulo                       | [`SensorModule`](src/app/app_sensors.c#L279)                             |

### Ficheiro de entrada

| Ficheiro                                             | Descrição                                   |
| ---------------------------------------------------- | ------------------------------------------- |
| [`sensores_rack.txt`](sensores_rack.txt)             | Ficheiro de exemplo incluído no repositório |
| [`sensores_rack_outro.txt`](sensores_rack_outro.txt) | Ficheiro alternativo para testes            |

### Requisitos cumpridos

- [x] Importar leituras de `sensores_rack.txt`
- [x] Formato: `codigo_sensor;tipo;valor;unidade;estado`
- [x] Listar leituras mais recentes
- [x] Pesquisar por código de sensor
- [x] Identificar leituras com estado AVISO, CRITICO, FALHA_REDE
- [x] Criar incidente automático para leituras anómalas
- [x] Registar importação em `log_sensores.txt`

──

## Módulo 4 - Incidentes Técnicos (Queue / Fila)

### Estrutura de dados: Fila (Queue)

| Elemento                  | Ficheiro                                                                    |
| ------------------------- | --------------------------------------------------------------------------- |
| Nó da fila                | [`ProgTP_IncidentNode`](src/domain/incidents/incident_store.h#L35)          |
| Estrutura do incidente    | [`ProgTP_Incident`](src/domain/incidents/incident_store.h#L14)              |
| Fila (front/back)         | [`ProgTP_IncidentStore`](src/domain/incidents/incident_store.h#L42)         |
| Enfileirar (push back)    | [`ProgTP_IncidentStoreEnqueue`](src/domain/incidents/incident_store.c#L216) |
| Desenfileirar (pop front) | [`ProgTP_IncidentStoreDequeue`](src/domain/incidents/incident_store.c#L275) |

### Operações

| Operação                       | Localização                                                                            |
| ------------------------------ | -------------------------------------------------------------------------------------- |
| Criar manualmente              | [`ProgTP_IncidentStoreEnqueue`](src/domain/incidents/incident_store.c#L216)            |
| Criar automaticamente (ping)   | [`ProgTP_IncidentStoreAppendPingFailure`](src/domain/connectivity/connectivity.c#L255) |
| Criar automaticamente (sensor) | [`CreateSensorIncident`](src/domain/sensors/sensor_store.c#L186)                       |
| Processar próximo da fila      | [`HandleUiInteraction` case `INCIDENT_START`](src/app/app.c#L888)                      |
| Concluir incidente             | [`HandleUiInteraction` case `INCIDENT_COMPLETE`](src/app/app.c#L899)                   |
| Ordenação por ID/Prioridade    | [`incident_sort_mode`](src/app/app.c#L783)                                             |
| Filtrar por estado             | [`IncidentVisible`](src/app/app_incidents.c#L30)                                       |
| UI do módulo                   | [`IncidentModule`](src/app/app_incidents.c#L394)                                       |

### Requisitos cumpridos

- [x] Criar incidente manualmente
- [x] Criar automaticamente por falha de ping
- [x] Criar automaticamente por leitura anómala de sensor
- [x] Fila de atendimento (FIFO)
- [x] Processar próximo incidente (Em Curso)
- [x] Concluir incidente com data/hora
- [x] Listar pendentes / em curso / concluídos
- [x] Listar por equipamento ou sensor
- [x] Listar por prioridade

──

## Módulo 5 - Registo de Configurações (Stack / Pilha)

### Estrutura de dados: Pilha (Stack)

| Elemento                    | Ficheiro                                                              |
| --------------------------- | --------------------------------------------------------------------- |
| Nó da pilha                 | [`ProgTP_ConfigNode`](src/domain/config/config_history.h#L39)         |
| Entrada de configuração     | [`ProgTP_ConfigEntry`](src/domain/config/config_history.h#L24)        |
| Histórico (topo/undo stack) | [`ProgTP_ConfigHistory`](src/domain/config/config_history.h#L50)      |
| Push (registar alteração)   | [`ProgTP_ConfigHistoryPush`](src/domain/config/config_history.c#L242) |
| Pop (reverter/undo)         | [`ProgTP_ConfigHistoryUndo`](src/domain/config/config_history.c#L338) |

### Operações

| Operação                  | Localização                                                                     |
| ------------------------- | ------------------------------------------------------------------------------- |
| Registar configuração     | [`ProgTP_ConfigHistoryPush`](src/domain/config/config_history.c#L63)            |
| Reverter última (Undo)    | [`HandleUiInteraction` case `CONFIG_UNDO`](src/app/app.c#L975)                  |
| Refazer (Redo) - extra    | [`HandleUiInteraction` case `CONFIG_REDO`](src/app/app.c#L985)                  |
| Consultar N mais recentes | [`ProgTP_ConfigHistoryGetByIndex`](src/domain/config/config_history.c#L280)     |
| Histórico por equipamento | [`ProgTP_ConfigHistoryGetByEquipment`](src/domain/config/config_history.c#L300) |
| Limpar registo            | [`ProgTP_ConfigHistoryClear`](src/domain/config/config_history.c#L70)           |
| UI do módulo              | [`ConfigModule`](src/app/app_config.c#L262)                                     |

### Requisitos cumpridos

- [x] Registar nova configuração de equipamento
- [x] Consultar última configuração realizada
- [x] Consultar N configurações mais recentes
- [x] Reverter última configuração (restaurar valor anterior)
- [x] Histórico de configurações por equipamento
- [x] Limpar registo mediante confirmação

──

## Módulo 6 - Ficheiros e Relatórios

### Ficheiros binários

| Ficheiro                | Load                                                                                     | Save                                                                                     |
| ----------------------- | ---------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------- |
| `equipamentos.dat`      | [`ProgTP_EquipmentInventoryLoadBinary`](src/domain/inventory/equipment_inventory.c#L373) | [`ProgTP_EquipmentInventorySaveBinary`](src/domain/inventory/equipment_inventory.c#L341) |
| `incidentes.dat`        | [`ProgTP_IncidentStoreLoad`](src/domain/incidents/incident_store.c#L130)                 | [`ProgTP_IncidentStoreSave`](src/domain/incidents/incident_store.c#L181)                 |
| `configuracoes.dat`     | [`ProgTP_ConfigHistoryLoad`](src/domain/config/config_history.c#L138)                    | [`ProgTP_ConfigHistorySave`](src/domain/config/config_history.c#L201)                    |
| `leituras_sensores.dat` | [`ProgTP_SensorStoreLoadBinary`](src/domain/sensors/sensor_store.c#L430)                 | [`ProgTP_SensorStoreSaveBinary`](src/domain/sensors/sensor_store.c#L410)                 |

Cada ficheiro binário usa um header com magic number e version:

- Inventário: [`ProgTP_EquipmentFileHeader`](src/domain/inventory/equipment_inventory.h#L108)
- Incidentes: [`ProgTP_IncidentFileHeader`](src/domain/incidents/incident_store.h#L48)
- Configurações: [`ProgTP_ConfigFileHeader`](src/domain/config/config_history.h#L18)
- Sensores: [`ProgTP_SensorFileHeader`](src/domain/sensors/sensor_store.h#L32)

### Ficheiros de texto

| Ficheiro                                | Gerado por                                                                       |
| --------------------------------------- | -------------------------------------------------------------------------------- |
| `resultado_ping.txt`                    | [`ProgTP_ConnectivityExecute`](src/domain/connectivity/connectivity.c#L75)       |
| `log_monitorizacao.txt`                 | [`ProgTP_ConnectivityExecute`](src/domain/connectivity/connectivity.c#L60)       |
| `log_sensores.txt`                      | [`ImportLines`](src/domain/sensors/sensor_store.c#L258)                          |
| `relatorio_estado_rede_<mês>_<ano>.txt` | [`ProgTP_GenerateNetworkStatusReport`](src/domain/reports/report_generator.c#L9) |
| `relatorio_incidentes_<mês>_<ano>.txt`  | [`ProgTP_GenerateIncidentReport`](src/domain/reports/report_generator.c#L185)    |

### Relatórios

| Funcionalidade                 | Localização                                                                      |
| ------------------------------ | -------------------------------------------------------------------------------- |
| Relatório de estado da rede    | [`ProgTP_GenerateNetworkStatusReport`](src/domain/reports/report_generator.c#L9) |
| Relatório mensal de incidentes | [`ProgTP_GenerateIncidentReport`](src/domain/reports/report_generator.c#L185)    |
| Pré-visualização de ficheiros  | [`FilesModule`](src/app/app_files.c#L560)                                        |
| UI do módulo                   | [`FilesModule`](src/app/app_files.c#L556)                                        |

### Requisitos cumpridos

- [x] Carregar dados ao iniciar
- [x] Guardar dados ao sair
- [x] Importar sensores de ficheiro de texto
- [x] Guardar resultados de comandos em ficheiros de texto
- [x] Gerar relatório de estado da rede
- [x] Gerar relatório mensal de incidentes
- [x] Pré-visualização de conteúdo dos ficheiros no módulo 6

──

## Funcionalidades Extra

### Interface multi-plataforma

| Plataforma                     | Localização                                                    |
| ------------------------------ | -------------------------------------------------------------- |
| Terminal UI (Termbox2)         | [`src/platform/tui/`](src/platform/tui/tui_termbox2.c#L1)      |
| Nativa (SDL3 + aceleração GPU) | [`src/platform/native/`](src/platform/native/native_sdl3.c#L1) |
| Web (WASM + SDL3)              | [`src/platform/web/`](src/platform/web/web.c#L1)               |

### Arquitetura Cliente-Servidor

| Componente     | Localização                                                     |
| -------------- | --------------------------------------------------------------- |
| Servidor HTTP  | [`src/server/server.c`](src/server/server.c#L1)                 |
| Protocolo JSON | [`src/protocol/protocol.c`](src/protocol/protocol.c#L1)         |
| Cliente remoto | [`src/client/command_client.c`](src/client/command_client.c#L1) |

Permite gestão remota do inventário e execução de comandos via HTTP.

### API de Sensores

| Funcionalidade       | Localização                                                 |
| -------------------- | ----------------------------------------------------------- |
| Fetch da API externa | [`ProgTP_FetchSensorApi`](src/client/command_client.c#L250) |
| Botão "Fetch API"    | [`SensorModule`](src/app/app_sensors.c#L317)                |

### Caminho customizado para sensores

| Funcionalidade                           | Localização                                                              |
| ---------------------------------------- | ------------------------------------------------------------------------ |
| Modal de ficheiro                        | [`SensorFileModal`](src/app/app_modal.c#L558)                            |
| Limpar campo (Clear)                     | [`HandleModalAction` case `SENSOR_CLEAR_PATH`](src/app/app_modal.c#L326) |
| Suporte a caminhos absolutos e relativos | [`ProgTP_SensorStoreImportText`](src/domain/sensors/sensor_store.c#L302) |

### Undo/Redo para configurações (Stack dupla)

| Funcionalidade | Localização                                                            |
| -------------- | ---------------------------------------------------------------------- |
| Undo stack     | [`ProgTP_ConfigHistory.undos`](src/domain/config/config_history.h#L52) |
| Redo stack     | [`ProgTP_ConfigHistory.redos`](src/domain/config/config_history.h#L53) |
| Undo action    | [`HandleUiInteraction` case `CONFIG_UNDO`](src/app/app.c#L975)         |
| Redo action    | [`HandleUiInteraction` case `CONFIG_REDO`](src/app/app.c#L985)         |

### Ordenação de incidentes

| Funcionalidade         | Localização                                                                        |
| ---------------------- | ---------------------------------------------------------------------------------- |
| Ordenar por ID         | [`ProgTP_IncidentStoreSortById`](src/domain/incidents/incident_store.c#L320)       |
| Ordenar por prioridade | [`ProgTP_IncidentStoreSortByPriority`](src/domain/incidents/incident_store.c#L340) |

### Pesquisa por código de sensor

| Funcionalidade        | Localização                                             |
| --------------------- | ------------------------------------------------------- |
| Campo de pesquisa     | [`SensorSearchField`](src/app/app_sensors.c#L321)       |
| Submissão da pesquisa | [`SubmitInput` case `SENSOR_CODE`](src/app/app.c#L1095) |

### Filtros avançados

| Funcionalidade                      | Localização                                                       |
| ----------------------------------- | ----------------------------------------------------------------- |
| Filtro de estado (inventário)       | [`CycleStateFilter`](src/app/app.c#L696)                          |
| Filtro de tipo (inventário)         | [`CycleTypeFilter`](src/app/app.c#L709)                           |
| Filtro de anomalias (sensores)      | [`PROGTP_APP_ACTION_SENSOR_FILTER_ANOMALOUS`](src/app/app.c#L737) |
| Filtro de ficheiros (binário/texto) | [`PROGTP_APP_ACTION_FILES_FILTER_BINARY`](src/app/app.c#L825)     |

### Auto-import de incidentes

| Funcionalidade        | Localização                                                             |
| --------------------- | ----------------------------------------------------------------------- |
| Importação automática | [`HandleUiInteraction` case `INCIDENT_AUTO_IMPORT`](src/app/app.c#L914) |

### Persistência automática

| Funcionalidade  | Localização                               |
| --------------- | ----------------------------------------- |
| Load ao iniciar | [`ProgTP_AppInit`](src/app/app.c#L130)    |
| Save ao sair    | [`ProgTP_AppDestroy`](src/app/app.c#L200) |

### UI compacta (modo terminal)

| Funcionalidade        | Localização                                            |
| --------------------- | ------------------------------------------------------ |
| Flag de modo compacto | [`progtp_ui_compact`](src/app/app.c#L44)               |
| Deteção de terminal   | [`ProgTP_AppSetTerminalRendering`](src/app/app.c#L220) |

──

## Estruturas de Dados (Resumo)

| Estrutura     | Tipo               | Implementação                | Ficheiro                                                                      |
| ------------- | ------------------ | ---------------------------- | ----------------------------------------------------------------------------- |
| Inventário    | **Lista ligada**   | Singly-linked list com head  | [`ProgTP_EquipmentInventory`](src/domain/inventory/equipment_inventory.h#L59) |
| Incidentes    | **Fila (Queue)**   | Singly-linked com front/back | [`ProgTP_IncidentStore`](src/domain/incidents/incident_store.h#L42)           |
| Configurações | **Pilha (Stack)**  | Duas stacks (undo + redo)    | [`ProgTP_ConfigHistory`](src/domain/config/config_history.h#L50)              |
| Sensores      | **Array dinâmico** | Dynamic array com capacity   | [`ProgTP_SensorStore`](src/domain/sensors/sensor_store.h#L39)                 |

──

## Exemplo de Fluxo

| #   | Requisito                           | Localização                                                                            |
| --- | ----------------------------------- | -------------------------------------------------------------------------------------- |
| 1   | Iniciar e carregar dados binários   | [`ProgTP_AppInit`](src/app/app.c#L130)                                                 |
| 2   | Registar equipamentos               | [`ProgTP_EquipmentInventoryAdd`](src/domain/inventory/equipment_inventory.c#L142)      |
| 3   | Executar ping a equipamento         | [`ProgTP_ConnectivityExecute`](src/domain/connectivity/connectivity.c#L30)             |
| 4   | Guardar resultado do ping           | [`fwrite` output](src/domain/connectivity/connectivity.c#L90)                          |
| 5   | Ler resultado e indicar resposta    | [`PingOutputIndicatesResponse`](src/domain/connectivity/connectivity.c#L110)           |
| 6   | Criar incidente automático (ping)   | [`ProgTP_IncidentStoreAppendPingFailure`](src/domain/connectivity/connectivity.c#L255) |
| 7   | Importar `sensores_rack.txt`        | [`ProgTP_SensorStoreImportText`](src/domain/sensors/sensor_store.c#L302)               |
| 8   | Criar incidente automático (sensor) | [`CreateSensorIncident`](src/domain/sensors/sensor_store.c#L186)                       |
| 9   | Processar próximo incidente         | [`INCIDENT_START`](src/app/app.c#L888)                                                 |
| 10  | Concluir incidente                  | [`INCIDENT_COMPLETE`](src/app/app.c#L899)                                              |
| 11  | Registar alteração de configuração  | [`ProgTP_ConfigHistoryPush`](src/domain/config/config_history.c#L242)                  |
| 12  | Reverter última configuração        | [`CONFIG_UNDO`](src/app/app.c#L975)                                                    |
| 13  | Gerar relatório de estado da rede   | [`ProgTP_GenerateNetworkStatusReport`](src/domain/reports/report_generator.c#L9)       |
| 14  | Guardar dados ao sair               | [`ProgTP_AppDestroy`](src/app/app.c#L205)                                              |

──
