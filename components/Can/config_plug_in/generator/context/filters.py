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
"""PC MCAL Can jinja2 filters."""


class Filter:
    """Static jinja2 filters."""

    @staticmethod
    def c_bool(value):
        if value in (True, 1, "1", "true", "TRUE", "True"):
            return "TRUE"
        return "FALSE"

    @staticmethod
    def c_str(value):
        if value is None:
            return '""'
        text = str(value).replace("\\", "\\\\").replace('"', '\\"')
        return f'"{text}"'
