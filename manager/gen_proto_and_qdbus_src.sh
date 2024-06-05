#! /bin/bash

# Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
#
# Authors:
#  Kobe Lee    lixiang@kylinos.cn
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

set -e

script_path=`dirname $0`

#$script_path/kmre/protobuf/generate_proto_cpp.sh

cd $script_path/dbus/
/usr/bin/qdbusxml2cpp cn.kylinos.Kmre.Manager.xml -i metatypes.h -a kmre_manager
cd -

echo "Generate qbus source finished."

exit 0
