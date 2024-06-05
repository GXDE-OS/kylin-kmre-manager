/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Ma Chao    machao@kylinos.cn
 *  Alan Xie   xiehuijun@kylinos.cn
 *  Clom       huangcailong@kylinos.cn
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <err.h>

#include <fcntl.h>
#include <sys/syslog.h>

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>

#include <xcb/xcb.h>
#include <xcb/dri3.h>
#include <xf86drm.h>

#include <errno.h>

#include <unistd.h>

#include "gpuinfo.h"

#include <QOpenGLContext>

#include <set>
#include <string>
#include <memory>

#include <GLES2/gl2.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

static int gpuVendorId = -1;
static int gpuChipId = -1;

static int drmPrimaryNodeMajor = -1;
static int drmPrimaryNodeMinor = -1;
static int drmRenderNodeMajor = -1;
static int drmRenderNodeMinor = -1;

bool GpuInfo::mGpuCheckCompleted = false;
GpuVendor GpuInfo::mGpuVendor = UNKNOWN_VGA;

static char *drm_construct_id_path_tag(drmDevicePtr device)
{
   char *tag = NULL;

   if (device->bustype == DRM_BUS_PCI) {
      if (asprintf(&tag, "pci-%04x_%02x_%02x_%1u",
                   device->businfo.pci->domain,
                   device->businfo.pci->bus,
                   device->businfo.pci->dev,
                   device->businfo.pci->func) < 0) {
         return NULL;
      }
   } else if (device->bustype == DRM_BUS_PLATFORM ||
              device->bustype == DRM_BUS_HOST1X) {
      char *fullname, *name, *address;

      if (device->bustype == DRM_BUS_PLATFORM)
         fullname = device->businfo.platform->fullname;
      else
         fullname = device->businfo.host1x->fullname;

      name = strrchr(fullname, '/');
      if (!name)
         name = strdup(fullname);
      else
         name = strdup(name + 1);

      address = strchr(name, '@');
      if (address) {
         *address++ = '\0';

         if (asprintf(&tag, "platform-%s_%s", address, name) < 0)
            tag = NULL;
      } else {
         if (asprintf(&tag, "platform-%s", name) < 0)
            tag = NULL;
      }

      free(name);
   }
   return tag;
}

static char *drm_get_id_path_tag_for_fd(int fd)
{
   drmDevicePtr device;
   char *tag;

   if (drmGetDevice2(fd, 0, &device) != 0)
       return NULL;

   tag = drm_construct_id_path_tag(device);
   drmFreeDevice(&device);
   return tag;
}

static bool drm_device_matches_tag(drmDevicePtr device, const char *prime_tag)
{
   char *tag = drm_construct_id_path_tag(device);
   int ret;

   if (tag == NULL)
      return false;

   ret = strcmp(tag, prime_tag);

   free(tag);
   return ret == 0;
}

static bool drm_get_pci_id_for_fd(int fd, int *vendor_id, int *chip_id)
{
   drmDevicePtr device;
   bool ret;

   if (drmGetDevice2(fd, 0, &device) == 0) {
      if (device->bustype == DRM_BUS_PCI) {
         *vendor_id = device->deviceinfo.pci->vendor_id;
         *chip_id = device->deviceinfo.pci->device_id;
         ret = true;
      }
      else {
         fprintf(stderr, "device is not located on the PCI bus\n");
         ret = false;
      }
      drmFreeDevice(&device);
   }
   else {
      fprintf(stderr, "failed to retrieve device information\n");
      ret = false;
   }

   return ret;
}

static int open_device(const char *device_name)
{
   int fd;
   fd = open(device_name, O_RDWR | O_CLOEXEC);
   if (fd == -1 && errno == EINVAL)
   {
      fd = open(device_name, O_RDWR);
      if (fd != -1)
         fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
   }
   return fd;
}



static int get_user_preferred_fd(int default_fd)
{
/* Arbitrary "maximum" value of drm devices. */
#define MAX_DRM_DEVICES 32
    const char *dri_prime = getenv("DRI_PRIME");
    char *default_tag, *prime = NULL;
    drmDevicePtr devices[MAX_DRM_DEVICES];
    int i, num_devices, fd;
    bool found = false;

    if (dri_prime)
        prime = strdup(dri_prime);

    if (prime == NULL) {
        return default_fd;
    }

    default_tag = drm_get_id_path_tag_for_fd(default_fd);
    if (default_tag == NULL)
        goto err;

    num_devices = drmGetDevices2(0, devices, MAX_DRM_DEVICES);
    if (num_devices < 0)
        goto err;

    /* two format are supported:
     * "1": choose any other card than the card used by default.
     * id_path_tag: (for example "pci-0000_02_00_0") choose the card
     * with this id_path_tag.
     */
    if (!strcmp(prime,"1")) {
        /* Hmm... detection for 2-7 seems to be broken. Oh well ...
         * Pick the first render device that is not our own.
         */
        for (i = 0; i < num_devices; i++) {
            if (devices[i]->available_nodes & 1 << DRM_NODE_RENDER &&
                    !drm_device_matches_tag(devices[i], default_tag)) {
                found = true;
                break;
            }
        }
    } else {
        for (i = 0; i < num_devices; i++) {
            if (devices[i]->available_nodes & 1 << DRM_NODE_RENDER &&
                    drm_device_matches_tag(devices[i], prime)) {
                found = true;
                break;
            }
        }
    }

    if (!found) {
        drmFreeDevices(devices, num_devices);
        goto err;
    }

    fd = open_device(devices[i]->nodes[DRM_NODE_RENDER]);
    drmFreeDevices(devices, num_devices);
    if (fd < 0)
        goto err;

    close(default_fd);


    free(default_tag);
    free(prime);
    return fd;

err:

    free(default_tag);
    free(prime);
    return default_fd;
}

static int dri3_open(xcb_connection_t *conn,
                     xcb_window_t root,
                     uint32_t provider)
{
   xcb_dri3_open_cookie_t       cookie;
   xcb_dri3_open_reply_t        *reply;
   int                          fd;

   cookie = xcb_dri3_open(conn,
                          root,
                          provider);

   reply = xcb_dri3_open_reply(conn, cookie, NULL);
   if (!reply)
      return -1;

   if (reply->nfd != 1) {
      free(reply);
      return -1;
   }

   fd = xcb_dri3_open_reply_fds(conn, reply)[0];
   free(reply);
   fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);

   return fd;
}

static int xcb_open(xcb_connection_t **connection, xcb_screen_t **screen)
{
    int screen_number;
    const xcb_setup_t *setup;
    xcb_connection_t *c;
    xcb_screen_t *scrn;
    xcb_screen_iterator_t screen_iter;

    /* getting the connection */
    c = xcb_connect(NULL, &screen_number);
    if (xcb_connection_has_error(c)) {
        return -1;
    }

    /* getting the current screen */
    setup = xcb_get_setup(c);

    scrn = NULL;
    screen_iter = xcb_setup_roots_iterator(setup);
    for (; screen_iter.rem != 0; --screen_number, xcb_screen_next(&screen_iter))
    {
        if (screen_number == 0) {
            scrn = screen_iter.data;
            break;
        }
    }
    if (!scrn) {
        xcb_disconnect(c);
        return -1;
    }

    *connection = c;
    *screen = scrn;
    return 0;
}

static bool get_gpu_pci_info_xcb(int* vendor_id, int* chip_id)
{
    int ret = 0;
    int fd = -1;
    xcb_connection_t* connection = NULL;
    xcb_screen_t* screen = NULL;
    bool result = false;

    ret = xcb_open(&connection, &screen);
    if (ret < 0)
        return false;

    fd = dri3_open(connection, screen->root, 0);
    if (fd < 0) {
        return false;
    }

    fd = get_user_preferred_fd(fd);

    result = drm_get_pci_id_for_fd(fd, vendor_id, chip_id);


    close(fd);
    xcb_disconnect(connection);

    return result;
}

static bool get_gpu_pci_info_wayland(int* vendor_id, int* chip_id)
{
    (void) vendor_id;
    (void) chip_id;

    // TODO get device info via wayland

    return false;
}

static bool get_gpu_pci_info(int* vendor_id, int* chip_id)
{
    if (get_gpu_pci_info_xcb(vendor_id, chip_id)) {
        return true;
    }

    return get_gpu_pci_info_wayland(vendor_id, chip_id);
}

static GpuVendor get_gpu_vendor(int vendor_id)
{
    if (vendor_id == 0x1002) {
        return AMD_VGA;
    } else if (vendor_id == 0x10de) {
        return NVIDIA_VGA;
    } else if (vendor_id == 0x8086) {
        return INTEL_VGA;
    }
    
    return UNKNOWN_VGA;
}

GpuVendor GpuInfo::getGpuVendorFromDrm()
{
    if (mGpuCheckCompleted) {
        return mGpuVendor;
    }

    bool result = false;

    if (gpuVendorId != -1 && gpuChipId != -1) {
        mGpuVendor = get_gpu_vendor(gpuVendorId);
        mGpuCheckCompleted = true;
        return mGpuVendor;
    }

    result = get_gpu_pci_info(&gpuVendorId, &gpuChipId);
    if (result) {
        mGpuVendor = get_gpu_vendor(gpuVendorId);
    }
    mGpuCheckCompleted = true;
    return mGpuVendor;
}

static const int i915_chip_ids[] = {
#define CHIPSET(chip, desc, name) chip,
#include "pci_ids/i915_pci_ids.h"
#undef CHIPSET
};

static const int i965_chip_ids[] = {
#define CHIPSET(chip, family, family_str, name) chip,
#include "pci_ids/i965_pci_ids.h"
#undef CHIPSET
};

static const int iris_chip_ids[] = {
#define CHIPSET(chip, family, family_str, name) chip,
#include "pci_ids/iris_pci_ids.h"
#undef CHIPSET
};

static const int r300_chip_ids[] = {
#define CHIPSET(chip, name, family) chip,
#include "pci_ids/r300_pci_ids.h"
#undef CHIPSET
};

static const int r600_chip_ids[] = {
#define CHIPSET(chip, name, family) chip,
#include "pci_ids/r600_pci_ids.h"
#undef CHIPSET
};

static const int radeonsi_chip_ids[] = {
#define CHIPSET(chip, family) chip,
#include "pci_ids/radeonsi_pci_ids.h"
#undef CHIPSET
};

static const int amdgpu_chip_ids[] = {
#define CHIPSET(chip, family) chip,
#include "pci_ids/amdgpu_pci_ids.h"
#undef CHIPSET
};

static bool chip_id_in_prefer_i965_list(int chip_id)
{
    size_t i = 0;

    for (i = 0; i < sizeof(i965_chip_ids) / sizeof(int); i++) {
        if (chip_id == i965_chip_ids[i])
            return true;
    }

    return false;
}

bool GpuInfo::preferI965()
{
    if (gpuVendorId == -1 || gpuChipId == -1) {
        get_gpu_pci_info(&gpuVendorId, &gpuChipId);
    }

    if (gpuVendorId == -1 || gpuChipId == -1) {
        return false;
    }

    if (get_gpu_vendor(gpuVendorId) != INTEL_VGA) {
        return false;
    }

    return chip_id_in_prefer_i965_list(gpuChipId);
}

static bool chip_id_in_list(int chip_id, const int* list, int list_size)
{
    int i = 0;

    for (i = 0; i < list_size; i++) {
        if (chip_id == list[i]) {
            return true;
        }
    }

    return false;
}

bool GpuInfo::isIntelIrisGpu()
{
    int gpu_vendor_id = -1;
    int gpu_chip_id = -1;

    get_gpu_pci_info(&gpu_vendor_id, &gpu_chip_id);
    if (get_gpu_vendor(gpu_vendor_id) == INTEL_VGA) {
        return chip_id_in_list(gpu_chip_id, iris_chip_ids, sizeof(iris_chip_ids) / sizeof(int));
    }

    return false;
}

static bool check_chip_id_for_intel_gpu(int chip_id)
{
    if (chip_id_in_list(chip_id, i915_chip_ids, sizeof(i915_chip_ids) / sizeof(int))) {
        return true;
    }

    if (chip_id_in_list(chip_id, i965_chip_ids, sizeof(i965_chip_ids) / sizeof(int))) {
        return true;
    }

    //Do not enable iris for the moment.
    if (chip_id_in_list(chip_id, iris_chip_ids, sizeof(iris_chip_ids) / sizeof(int))) {
        return true;
    }

    return false;
}

static bool check_chip_id_for_amd_gpu(int chip_id)
{
    /* Do not enable r300 for the moment. These GPUs are too old.
    if (chip_id_in_list(chip_id, r300_chip_ids, sizeof(r300_chip_ids) / sizeof(int))) {
        return true;
    }
    */

    if (chip_id_in_list(chip_id, r600_chip_ids, sizeof(r600_chip_ids) / sizeof(int))) {
        return true;
    }

    if (chip_id_in_list(chip_id, radeonsi_chip_ids, sizeof(radeonsi_chip_ids) / sizeof(int))) {
        return true;
    }

    if (chip_id_in_list(chip_id, amdgpu_chip_ids, sizeof(amdgpu_chip_ids) / sizeof(int))) {
        return true;
    }

    return false;
}

bool GpuInfo::couldUsesDrmMode()
{
    if (gpuVendorId == -1 || gpuChipId == -1) {
        get_gpu_pci_info(&gpuVendorId, &gpuChipId);
    }

    if (gpuVendorId == -1 || gpuChipId == -1) {
        return false;
    }

    if (get_gpu_vendor(gpuVendorId) == INTEL_VGA) {
        return check_chip_id_for_intel_gpu(gpuChipId);
    } else if (get_gpu_vendor(gpuVendorId) == AMD_VGA) {
        return check_chip_id_for_amd_gpu(gpuChipId);
    }

    return false;
}

#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(__amd64__)
bool GpuInfo::couldUseIntelGralloc()
{
    if (gpuVendorId == -1 || gpuChipId == -1) {
        get_gpu_pci_info(&gpuVendorId, &gpuChipId);
    }

    if (gpuVendorId == -1 || gpuChipId == -1) {
        return false;
    }

    return false;
}
#endif

static int get_device_node_fd_from_dri3()
{
    int ret = 0;
    int fd = -1;
    xcb_connection_t* connection = NULL;
    xcb_screen_t* screen = NULL;

    ret = xcb_open(&connection, &screen);
    if (ret < 0)
        return -1;

    fd = dri3_open(connection, screen->root, 0);
    if (fd < 0) {
        return -1;
    }

    fd = get_user_preferred_fd(fd);

    if (connection) {
        xcb_disconnect(connection);
    }

    return fd;
}

static bool get_drm_device_node_number()
{
    int fd = -1;
    bool result = false;
    char* id_path_tag = NULL;
    drmDevicePtr devices[MAX_DRM_DEVICES];
    int i = 0;
    int num_devices = -1;
    struct stat sb;

    if (drmPrimaryNodeMajor >= 0 &&
        drmPrimaryNodeMinor >= 0 &&
        drmRenderNodeMajor >= 0 &&
        drmRenderNodeMinor >= 0) {
        return true;
    }

    fd = get_device_node_fd_from_dri3();
    if (fd < 0) {
        return false;
    }

    id_path_tag = drm_get_id_path_tag_for_fd(fd);
    if (!id_path_tag) {
        result = false;
        goto out;
    }

    num_devices = drmGetDevices2(0, devices, MAX_DRM_DEVICES);
    if (num_devices < 0) {
        goto out;
    }

    for (i = 0; i < num_devices; i++) {
        if (devices[i]->available_nodes & 1 << DRM_NODE_PRIMARY &&
                drm_device_matches_tag(devices[i], id_path_tag)) {
            if (stat(devices[i]->nodes[DRM_NODE_PRIMARY], &sb) == 0) {
                drmPrimaryNodeMajor = major(sb.st_rdev);
                drmPrimaryNodeMinor = minor(sb.st_rdev);
            }
            break;
        }
    }

    for (i = 0; i < num_devices; i++) {
        if (devices[i]->available_nodes & 1 << DRM_NODE_RENDER &&
                drm_device_matches_tag(devices[i], id_path_tag)) {
            if (stat(devices[i]->nodes[DRM_NODE_RENDER], &sb) == 0) {
                drmRenderNodeMajor = major(sb.st_rdev);
                drmRenderNodeMinor = minor(sb.st_rdev);
            }
            break;
        }
    }

    drmFreeDevices(devices, num_devices);

    if (drmPrimaryNodeMajor >= 0 &&
        drmPrimaryNodeMinor >= 0 &&
        drmRenderNodeMajor >= 0 &&
        drmRenderNodeMinor >= 0) {
        result = true;
    }

out:

    if (id_path_tag) {
        free(id_path_tag);
    }

    close(fd);

    return result;
}

bool GpuInfo::getDrmDeviceNodeNumber(int* primaryMajor, int* primaryMinor,
                                     int* renderMajor, int* renderMinor)
{
    if (!primaryMajor || !primaryMinor || !renderMajor || !renderMinor) {
        return false;
    }

    *primaryMajor = -1;
    *primaryMinor = -1;
    *renderMajor = -1;
    *renderMinor = -1;

    if (!get_drm_device_node_number()) {
        return false;
    }

    if (drmPrimaryNodeMajor == -1 ||
        drmPrimaryNodeMinor == -1 ||
        drmRenderNodeMajor == -1 ||
        drmRenderNodeMinor == -1) {
        return false;
    }

    *primaryMajor = drmPrimaryNodeMajor;
    *primaryMinor = drmPrimaryNodeMinor;
    *renderMajor = drmRenderNodeMajor;
    *renderMinor = drmRenderNodeMinor;

    return true;
}

bool GpuInfo::supportsThreadedOpenGL()
{
    return QOpenGLContext::supportsThreadedOpenGL();
}

static bool checkOpenGLESVersion(int* major, int* minor)
{
    int glesMajor = 0;
    int glesMinor = 0;

    const char* version = (const char*) glGetString(GL_VERSION);
    if (glGetError() != GL_NO_ERROR) {
        return false;
    }

    if (sscanf(version, "OpenGL ES-CM %d.%d", &glesMajor, &glesMinor) != 2) {
        if (sscanf(version, "OpenGL ES %d.%d", &glesMajor, &glesMinor) != 2) {
            return false;
        }
    }

    if (glesMajor > 0 && glesMinor >= 0) {
        if (major) {
            *major = glesMajor;
        }
        if (minor) {
            *minor = glesMinor;
        }
    } else {
        return false;
    }

    return true;
}

static bool checkOpenGLES31AEP()
{
    bool result = false;
    EGLint eglMajor = 0;
    EGLint eglMinor = 0;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLConfig config = EGL_NO_CONFIG_KHR;
    EGLContext context = EGL_NO_CONTEXT;
    EGLint numConfigs = 0;
    int glesMajor = 0;
    int glesMinor = 0;
    std::set<std::string> extensions;

    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE,
    };

    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE,
    };

    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        fprintf(stderr, "Failed to get egl display.\n");
        goto out;
    }

    if (!eglInitialize(display, &eglMajor, &eglMinor)) {
        fprintf(stderr, "Failed to initialize egl.\n");
        goto out;
    }

    if (!eglChooseConfig(display, attribs, &config, 1, &numConfigs)) {
        fprintf(stderr, "Failed to choose egl config.\n");
        goto out;
    }

    if (config == EGL_NO_CONFIG_KHR || numConfigs == 0) {
        fprintf(stderr, "No suitable egl config.\n");
        goto out;
    }

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
    if (context == EGL_NO_CONTEXT) {
        fprintf(stderr, "Failed to create egl context.\n");
        goto out;
    }

    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);

    if (!checkOpenGLESVersion(&glesMajor, &glesMinor)) {
        goto out;
    }

    if (glesMajor >= 3 && glesMinor >= 1) {
        for (unsigned int i = 0; i < 1000; i++) {
            const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
            GLenum err = glGetError();
            if (err != GL_NO_ERROR || ext == nullptr) {
                break;
            }
            extensions.insert(std::string(ext));
        }
    }

    if (extensions.find("GL_ANDROID_extension_pack_es31a") != extensions.end()) {
        result = true;
        goto out;
    }

    if (extensions.find("GL_KHR_debug") != extensions.end() &&
            extensions.find("GL_KHR_texture_compression_astc_ldr") != extensions.end() &&
            extensions.find("GL_KHR_blend_equation_advanced") != extensions.end() &&
            extensions.find("GL_OES_sample_shading") != extensions.end() &&
            extensions.find("GL_OES_sample_variables") != extensions.end() &&
            extensions.find("GL_OES_shader_image_atomic") != extensions.end() &&
            extensions.find("GL_OES_shader_multisample_interpolation") != extensions.end() &&
            extensions.find("GL_OES_texture_stencil8") != extensions.end() &&
            extensions.find("GL_OES_texture_storage_multisample_2d_array") != extensions.end() &&
            extensions.find("GL_EXT_copy_image") != extensions.end() &&
            extensions.find("GL_EXT_draw_buffers_indexed") != extensions.end() &&
            extensions.find("GL_EXT_geometry_shader") != extensions.end() &&
            extensions.find("GL_EXT_gpu_shader5") != extensions.end() &&
            extensions.find("GL_EXT_primitive_bounding_box") != extensions.end() &&
            extensions.find("GL_EXT_shader_io_blocks") != extensions.end() &&
            extensions.find("GL_EXT_tessellation_shader") != extensions.end() &&
            extensions.find("GL_EXT_texture_border_clamp") != extensions.end() &&
            extensions.find("GL_EXT_texture_buffer") != extensions.end() &&
            extensions.find("GL_EXT_texture_cube_map_array") != extensions.end() &&
            extensions.find("GL_EXT_texture_sRGB_decode") != extensions.end()) {
        result = true;
        goto out;
    }

out:
    if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context != EGL_NO_CONTEXT) {
            eglDestroyContext(display, context);
        }
        eglTerminate(display);
    }

    return result;
}

static bool s_supportsOpenGLES31AEP = checkOpenGLES31AEP();

bool GpuInfo::supportsOpenGLES31AEP()
{
    return s_supportsOpenGLES31AEP;
}
