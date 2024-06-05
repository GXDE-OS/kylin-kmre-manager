// Copyright (C) 2014 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef EMUGL_COMMON_SOCKETS_H
#define EMUGL_COMMON_SOCKETS_H

// A set of helper functions used to deal with sockets in a portable way.

namespace kmre
{
// Bind to a local/Unix path, and return its socket descriptor on success,
// or -errno code on failure.
int socketLocalServer(const char *path, int socketType);

// Connect to a Unix local path, and return a new socket descriptor
// on success, or -errno code on failure.
int socketLocalClient(const char *path, int socketType);

} // namespace kmre

#endif // EMUGL_COMMON_SOCKETS_H
