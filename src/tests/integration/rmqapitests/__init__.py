"""Copyright 2020-2023 Bloomberg Finance L.P.
SPDX-License-Identifier: Apache-2.0

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""
""" 
This integration test module runs various scenarios to ensure the proper behavior of a reliable RabbitMQ client.

Tests require the presence of a RabbitMQ broker.

Upon import tests will run using an executable defined by the environment variable 'RMQ_EXE'.
See fixtures.TestConfig for other evn var arguments.

"""
