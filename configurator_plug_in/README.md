# PC MCAL Studio system plugin pack

Point VCOS Studio **system_path** at this directory. Do not import `components/Can` as a user plugin.

## New project

1. Studio → New Project.
2. **Configurator PlugIn Path / system_path**: `vcos/mcal_pc/configurator_plug_in`.
3. **Target**: `PC_Common` (compiler `vs20xx`).
4. Enable **Can**, **CanIf**, and the other BSW modules you need. CanIf depends on Can.

Studio loads **Can** from `vcos/mcal_pc/components/Can` (module name stays `Can`, so CanIf refs work). Os, CanIf, PduR, Com, and the rest come from `vcos/components`. `PC_Common` does not register Infineon `Can_17_McmCan`, so there is no name clash.

## Layout

```text
mcal_pc/configurator_plug_in
├── components_plug_in_cfg.json   # PC_Common + module list
├── components_version.py
├── compatibility.json
├── linkage_cfg.json
├── platform_cfg/PC/PC_Common/    # empty MCAL overlay; Can stays ours
└── system_secondary/             # Studio menus / BswM common data
```

Relative `plug_in_path` values assume this pack lives at `vcos/mcal_pc/configurator_plug_in`, next to `vcos/components`.
