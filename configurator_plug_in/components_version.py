# This Python file uses the following encoding: utf-8
############################################################################
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
############################################################################
import os
import json

CUR_PATH = os.path.dirname(__file__)
json_file_path = os.path.join(CUR_PATH, "compatibility.json")


class ComponentsVersionInfo:

    def __init__(self, json_file=json_file_path):
        self.json_file = json_file
        with open(self.json_file, "r", encoding="utf-8") as f:
            self.data = json.load(f)

    def get_current_version(self):
        """获取当前版本信息"""
        return self.data["components"]["version"]

    def update_current_version(self, new_version):
        """修改当前版本信息"""
        self.data["components"]["version"] = new_version
        self.update_compatible_project_system_versions([new_version])
        self._save_changes()

    def get_compatible_project_system_versions(self):
        """获取兼容版本信息"""
        return self.data["components"]["compatibleProjectVersions"]

    def update_compatible_project_system_versions(self, new_versions: list):
        """更新支持的工程版本信息"""
        current_compatible_versions = set(self.get_compatible_project_system_versions())
        new_versions_list = []
        for new_version in new_versions:
            new_versions_list.extend([_replace_after_second_dot(v) for v in [new_version]])
        new_versions = set(new_versions_list)

        # 判断传入的版本是否已存在于兼容列表中
        versions_to_add = new_versions - current_compatible_versions
        if versions_to_add:
            current_compatible_versions.update(versions_to_add)
        sorted_versions = sorted(list(current_compatible_versions), reverse=True)
        self.data["components"]["compatibleProjectVersions"] = sorted_versions
        self._save_changes()

    def _save_changes(self):
        """保存修改到 JSON 文件"""
        with open(self.json_file, "w", encoding="utf-8") as f:
            json.dump(self.data, f, indent=4)


def _replace_after_second_dot(input_str):
    try:
        # 找到第一个点号的位置
        first_dot_index = input_str.find(".")
        if first_dot_index == -1:
            print("输入的字符串不是有效的版本号，应该符合 x.x.x 格式")

        # 找到第二个点号的位置
        second_dot_index = input_str.find(".", first_dot_index + 1)
        if second_dot_index == -1:
            print("输入的字符串不是有效的版本号，应该符合 x.x.x")

        # 构造新的字符串
        new_str = input_str[: second_dot_index + 1] + "x"
        return new_str

    except ValueError as e:
        raise Exception(f"错误: {e}")


version_manager = ComponentsVersionInfo()
