/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Ma Chao    machao@kylinos.cn
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <gio/gio.h>
#include <QFile>

#include <sys/syslog.h>

#include "custom.h"

static int file_set_attribute(const char *file_path, const char* attribute, const char* value);

int file_set_custom_icon(const char *file_path, const char *icon_path)
{
    char normal_icon_path[1024] = {0};
    if (!file_path || !icon_path) {
        return -1;
    }

    int pro_os = 0;
    QFile lsb_release("/etc/lsb-release");
    if (lsb_release.open(QIODevice::ReadOnly)) {
        QByteArray data = lsb_release.readAll();
        if (data.contains("V10 Professional") || data.contains("V10 SP1") || data.contains("V10 SP2")) {
            pro_os = 1;
        }
        lsb_release.close();
    }

    if (pro_os > 0) {
        // For V10 Pro
        snprintf(normal_icon_path, sizeof(normal_icon_path), "%s", icon_path);
    } else {
        // For V10
        if (strncmp(icon_path, "file://", strlen("file://")) == 0) {
            snprintf(normal_icon_path, sizeof(normal_icon_path), "%s", icon_path);
        } else {
            snprintf(normal_icon_path, sizeof(normal_icon_path), "file://%s", icon_path);
        }
    }

    return file_set_attribute(file_path, "metadata::custom-icon", normal_icon_path);
}


int file_set_attribute(const char *file_path, const char *attribute, const char *value)
{
    GFile* f = NULL;
    GError* error = NULL;
    int ret = 0;

    f = g_file_new_for_path(file_path);
    if (!g_file_set_attribute_string(f, attribute, value, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &error)) {
        fprintf(stderr, "error code: %d (%s)\n", error->code, error->message);
        syslog(LOG_WARNING, "Failed to set attribute string for file %s: %s.", file_path, error->message);
        g_error_free(error);
        ret = -1;
    }

    g_object_unref(f);

    return ret;
}

//int main(int argc, char** argv)
//{
//    GFile * f;
//    char* path = argv[1];
//    char* attr = argv[2];
//    char* val = argv[3];
//    GError* err = NULL;

//    f = g_file_new_for_path(path);
//    if (!g_file_set_attribute_string(f, attr, val, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &err)) {
//        fprintf(stderr, "%s", err->message);
//        g_error_free(err);

//    }

//    g_object_unref(f);
//    printf("Hello World!\n");
//    return 0;
//}
