# This Python file uses the following encoding: utf-8
# ##############################################################################
# Copyright (c) 2025 Li Auto Inc. and its affiliates
# Licensed under the Apache License, Version 2.0(the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ##############################################################################
"""Parse Can ECUC dict into template input for PC MCAL."""

from func_module.health_monitor.logger import logger

OBJECT_TYPE_MAP = {"RECEIVE": "CAN_OBJECT_RECEIVE", "TRANSMIT": "CAN_OBJECT_TRANSMIT"}
HANDLE_TYPE_MAP = {"BASIC": "CAN_HANDLE_BASIC", "FULL": "CAN_HANDLE_FULL"}
ID_TYPE_MAP = {
    "STANDARD": "CAN_ID_STANDARD",
    "EXTENDED": "CAN_ID_EXTENDED",
    "MIXED": "CAN_ID_MIXED",
}


def get_val_from_dict_by_path(data_dict, path, default=None):
    try:
        val = data_dict
        for key in path.split("/"):
            if key == "":
                continue
            val = val[key]
        return val
    except (KeyError, TypeError):
        return default


def get_container_value(data_dict, default=None):
    if not isinstance(data_dict, dict):
        return default
    if "Value" in data_dict:
        return data_dict["Value"]
    return default


def get_container_value_ref(data_dict, default=""):
    if not isinstance(data_dict, dict):
        return default
    ref = data_dict.get("ValueRef")
    if not ref:
        return default
    return str(ref).split("/")[-1]


def get_dict_by_definitionref(data_dict, def_ref, path="/"):
    rv = {}
    val_dict = get_val_from_dict_by_path(data_dict, path, {})
    if not isinstance(val_dict, dict):
        return rv
    for key, val in val_dict.items():
        if not isinstance(val, dict):
            continue
        definition = val.get("DefinitionRef", "")
        if str(definition).split("/")[-1] == def_ref:
            rv[key] = val
    return rv


def _as_bool(value, default=False):
    if value in (None, ""):
        return default
    if value in (True, 1, "1", "true", "TRUE", "True"):
        return True
    if value in (False, 0, "0", "false", "FALSE", "False"):
        return False
    return default


def _as_int(value, default=0):
    if value in (None, ""):
        return default
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _as_str(value, default=""):
    if value in (None,):
        return default
    return str(value)


def _sort_by_id(items, key_name):
    return sorted(items, key=lambda item: _as_int(item.get(key_name), 65535))


def _get_can_root(config_dict):
    bsw = config_dict.get("bsw", {})
    can = bsw.get("can") or bsw.get("Can")
    if not can:
        logger.error("Can ECUC not found in bsw config")
        return {}
    return get_val_from_dict_by_path(can, "ArPackages/ActiveEcuC/Can", {})


def _parse_rpc(can_root):
    general = get_dict_by_definitionref(can_root, "CanGeneral")
    rpc = {}
    for _, general_cfg in general.items():
        rpc_cfgs = get_dict_by_definitionref(general_cfg, "CanPcRpc")
        if rpc_cfgs:
            rpc = next(iter(rpc_cfgs.values()))
            break
        # Some ECUC dumps flatten CanPcRpc under CanGeneral
        if "CanPcRpc" in general_cfg and isinstance(general_cfg["CanPcRpc"], dict):
            rpc = general_cfg["CanPcRpc"]
            break

    return {
        "host": _as_str(get_container_value(rpc.get("CanRpcHost"), "127.0.0.1"), "127.0.0.1"),
        "port": _as_int(get_container_value(rpc.get("CanRpcPort"), 17320), 17320),
        "socket_path": _as_str(get_container_value(rpc.get("CanRpcSocketPath"), "")),
        "auto_reconnect": _as_bool(get_container_value(rpc.get("CanRpcAutoReconnect"), True), True),
        "reconnect_interval_ms": max(
            0, min(60000, _as_int(get_container_value(rpc.get("CanRpcReconnectIntervalMs"), 500), 500))
        ),
    }


def _parse_baudrate(ctrl_cfg):
    baud_cfgs = get_dict_by_definitionref(ctrl_cfg, "CanControllerBaudrateConfig")
    default_name = get_container_value_ref(ctrl_cfg.get("CanControllerDefaultBaudrate", {}))
    selected = None
    if default_name and default_name in baud_cfgs:
        selected = baud_cfgs[default_name]
    elif baud_cfgs:
        selected = next(iter(baud_cfgs.values()))
    if not selected:
        return {"bitrate_bps": 500000, "fd_bitrate_bps": 0, "brs": False, "baud_id": 0}

    kbps = _as_int(get_container_value(selected.get("CanControllerBaudRate"), 500), 500)
    baud_id = _as_int(get_container_value(selected.get("CanControllerBaudRateConfigID"), 0), 0)
    fd_cfgs = get_dict_by_definitionref(selected, "CanControllerFdBaudrateConfig")
    fd_kbps = 0
    brs = False
    if fd_cfgs:
        fd_cfg = next(iter(fd_cfgs.values()))
        fd_kbps = _as_int(get_container_value(fd_cfg.get("CanControllerFdBaudRate"), 0), 0)
        brs = _as_bool(get_container_value(fd_cfg.get("CanControllerTxBitRateSwitch"), False))
    return {
        "bitrate_bps": kbps * 1000,
        "fd_bitrate_bps": fd_kbps * 1000,
        "brs": brs,
        "baud_id": baud_id,
    }


def _parse_controllers(can_root):
    config_sets = get_dict_by_definitionref(can_root, "CanConfigSet")
    controllers = []
    for _, cfg_set in config_sets.items():
        ctrl_cfgs = get_dict_by_definitionref(cfg_set, "CanController")
        for name, ctrl in ctrl_cfgs.items():
            baud = _parse_baudrate(ctrl)
            canfd_cfg = _as_bool(get_container_value(ctrl.get("CanRpcCanFd"), False))
            controllers.append(
                {
                    "name": name,
                    "controller_id": _as_int(get_container_value(ctrl.get("CanControllerId"), 0)),
                    "activation": _as_bool(get_container_value(ctrl.get("CanControllerActivation"), True), True),
                    "vendor": _as_str(get_container_value(ctrl.get("CanRpcVendor"), "simulate"), "simulate"),
                    "handle": _as_int(get_container_value(ctrl.get("CanRpcHandle"), 0)),
                    "canfd": canfd_cfg or (baud["fd_bitrate_bps"] > 0),
                    "hardware_name": _as_str(get_container_value(ctrl.get("HardwareName"), name), name),
                    "rx_processing": _as_str(get_container_value(ctrl.get("CanRxProcessing"), "POLLING"), "POLLING"),
                    "tx_processing": _as_str(get_container_value(ctrl.get("CanTxProcessing"), "POLLING"), "POLLING"),
                    "busoff_processing": _as_str(
                        get_container_value(ctrl.get("CanBusoffProcessing"), "POLLING"), "POLLING"
                    ),
                    **baud,
                }
            )
    return _sort_by_id(controllers, "controller_id")


def _parse_filters(hoh_cfg):
    filters = get_dict_by_definitionref(hoh_cfg, "CanHwFilter")
    parsed = []
    for fname, fcfg in filters.items():
        parsed.append(
            {
                "name": fname,
                "code": _as_int(get_container_value(fcfg.get("CanHwFilterCode"), 0)),
                "mask": _as_int(get_container_value(fcfg.get("CanHwFilterMask"), 0)),
            }
        )
    return parsed


def _parse_hohs(can_root):
    config_sets = get_dict_by_definitionref(can_root, "CanConfigSet")
    hohs = []
    for _, cfg_set in config_sets.items():
        hoh_cfgs = get_dict_by_definitionref(cfg_set, "CanHardwareObject")
        for name, hoh in hoh_cfgs.items():
            obj_type = _as_str(get_container_value(hoh.get("CanObjectType"), "RECEIVE"), "RECEIVE")
            handle_type = _as_str(get_container_value(hoh.get("CanHandleType"), "BASIC"), "BASIC")
            id_type = _as_str(get_container_value(hoh.get("CanIdType"), "STANDARD"), "STANDARD")
            filters = _parse_filters(hoh)
            first = filters[0] if filters else {"code": 0, "mask": 0}
            hohs.append(
                {
                    "name": name,
                    "hoh_id": _as_int(get_container_value(hoh.get("CanObjectId"), 0)),
                    "controller_name": get_container_value_ref(hoh.get("CanControllerRef", {})),
                    "object_type": obj_type,
                    "object_type_enum": OBJECT_TYPE_MAP.get(obj_type, "CAN_OBJECT_RECEIVE"),
                    "handle_type": handle_type,
                    "handle_type_enum": HANDLE_TYPE_MAP.get(handle_type, "CAN_HANDLE_BASIC"),
                    "id_type": id_type,
                    "id_type_enum": ID_TYPE_MAP.get(id_type, "CAN_ID_STANDARD"),
                    "padding": _as_int(get_container_value(hoh.get("CanFdPaddingValue"), 0)),
                    "hw_object_count": _as_int(get_container_value(hoh.get("CanHwObjectCount"), 1), 1),
                    "has_filter": len(filters) > 0,
                    "filter_code": first["code"],
                    "filter_mask": first["mask"],
                    "filters": filters,
                }
            )
    hohs = _sort_by_id(hohs, "hoh_id")
    return hohs


def _bind_controller_ids(controllers, hohs):
    name_to_id = {ctrl["name"]: ctrl["controller_id"] for ctrl in controllers}
    for hoh in hohs:
        hoh["controller_id"] = name_to_id.get(hoh["controller_name"], 0)


def get_template_dynamic_code_dict(config_dict):
    can_root = _get_can_root(config_dict)
    if not can_root:
        return {"rpc": _parse_rpc({}), "controllers": [], "hohs": []}
    controllers = _parse_controllers(can_root)
    hohs = _parse_hohs(can_root)
    _bind_controller_ids(controllers, hohs)
    general = {}
    generals = get_dict_by_definitionref(can_root, "CanGeneral")
    if generals:
        gcfg = next(iter(generals.values()))
        general = {
            "dev_error_detect": _as_bool(get_container_value(gcfg.get("CanDevErrorDetection"), False)),
            "version_info_api": _as_bool(get_container_value(gcfg.get("CanVersionInfoApi"), False)),
            "set_baudrate_api": _as_bool(get_container_value(gcfg.get("CanSetBaudrateApi"), False)),
            "index": _as_int(get_container_value(gcfg.get("CanIndex"), 0)),
        }
    return {
        "rpc": _parse_rpc(can_root),
        "general": general,
        "controllers": controllers,
        "hohs": hohs,
    }
