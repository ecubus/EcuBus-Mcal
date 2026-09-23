# PC MCAL

PC 上的 AUTOSAR MCAL，跑在 **VCOS** 里，用 **VCOS Studio** 配置。驱动保持标准模块名和 API（`Can_Init`、`Dio_ReadChannel`、`Lin_SendFrame` 等），把寄存器访问换成对 **EcuBus-Pro Simulate** 的 JSON-RPC。Peak / Kvaser / Vector 等真实适配器不在这条 API 上。

当前已有 **Can**。Dio、Lin 等按同一套目录和配置工具接入，模块名保持 AUTOSAR 名，上层（CanIf、Dio 用户、LinIf）可以直接引用。

## 依赖

| 依赖 | 用途 |
| --- | --- |
| `vcos/components` | Os、CanIf、PduR、Com 等 BSW。本包的 `plug_in_path` 指到这里 |
| `vcos/vcos_studio` | 配置工具。工程的 system_path 指向本包的 `configurator_plug_in`，不要用芯片那份 |
| EcuBus-Pro Simulate | 虚拟总线。JSON-RPC 在工程里的 Simulate 设备打开后才监听 |
| Visual Studio 20xx | 目标 `PC_Common` 的编译器是 `vs20xx`，不是 gcc |

## 目录

```text
vcos/mcal_pc
├── README.md
├── configurator_plug_in          # Studio system_path（目标 PC_Common）
└── components
    └── Can                        # 已实现。后续 Dio、Lin 与它平级
        ├── CMakeLists.txt         # CONFIG_MODULE_CAN → pc_can_driver
        ├── Kconfig
        ├── inc / src
        └── config_plug_in         # BSWMD、默认 ECUC、生成器、校验
```

每个 MCAL 模块是 `components/<Module>/`。路径里必须有 `/components/`，Studio 才能按 `components.<Module>.config_plug_in...` 加载生成器。`config_plug_in/base_config/bswmd/comm_bswmd` 里的 `comm` 是芯片无关回退目录，不是 Communication 模块。Studio 先找 `bswmd/<target>`，没有再找 `comm_bswmd`。

## 配置工具

在 `vcos` 目录启动 Studio：

```powershell
vcos_studio/configurator/configurator.bat
```

新建或打开工程时：

1. **Configurator PlugIn Path / system_path** 设为 `vcos/mcal_pc/configurator_plug_in`。
2. **Target** 选 `PC_Common`，**Compiler** 选 `vs20xx`，MCAL 版本 `PC_MCAL_0.1.0`。
3. 使能需要的 BSW（来自 `vcos/components`）和本包的 MCAL（来自 `mcal_pc/components`）。
4. 生成代码到工程的 `gen_data/<Module>/`。

`configurator_plug_in/components_plug_in_cfg.json` 是模块清单。BSW 的 `plug_in_path` 是 `../../components/...`（即 `vcos/components`）。MCAL 的 `plug_in_path` 是 `../components/<Module>`。`PC_Common` 的 `platform_cfg` 不登记芯片 MCAL（例如 Infineon `Can_17_McmCan`），避免和本包同名模块冲突。

不要把 `components/Can` 当用户插件导入。用户插件不允许复用已在 system_path 里登记的模块名。

`group` 只能用 Studio 校验/生成树里已有的文件夹，例如 `Communication`、`I/O`、`Mode Management`、`Diagnostics`、`Memory`、`Timing`。写成 `MCAL` 时，打开 Validate 会在组节点上崩溃。MCAL 身份靠模块自己的生成器，`is_mcal` 保持 `false`，这样 Studio 会调用本包的 `generate_dynamic_code`，而不会走芯片 MCAL 命令行生成。

新增 Dio、Lin 时：

1. 在 `components/<Module>/` 放下源码、`CMakeLists.txt`（`if(CONFIG_MODULE_...)` + `nuttx_add_library`）、`config_plug_in`（BSWMD、`comm_ecuc` 默认配置、generator）。
2. 在 `components_plug_in_cfg.json` 的 `module_cfg` 增加同名条目，`plug_in_path` 为 `../components/<Module>`，`group` 用上面的已有文件夹，`support_generation` 为 `true`，`is_mcal` 为 `false`。
3. 默认 ECUC 只放该模块在 PC 上必需的容器。控制器、通道、引脚由工程里再加。

## 模块

| 模块 | 状态 | 上层 | group |
| --- | --- | --- | --- |
| Can | 已有 | CanIf | Communication |
| Dio | 计划 | 应用 / IoHwAb | I/O |
| Lin | 计划 | LinIf | Communication |

### Can

C 接口是 `Can_Init` / `Can_Write` / `Can_MainFunction_Read` 等。RPC 打到同一台机器上的 Simulate 虚拟总线。默认 ECUC 只有 `CanGeneral/CanPcRpc`（`127.0.0.1:17320`），控制器和 Hardware Object 在 Studio 里添加。

| 位置 | 参数 | 含义 |
| --- | --- | --- |
| `CanGeneral/CanPcRpc` | `CanRpcHost` / `CanRpcPort` | 默认 `127.0.0.1:17320`，与 EcuBus-Pro Home → Setting → **SIM-CAN** 一致 |
| | `CanRpcSocketPath` | 可选 Unix 域套接字 |
| `CanController` | `CanRpcVendor` | 仅 `simulate` |
| | `CanRpcHandle` | 空闲 Simulate 句柄。工程通常占用 **Simulate-0**，MCU 用 **1**。RPC `controllerId` 等于该句柄 |
| | `CanRpcCanFd` | 是否 CAN FD |
| | `HardwareName` | 传给 `Can.Init` 的控制器名 |
| | `CanControllerId` | AUTOSAR 控制器 ID，从 0 连续。可与 handle 不同，驱动会映射 |

BSWMD 保留 CanIf 要引用的容器（`CanController`、`CanHardwareObject`、`CanHwFilter`、`CanObjectId`、`CanHandleType`）。不提供 `internal_behavior`：MainFunction 由调度直接调用。

使用顺序：

1. Studio 里使能 Can 和 CanIf，配好 `CanPcRpc`、控制器、HOH；CanIf 引用本模块的 Controller / Hardware Object。
2. 生成 `gen_data/Can/Can_Cfg.h`、`Can_PBcfg.c`。
3. 先启动带 **Simulate-0** 的 EcuBus-Pro 工程（GUI，或 `ecb_cli seq` / `ecb_cli test`）。
4. 再跑 ECU 软件。`Can.c` 打开空闲句柄（默认 1），与 Simulate-0 在同一条虚拟总线上。

调度周期调用 `Can_MainFunction_Read` / `Write` / `BusOff` / `Mode`（PC 默认 POLLING）。RPC 的 indication / confirmation 转成 `CanIf_RxIndication` / `CanIf_TxConfirmation`。

## 编译

在 VCOS 工程里打开对应 Kconfig（Can 为 `CONFIG_MODULE_CAN`），模块编成静态库 `pc_can_driver`。Windows 链接 `ws2_32`。生成代码目录是 `${GENDATA_DIR}/Can`。

## 测试

Can 的主机测试在 `components/Can/test`，用 GoogleTest，不走 VCOS `ut`。说明见 [`components/Can/test/TEST.md`](components/Can/test/TEST.md)。

```powershell
cmake -S vcos/mcal_pc/components/Can/test -B vcos/mcal_pc/components/Can/test/build
cmake --build vcos/mcal_pc/components/Can/test/build
ctest --test-dir vcos/mcal_pc/components/Can/test/build -C Debug --output-on-failure
```
