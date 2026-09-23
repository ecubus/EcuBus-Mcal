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
"""Optional json loader for Can module."""
import os
import json
from func_module.health_monitor.logger import logger


def load_configuration_file(sys_config: dict) -> dict:
    can_cfg_path = os.path.join(sys_config["config_path"], "Bsw/Can/Can_Cfg.json")
    if os.path.isfile(can_cfg_path) is False:
        logger.error(f"File {can_cfg_path} Not Exist !")
        return None
    with open(can_cfg_path, "r", encoding="utf-8") as handle:
        return json.load(handle)
