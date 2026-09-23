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
"""Generate PC MCAL Can configuration sources."""
import os
import inspect

from jinja2 import Environment, FileSystemLoader, StrictUndefined, UndefinedError, Template

from . import context
from . import filter as can_filter
from func_module.health_monitor.logger import logger

CUR_FILE_PATH = os.path.dirname(__file__)
DEFAULT_TEMPLATE_FILE_DIR = os.path.join(CUR_FILE_PATH, "template")


def generate_dynamic_code(sys_config: dict, config_dict: dict):
    logger.debug("Generate Can dynamic code (PC MCAL)")
    input_dict = can_filter.get_template_dynamic_code_dict(config_dict)
    output_dir = os.path.realpath(os.path.join(sys_config["gendata_path"], "Can"))
    os.makedirs(output_dir, exist_ok=True)

    env = Environment(loader=FileSystemLoader(os.path.abspath(DEFAULT_TEMPLATE_FILE_DIR)), undefined=StrictUndefined)
    env.filters.update(context.get_filters())
    env.globals.update(context.get_functions())

    render_ctx = {"can_cfg": input_dict, "sys_cfg": sys_config}
    for template_name in env.list_templates():
        template = env.get_template(template_name)
        if not _render_file(sys_config, template_name, template, render_ctx, output_dir):
            return False
    return True


def _render_file(sys_config, template_name, template: Template, render_ctx, output_dir):
    file_name = template_name[: template_name.find(".jinja")]
    file_path = os.path.join(output_dir, file_name)
    logger.debug(f"Start generating {file_name}")
    try:
        with open(file_path, "w", encoding="utf-8") as out_stream:
            out_stream.write(template.render(render_ctx))
        logger.info(f"Finish generating {file_name}")
        return True
    except UndefinedError as exc:
        error_info = None
        frame_info_list = inspect.trace()
        frame_info_list.reverse()
        for frame_info in frame_info_list:
            if frame_info.function == "top-level template code":
                error_info = frame_info
                break
        if error_info is not None:
            filename = os.path.basename(error_info.filename)
            error_line = error_info.lineno
            error_code = error_info.code_context[0].strip() if error_info.code_context else ""
            logger.error(f"UndefinedError: {filename}:{error_line} {error_code} {exc}")
        else:
            logger.error(f"UndefinedError while rendering {template_name}: {exc}")
        return False
