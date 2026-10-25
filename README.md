# PC MCAL

PC 上的 AUTOSAR MCAL，跑�?**VCOS** 里，�?**VCOS Studio** 配置。驱动保持标准模块名�?API（`Can_Init`、`Dio_ReadChannel`、`Lin_SendFrame` 等），把寄存器访问换成对 **EcuBus-Pro Simulate** �?JSON-RPC。Peak / Kvaser / Vector 等真实适配器不在这�?API 上�?

当前已有 **Can**。Dio、Lin 等按同一套目录和配置工具接入，模块名保持 AUTOSAR 名，上层（CanIf、Dio 用户、LinIf）可以直接引用�?

## 依赖

| 依赖 | 用�?|
| --- | --- |
| `vcos/components` | Os、CanIf、PduR、Com �?BSW。本包的 `plug_in_path` 指到这里 |
| `vcos/vcos_studio` | 配置工具。工程的 system_path 指向本包�?`configurator_plug_in`，不要用芯片那份 |
| EcuBus-Pro Simulate | 虚拟总线。JSON-RPC 在工程里�?Simulate 设备打开后才监听 |
| Visual Studio 20xx | 目标 `PC_Common` 的编译器�?`vs20xx`，不�?gcc |

## 目录

```text
vcos/vendor/ecubus
├── README.md
├── configurator_plug_in          # Studio system_path（目�?PC_Common�?
└── components
    └── Can                        # 已实现。后�?Dio、Lin 与它平级
        ├── CMakeLists.txt         # CONFIG_MODULE_CAN �?pc_can_driver
        ├── Kconfig
        ├── inc / src
        └── config_plug_in         # BSWMD、默�?ECUC、生成器、校�?
```

每个 MCAL 模块�?`components/<Module>/`。路径里必须�?`/components/`，Studio 才能�?`components.<Module>.config_plug_in...` 加载生成器。`config_plug_in/base_config/bswmd/comm_bswmd` 里的 `comm` 是芯片无关回退目录，不�?Communication 模块。Studio 先找 `bswmd/<target>`，没有再�?`comm_bswmd`�?

## 配置工具

�?`vcos` 目录启动 Studio�?

```powershell
vcos_studio/configurator/configurator.bat
```

新建或打开工程时：

1. **Configurator PlugIn Path / system_path** 设为 `vcos/vendor/ecubus/configurator_plug_in`�?
2. **Target** �?`PC_Common`�?*Compiler** �?`vs20xx`，MCAL 版本 `PC_MCAL_0.1.0`�?
3. 使能需要的 BSW（来�?`vcos/components`）和本包�?MCAL（来�?`vendor/ecubus/components`）�?
4. 生成代码到工程的 `gen_data/<Module>/`�?

`configurator_plug_in/components_plug_in_cfg.json` 是模块清单。BSW �?`plug_in_path` �?`../../../components/...`（即 `vcos/components`；因本包�?vendor/ecubus 下多一层）。MCAL �?`plug_in_path` �?`../components/<Module>`。`PC_Common` �?`platform_cfg` 不登记芯�?MCAL（例�?Infineon `Can_17_McmCan`），避免和本包同名模块冲突�?

不要�?`components/Can` 当用户插件导入。用户插件不允许复用已在 system_path 里登记的模块名�?

`group` 只能�?Studio 校验/生成树里已有的文件夹，例�?`Communication`、`I/O`、`Mode Management`、`Diagnostics`、`Memory`、`Timing`。写�?`MCAL` 时，打开 Validate 会在组节点上崩溃。MCAL 身份靠模块自己的生成器，`is_mcal` 保持 `false`，这�?Studio 会调用本包的 `generate_dynamic_code`，而不会走芯片 MCAL 命令行生成�?

新增 Dio、Lin 时：

1. �?`components/<Module>/` 放下源码、`CMakeLists.txt`（`if(CONFIG_MODULE_...)` + `nuttx_add_library`）、`config_plug_in`（BSWMD、`comm_ecuc` 默认配置、generator）�?
2. �?`components_plug_in_cfg.json` �?`module_cfg` 增加同名条目，`plug_in_path` �?`../components/<Module>`，`group` 用上面的已有文件夹，`support_generation` �?`true`，`is_mcal` �?`false`�?
3. 默认 ECUC 只放该模块在 PC 上必需的容器。控制器、通道、引脚由工程里再加�?

## 模块

| 模块 | 状�?| 上层 | group |
| --- | --- | --- | --- |
| Can | 已有 | CanIf | Communication |
| Dio | 计划 | 应用 / IoHwAb | I/O |
| Lin | 计划 | LinIf | Communication |

### Can

C 接口�?`Can_Init` / `Can_Write` / `Can_MainFunction_Read` 等。RPC 打到同一台机器上�?Simulate 虚拟总线。默�?ECUC 只有 `CanGeneral/CanPcRpc`（`127.0.0.1:17320`），控制器和 Hardware Object �?Studio 里添加�?

| 位置 | 参数 | 含义 |
| --- | --- | --- |
| `CanGeneral/CanPcRpc` | `CanRpcHost` / `CanRpcPort` | 默认 `127.0.0.1:17320`，与 EcuBus-Pro Home �?Setting �?**SIM-CAN** 一�?|
| | `CanRpcSocketPath` | 可�?Unix 域套接字 |
| `CanController` | `CanRpcVendor` | �?`simulate` |
| | `CanRpcHandle` | 空闲 Simulate 句柄。工程通常占用 **Simulate-0**，MCU �?**1**。RPC `controllerId` 等于该句�?|
| | `CanRpcCanFd` | 是否 CAN FD |
| | `HardwareName` | 传给 `Can.Init` 的控制器�?|
| | `CanControllerId` | AUTOSAR 控制�?ID，从 0 连续。可�?handle 不同，驱动会映射 |

BSWMD 保留 CanIf 要引用的容器（`CanController`、`CanHardwareObject`、`CanHwFilter`、`CanObjectId`、`CanHandleType`）。不提供 `internal_behavior`：MainFunction 由调度直接调用�?

使用顺序�?

1. Studio 里使�?Can �?CanIf，配�?`CanPcRpc`、控制器、HOH；CanIf 引用本模块的 Controller / Hardware Object�?
2. 生成 `gen_data/Can/Can_Cfg.h`、`Can_PBcfg.c`�?
3. 先启动带 **Simulate-0** �?EcuBus-Pro 工程（GUI，或 `ecb_cli seq` / `ecb_cli test`）�?
4. 再跑 ECU 软件。`Can.c` 打开空闲句柄（默�?1），�?Simulate-0 在同一条虚拟总线上�?

调度周期调用 `Can_MainFunction_Read` / `Write` / `BusOff` / `Mode`（PC 默认 POLLING）。RPC �?indication / confirmation 转成 `CanIf_RxIndication` / `CanIf_TxConfirmation`�?

## 编译

�?VCOS 工程里打开对应 Kconfig（Can �?`CONFIG_MODULE_CAN`），模块编成静态库 `pc_can_driver`。Windows 链接 `ws2_32`。生成代码目录是 `${GENDATA_DIR}/Can`�?

## 测试

Can 的主机测试在 `components/Can/test`，用 GoogleTest，不�?VCOS `ut`。说明见 [`components/Can/test/TEST.md`](components/Can/test/TEST.md)�?

```powershell
cmake -S vcos/vendor/ecubus/components/Can/test -B vcos/vendor/ecubus/components/Can/test/build
cmake --build vcos/vendor/ecubus/components/Can/test/build
ctest --test-dir vcos/vendor/ecubus/components/Can/test/build -C Debug --output-on-failure
```

## Host build (vcos_build, board `pc`)

Native PC host compile is registered as board `pc` in `vcos/build/build_config.json`.
Studio stays `vs20xx`; host uses `-compiler gcc`. See `vcos/build/PC_BUILD.md`.
Board defconfig: `vendor/ecubus/boards/pc/configs/vcos` (reuses semidrive sim CUSTOM board/chip; no `-sim 1`).