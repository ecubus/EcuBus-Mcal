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
"""PC MCAL Can jinja2 functions."""
from jinja2 import pass_context
from func_module.generator.common.common import FunctionBase


class Function(FunctionBase):
    """Static jinja2 functions."""

    @staticmethod
    @pass_context
    def get_can_cfg(context):
        return context["can_cfg"]

    @staticmethod
    @pass_context
    def get_rpc_cfg(context):
        return context["can_cfg"]["rpc"]

    @staticmethod
    @pass_context
    def get_controller_list(context):
        return context["can_cfg"]["controllers"]

    @staticmethod
    @pass_context
    def get_hoh_list(context):
        return context["can_cfg"]["hohs"]
