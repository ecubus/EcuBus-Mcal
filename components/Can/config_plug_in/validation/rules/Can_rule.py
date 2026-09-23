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
"""PC MCAL Can validation.

Semantic continuity of CanControllerId / CanObjectId stays in Can_semantic_rule.json.
Rules here cover the PC RPC driver and references the generator cannot invent.
"""
from func_module.validation.common.basic_rule import BasicRule
from func_module.validation.common.validation_def import ValidationData

_SKIP_KEYS = ("DefinitionRef", "Value", "ValueRef", "ValueIRef", "reference_item_path", "Locked")


def _walk(node, path, found):
    if not isinstance(node, dict):
        return
    definition = str(node.get("DefinitionRef") or "")
    short_name = definition.rsplit("/", 1)[-1] if definition else ""
    if short_name:
        found.setdefault(short_name, []).append((path, node))
    for key, value in node.items():
        if key in _SKIP_KEYS or not isinstance(value, dict) or "DefinitionRef" not in value:
            continue
        _walk(value, f"{path}/{key}", found)


def _value(container, name):
    item = container.get(name)
    if isinstance(item, dict) and "Value" in item:
        return item.get("Value")
    return None


def _value_ref(container, name):
    item = container.get(name)
    if not isinstance(item, dict):
        return None
    return item.get("ValueRef")


def _ref_is_empty(value_ref):
    if value_ref is None:
        return True
    if isinstance(value_ref, list):
        return not any(str(item).strip() for item in value_ref if item is not None)
    return not str(value_ref).strip()


def _ref_tail(value_ref):
    if isinstance(value_ref, list):
        value_ref = next((item for item in value_ref if item), "")
    return str(value_ref or "").rstrip("/").rsplit("/", 1)[-1]


def _as_int(value):
    if value in (None, ""):
        return None
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def _as_bool(value):
    return value in (True, 1, "1", "true", "TRUE", "True")


def _children(container, short_name):
    children = []
    for key, value in container.items():
        if not isinstance(value, dict):
            continue
        definition = str(value.get("DefinitionRef") or "")
        if definition.endswith("/" + short_name):
            children.append(key)
    return children


class CanRule(BasicRule):

    def __init__(self) -> None:
        self.logs = {}
        self.validation_logs_dict = {}

    def validate(self, validation_data: ValidationData):
        containers = self._load(validation_data)
        if containers is None:
            return self.logs
        self._check_ttcan_ref(containers)
        self._check_rpc(containers)
        self._check_controllers(containers)
        self._check_hohs(containers)
        return self.logs

    def _load(self, validation_data: ValidationData):
        bsw_ecuc = getattr(validation_data, "bsw_ecuc_dict", None) or {}
        try:
            can_ecuc = bsw_ecuc["Can"]["ArPackages"]["ActiveEcuC"]["Can"]
        except (KeyError, TypeError):
            self.vlog("CAN016003", "Can module is configured.", "Can module is not configured.", ["/ActiveEcuC/Can"], "pass")
            return None
        found = {}
        _walk(can_ecuc, "/ActiveEcuC/Can", found)
        return found

    def _check_ttcan_ref(self, containers):
        rule = "CAN016003"
        summary = "CanSupportTTCANRef must have a target when it is configured."
        params = containers.get("CanSupportTTCANRef", [])
        path = "/ActiveEcuC/Can/CanGeneral/CanSupportTTCANRef"
        if not params:
            self.vlog(rule, summary, "CanSupportTTCANRef is optional and is not configured.", [path], "pass")
            return
        failed = False
        for param_path, param in params:
            if _ref_is_empty(param.get("ValueRef")):
                failed = True
                self.vlog(
                    rule,
                    summary,
                    "CanSupportTTCANRef is configured but has no target. "
                    "Set it to CanIfPrivateCfg, or remove the reference when TTCAN is not used.",
                    [param_path],
                    "fail",
                )
        if not failed:
            self.vlog(rule, summary, "CanSupportTTCANRef has a target.", [path], "pass")

    def _check_rpc(self, containers):
        rule = "CAN016004"
        summary = "CanPcRpc host and port must be usable."
        rpcs = containers.get("CanPcRpc", [])
        if not rpcs:
            self.vlog(
                rule,
                summary,
                "CanGeneral/CanPcRpc is required.",
                ["/ActiveEcuC/Can/CanGeneral/CanPcRpc"],
                "fail",
            )
            return
        failed = False
        for path, rpc in rpcs:
            host = _value(rpc, "CanRpcHost")
            socket_path = _value(rpc, "CanRpcSocketPath")
            port = _as_int(_value(rpc, "CanRpcPort"))
            if not str(socket_path or "").strip() and not str(host or "").strip():
                failed = True
                self.vlog(rule, summary, "CanRpcHost is empty and CanRpcSocketPath is not set.", [f"{path}/CanRpcHost"], "fail")
            if port is None or port < 1 or port > 65535:
                failed = True
                self.vlog(rule, summary, "CanRpcPort must be in 1..65535.", [f"{path}/CanRpcPort"], "fail")
        if not failed:
            self.vlog(rule, summary, "CanPcRpc host and port are set.", [rpcs[0][0]], "pass")

    def _check_controllers(self, containers):
        controllers = containers.get("CanController", [])
        self._check_handles(controllers)
        self._check_vendor(controllers)
        self._check_default_baudrate(controllers)
        self._check_canfd(controllers)

    def _check_handles(self, controllers):
        rule = "CAN016005"
        summary = "CanRpcHandle must be unique and in 0..63."
        seen = {}
        failed = False
        for path, ctrl in controllers:
            handle = _as_int(_value(ctrl, "CanRpcHandle"))
            param_path = f"{path}/CanRpcHandle"
            if handle is None or handle < 0 or handle > 63:
                failed = True
                self.vlog(rule, summary, "CanRpcHandle must be an integer in 0..63.", [param_path], "fail")
                continue
            if handle in seen:
                failed = True
                self.vlog(
                    rule,
                    summary,
                    f"CanRpcHandle {handle} is already used by {seen[handle]}.",
                    [param_path],
                    "fail",
                )
            else:
                seen[handle] = path
        if controllers and not failed:
            self.vlog(rule, summary, "CanRpcHandle values are unique and in range.", ["/ActiveEcuC/Can/CanConfigSet"], "pass")

    def _check_vendor(self, controllers):
        rule = "CAN016006"
        summary = "CanRpcVendor must be simulate."
        failed = False
        for path, ctrl in controllers:
            vendor = _value(ctrl, "CanRpcVendor")
            if vendor is None:
                continue
            if str(vendor) != "simulate":
                failed = True
                self.vlog(
                    rule,
                    summary,
                    "JSON-RPC only supports vendor simulate.",
                    [f"{path}/CanRpcVendor"],
                    "fail",
                )
        if controllers and not failed:
            self.vlog(rule, summary, "CanRpcVendor is simulate.", ["/ActiveEcuC/Can/CanConfigSet"], "pass")

    def _check_default_baudrate(self, controllers):
        rule = "CAN016007"
        summary = "CanControllerDefaultBaudrate must point at a baudrate of the same controller."
        failed = False
        for path, ctrl in controllers:
            ref = _value_ref(ctrl, "CanControllerDefaultBaudrate")
            baud_names = set(_children(ctrl, "CanControllerBaudrateConfig"))
            param_path = f"{path}/CanControllerDefaultBaudrate"
            if _ref_is_empty(ref) or _ref_tail(ref) not in baud_names:
                failed = True
                self.vlog(
                    rule,
                    summary,
                    "Set CanControllerDefaultBaudrate to a CanControllerBaudrateConfig under this controller.",
                    [param_path],
                    "fail",
                )
        if controllers and not failed:
            self.vlog(rule, summary, "Each controller has a default baudrate.", ["/ActiveEcuC/Can/CanConfigSet"], "pass")

    def _check_canfd(self, controllers):
        rule = "CAN016008"
        summary = "CanRpcCanFd requires a CAN FD baudrate."
        failed = False
        for path, ctrl in controllers:
            if not _as_bool(_value(ctrl, "CanRpcCanFd")):
                continue
            has_fd_baud = any(
                _children(ctrl.get(name), "CanControllerFdBaudrateConfig")
                for name in _children(ctrl, "CanControllerBaudrateConfig")
                if isinstance(ctrl.get(name), dict)
            )
            if not has_fd_baud:
                failed = True
                self.vlog(
                    rule,
                    summary,
                    "CanRpcCanFd is true but this controller has no CanControllerFdBaudrateConfig.",
                    [f"{path}/CanRpcCanFd"],
                    "fail",
                )
        if controllers and not failed:
            self.vlog(rule, summary, "CAN FD controllers have an FD baudrate.", ["/ActiveEcuC/Can/CanConfigSet"], "pass")

    def _check_hohs(self, containers):
        rule = "CAN016009"
        summary = "CanControllerRef must point at a configured CanController."
        controller_names = {path.rsplit("/", 1)[-1] for path, _ in containers.get("CanController", [])}
        hohs = containers.get("CanHardwareObject", [])
        failed = False
        for path, hoh in hohs:
            ref = _value_ref(hoh, "CanControllerRef")
            param_path = f"{path}/CanControllerRef"
            if _ref_is_empty(ref) or _ref_tail(ref) not in controller_names:
                failed = True
                self.vlog(
                    rule,
                    summary,
                    "CanControllerRef is missing or does not name a CanController in this CanConfigSet.",
                    [param_path],
                    "fail",
                )
        if hohs and not failed:
            self.vlog(rule, summary, "Each hardware object references a controller.", ["/ActiveEcuC/Can/CanConfigSet"], "pass")
