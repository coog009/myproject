/*
 *  vaapi.c - VA API common code
 *
 *  hwdecode-demos (C) 2009-2010 Splitted-Desktop Systems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "sysdeps.h"
#include "vaapi.h"
#include "vaapi_compat.h"
#include "utils.h"

#if USE_GLX
#include "glx.h"
#endif

#if USE_VAAPI_GLX
#include <va/va_glx.h>
#endif

#define DEBUG 1
#include "debug.h"

static VAAPIContext *vaapi_context;

static inline const char *string_of_VAImageFormat(VAImageFormat *imgfmt)
{
    return string_of_FOURCC(imgfmt->fourcc);
}

static const char *string_of_VAProfile(VAProfile profile)
{
    switch (profile) {
#define PROFILE(profile) \
        case VAProfile##profile: return "VAProfile" #profile
        PROFILE(MPEG2Simple);
        PROFILE(MPEG2Main);
        PROFILE(MPEG4Simple);
        PROFILE(MPEG4AdvancedSimple);
        PROFILE(MPEG4Main);
        PROFILE(H264Baseline);
        PROFILE(H264Main);
        PROFILE(H264High);
        PROFILE(VC1Simple);
        PROFILE(VC1Main);
        PROFILE(VC1Advanced);
#undef PROFILE
    default: break;
    }
    return "<unknown>";
}

static const char *string_of_VAEntrypoint(VAEntrypoint entrypoint)
{
    switch (entrypoint) {
#define ENTRYPOINT(entrypoint) \
        case VAEntrypoint##entrypoint: return "VAEntrypoint" #entrypoint
        ENTRYPOINT(VLD);
        ENTRYPOINT(IZZ);
        ENTRYPOINT(IDCT);
        ENTRYPOINT(MoComp);
        ENTRYPOINT(Deblocking);
#undef ENTRYPOINT
    default: break;
    }
    return "<unknown>";
}

static const char *string_of_VADisplayAttribType(VADisplayAttribType type)
{
    switch (type) {
#define TYPE(type) \
        case VADisplayAttrib##type: return "VADisplayAttrib" #type
        TYPE(Brightness);
        TYPE(Contrast);
        TYPE(Hue);
        TYPE(Saturation);
        TYPE(BackgroundColor);
        TYPE(DirectSurface);
#undef TYPE
    default: break;
    }
    return "<unknown>";
}

static void *alloc_buffer(VAAPIContext *vaapi, int type, unsigned int size, VABufferID *buf_id)
{
    VAStatus status;
    void *data = NULL;

    *buf_id = 0;
    status = vaCreateBuffer(vaapi->display, vaapi->context_id,
                            type, size, 1, NULL, buf_id);
    if (!vaapi_check_status(status, "vaCreateBuffer()"))
        return NULL;

    status = vaMapBuffer(vaapi->display, *buf_id, &data);
    if (!vaapi_check_status(status, "vaMapBuffer()"))
        return NULL;

    return data;
}

static void destroy_buffers(VADisplay display, VABufferID *buffers, unsigned int n_buffers)
{
    unsigned int i;
    for (i = 0; i < n_buffers; i++) {
        if (buffers[i]) {
            vaDestroyBuffer(display, buffers[i]);
            buffers[i] = 0;
        }
    }
}

int vaapi_init(VADisplay display)
{
#if 0
    CommonContext * common = common_get_context();
#endif
    VAAPIContext *vaapi;
    int major_version, minor_version;
    int i, num_display_attrs, max_display_attrs;
    VADisplayAttribute *display_attrs;
    VAStatus status;

    if (vaapi_context)
        return 0;

    if (!display)
        return -1;
    D(bug("VA display %p\n", display));

    status = vaInitialize(display, &major_version, &minor_version);
    if (!vaapi_check_status(status, "vaInitialize()"))
        return -1;
    D(bug("VA API version %d.%d\n", major_version, minor_version));

    max_display_attrs = vaMaxNumDisplayAttributes(display);
    display_attrs = malloc(max_display_attrs * sizeof(display_attrs[0]));
    if (!display_attrs)
        return -1;

    num_display_attrs = 0; /* XXX: workaround old GMA500 bug */
    status = vaQueryDisplayAttributes(display, display_attrs, &num_display_attrs);
    if (!vaapi_check_status(status, "vaQueryDisplayAttributes()")) {
        free(display_attrs);
        return -1;
    }
    D(bug("%d display attributes available\n", num_display_attrs));
    for (i = 0; i < num_display_attrs; i++) {
        VADisplayAttribute * const display_attr = &display_attrs[i];
        D(bug("  %-32s (%s/%s) min %d max %d value 0x%x\n",
              string_of_VADisplayAttribType(display_attr->type),
              (display_attr->flags & VA_DISPLAY_ATTRIB_GETTABLE) ? "get" : "---",
              (display_attr->flags & VA_DISPLAY_ATTRIB_SETTABLE) ? "set" : "---",
              display_attr->min_value,
              display_attr->max_value,
              display_attr->value));
    }
    free(display_attrs);

#if 0
    if (common->use_vaapi_background_color) {
        VADisplayAttribute attr;
        attr.type  = VADisplayAttribBackgroundColor;
        attr.value = common->vaapi_background_color;
        status = vaSetDisplayAttributes(display, &attr, 1);
        if (!vaapi_check_status(status, "vaSetDisplayAttributes()"))
            return -1;
    }
#endif

    if ((vaapi = calloc(1, sizeof(*vaapi))) == NULL)
        return -1;
    vaapi->display               = display;
    vaapi->subpic_image.image_id = VA_INVALID_ID;
    for (i = 0; i < ARRAY_ELEMS(vaapi->subpic_ids); i++)
        vaapi->subpic_ids[i]     = VA_INVALID_ID;

    vaapi_context = vaapi;
    return 0;
}

int vaapi_exit(void)
{
    VAAPIContext * const vaapi = vaapi_get_context();
    unsigned int i;

    if (!vaapi)
        return 0;

#if USE_GLX
    if (display_type() == DISPLAY_GLX)
        vaapi_glx_destroy_surface();
#endif

    destroy_buffers(vaapi->display, &vaapi->pic_param_buf_id, 1);
    destroy_buffers(vaapi->display, &vaapi->iq_matrix_buf_id, 1);
    destroy_buffers(vaapi->display, &vaapi->bitplane_buf_id, 1);
    destroy_buffers(vaapi->display, vaapi->slice_buf_ids, vaapi->n_slice_buf_ids);

    if (vaapi->subpic_flags) {
        free(vaapi->subpic_flags);
        vaapi->subpic_flags = NULL;
    }

    if (vaapi->subpic_formats) {
        free(vaapi->subpic_formats);
        vaapi->subpic_formats = NULL;
        vaapi->n_subpic_formats = 0;
    }

    if (vaapi->image_formats) {
        free(vaapi->image_formats);
        vaapi->image_formats = NULL;
        vaapi->n_image_formats = 0;
    }

    if (vaapi->entrypoints) {
        free(vaapi->entrypoints);
        vaapi->entrypoints = NULL;
        vaapi->n_entrypoints = 0;
    }

    if (vaapi->profiles) {
        free(vaapi->profiles);
        vaapi->profiles = NULL;
        vaapi->n_profiles = 0;
    }

    if (vaapi->slice_params) {
        free(vaapi->slice_params);
        vaapi->slice_params = NULL;
        vaapi->slice_params_alloc = 0;
        vaapi->n_slice_params = 0;
    }

    if (vaapi->slice_buf_ids) {
        free(vaapi->slice_buf_ids);
        vaapi->slice_buf_ids = NULL;
        vaapi->n_slice_buf_ids = 0;
    }

    if (vaapi->subpic_image.image_id != VA_INVALID_ID) {
        vaDestroyImage(vaapi->display, vaapi->subpic_image.image_id);
        vaapi->subpic_image.image_id = VA_INVALID_ID;
    }

    for (i = 0; i < ARRAY_ELEMS(vaapi->subpic_ids); i++) {
        if (vaapi->subpic_ids[i] != VA_INVALID_ID) {
            vaDestroySubpicture(vaapi->display, vaapi->subpic_ids[i]);
            vaapi->subpic_ids[i] = VA_INVALID_ID;
        }
    }

    if (vaapi->surface_id) {
        vaDestroySurfaces(vaapi->display, &vaapi->surface_id, 1);
        vaapi->surface_id = 0;
    }

    if (vaapi->context_id) {
        vaDestroyContext(vaapi->display, vaapi->context_id);
        vaapi->context_id = 0;
    }

    if (vaapi->config_id) {
        vaDestroyConfig(vaapi->display, vaapi->config_id);
        vaapi->config_id = 0;
    }

    if (vaapi->display) {
        vaTerminate(vaapi->display);
        vaapi->display = NULL;
    }

    free(vaapi_context);
    return 0;
}

VAAPIContext *vaapi_get_context(void)
{
    return vaapi_context;
}

int vaapi_check_status(VAStatus status, const char *msg)
{
    if (status != VA_STATUS_SUCCESS) {
        fprintf(stderr, "[%s] %s: %s\n", PACKAGE_NAME, msg, vaErrorStr(status));
        return 0;
    }
    return 1;
}

static int has_profile(VAAPIContext *vaapi, VAProfile profile)
{
    VAStatus status;
    int i;

    if (!vaapi->profiles || vaapi->n_profiles == 0) {
        vaapi->profiles = calloc(vaMaxNumProfiles(vaapi->display), sizeof(vaapi->profiles[0]));

        status = vaQueryConfigProfiles(vaapi->display,
                                       vaapi->profiles,
                                       &vaapi->n_profiles);
        if (!vaapi_check_status(status, "vaQueryConfigProfiles()"))
            return 0;

        D(bug("%d profiles available\n", vaapi->n_profiles));
        for (i = 0; i < vaapi->n_profiles; i++)
            D(bug("  %s\n", string_of_VAProfile(vaapi->profiles[i])));
    }

    for (i = 0; i < vaapi->n_profiles; i++) {
        if (vaapi->profiles[i] == profile)
            return 1;
    }
    return 0;
}

static int has_entrypoint(VAAPIContext *vaapi, VAProfile profile, VAEntrypoint entrypoint)
{
    VAStatus status;
    int i;

    if (!vaapi->entrypoints || vaapi->n_entrypoints == 0) {
        vaapi->entrypoints = calloc(vaMaxNumEntrypoints(vaapi->display), sizeof(vaapi->entrypoints[0]));

        status = vaQueryConfigEntrypoints(vaapi->display, profile,
                                          vaapi->entrypoints,
                                          &vaapi->n_entrypoints);
        if (!vaapi_check_status(status, "vaQueryConfigEntrypoints()"))
            return 0;

        D(bug("%d entrypoints available for %s\n", vaapi->n_entrypoints,
              string_of_VAProfile(profile)));
        for (i = 0; i < vaapi->n_entrypoints; i++)
            D(bug("  %s\n", string_of_VAEntrypoint(vaapi->entrypoints[i])));
    }

    for (i = 0; i < vaapi->n_entrypoints; i++) {
        if (vaapi->entrypoints[i] == entrypoint)
            return 1;
    }
    return 0;
}

int vaapi_init_decoder(VAProfile    profile,
                       VAEntrypoint entrypoint,
                       unsigned int picture_width,
                       unsigned int picture_height)
{
    VAAPIContext * const vaapi = vaapi_get_context();
    VAConfigAttrib attrib;
    VAConfigID config_id = 0;
    VAContextID context_id = 0;
    VASurfaceID surface_id = 0;
    VAStatus status;

    if (!vaapi)
        return -1;
#if 0
    if (common_init_decoder(picture_width, picture_height) < 0)
        return -1;
#endif

    if (!has_profile(vaapi, profile))
        return -1;
    if (!has_entrypoint(vaapi, profile, entrypoint))
        return -1;

    if (vaapi->profile != profile || vaapi->entrypoint != entrypoint) {
        if (vaapi->config_id)
            vaDestroyConfig(vaapi->display, vaapi->config_id);

        attrib.type = VAConfigAttribRTFormat;
        status = vaGetConfigAttributes(vaapi->display, profile, entrypoint,
                                       &attrib, 1);
        if (!vaapi_check_status(status, "vaGetConfigAttributes()"))
            return -1;
        if ((attrib.value & VA_RT_FORMAT_YUV420) == 0)
            return -1;

        status = vaCreateConfig(vaapi->display, profile, entrypoint,
                                &attrib, 1, &config_id);
        if (!vaapi_check_status(status, "vaCreateConfig()"))
            return -1;
    }
    else
        config_id = vaapi->config_id;

    if (vaapi->picture_width != picture_width || vaapi->picture_height != picture_height) {
        if (vaapi->surface_id)
            vaDestroySurfaces(vaapi->display, &vaapi->surface_id, 1);

        status = vaCreateSurfaces(vaapi->display, picture_width, picture_height,
                                  VA_RT_FORMAT_YUV420, 1, &surface_id);
        if (!vaapi_check_status(status, "vaCreateSurfaces()"))
            return -1;

        if (vaapi->context_id)
            vaDestroyContext(vaapi->display, vaapi->context_id);

        status = vaCreateContext(vaapi->display, config_id,
                                 picture_width, picture_height,
                                 VA_PROGRESSIVE,
                                 &surface_id, 1,
                                 &context_id);
        if (!vaapi_check_status(status, "vaCreateContext()"))
            return -1;
    }
    else {
        context_id = vaapi->context_id;
        surface_id = vaapi->surface_id;
    }

    vaapi->config_id      = config_id;
    vaapi->context_id     = context_id;
    vaapi->surface_id     = surface_id;
    vaapi->profile        = profile;
    vaapi->entrypoint     = entrypoint;
    vaapi->picture_width  = picture_width;
    vaapi->picture_height = picture_height;
    return 0;
}

static const uint32_t image_formats[] = {
    VA_FOURCC('Y','V','1','2'),
    VA_FOURCC('N','V','1','2'),
    VA_FOURCC('U','Y','V','Y'),
    VA_FOURCC('Y','U','Y','V'),
    VA_FOURCC('A','R','G','B'),
    VA_FOURCC('A','B','G','R'),
    VA_FOURCC('B','G','R','A'),
    VA_FOURCC('R','G','B','A'),
    0
};

static int
get_image_format(
    VAAPIContext   *vaapi,
    uint32_t        fourcc,
    VAImageFormat **image_format
)
{
    VAStatus status;
    int i;

    if (image_format)
        *image_format = NULL;

    if (!vaapi->image_formats || vaapi->n_image_formats == 0) {
        vaapi->image_formats = calloc(vaMaxNumImageFormats(vaapi->display),
                                      sizeof(vaapi->image_formats[0]));
        if (!vaapi->image_formats)
            return 0;

        status = vaQueryImageFormats(vaapi->display,
                                     vaapi->image_formats,
                                     &vaapi->n_image_formats);
        if (!vaapi_check_status(status, "vaQueryImageFormats()"))
            return 0;

        D(bug("%d image formats\n", vaapi->n_image_formats));
        for (i = 0; i < vaapi->n_image_formats; i++)
            D(bug("  %s\n", string_of_VAImageFormat(&vaapi->image_formats[i])));
    }

    for (i = 0; i < vaapi->n_image_formats; i++) {
        if (vaapi->image_formats[i].fourcc == fourcc) {
            if (image_format)
                *image_format = &vaapi->image_formats[i];
            return 1;
        }
    }
    return 0;
}

static int is_vaapi_rgb_format(const VAImageFormat *image_format)
{
    switch (image_format->fourcc) {
    case VA_FOURCC('A','R','G','B'):
    case VA_FOURCC('A','B','G','R'):
    case VA_FOURCC('B','G','R','A'):
    case VA_FOURCC('R','G','B','A'):
        return 1;
    }
    return 0;
}

static int bind_image(VAImage *va_image, Image *image)
{
    VAAPIContext * const vaapi = vaapi_get_context();
    VAImageFormat * const va_format = &va_image->format;
    VAStatus status;
    void *va_image_data;
    unsigned int i;

    if (va_image->num_planes > MAX_IMAGE_PLANES)
        return -1;

    status = vaMapBuffer(vaapi->display, va_image->buf, &va_image_data);
    if (!vaapi_check_status(status, "vaMapBuffer()"))
        return -1;

    memset(image, 0, sizeof(*image));
    image->format = va_format->fourcc;
    if (is_vaapi_rgb_format(va_format)) {
        image->format = image_rgba_format(
            va_format->bits_per_pixel,
            va_format->byte_order == VA_MSB_FIRST,
            va_format->red_mask,
            va_format->green_mask,
            va_format->blue_mask,
            va_format->alpha_mask
        );
        if (!image->format)
            return -1;
    }

    image->width      = va_image->width;
    image->height     = va_image->height;
    image->num_planes = va_image->num_planes;
    for (i = 0; i < va_image->num_planes; i++) {
        image->pixels[i]  = (uint8_t *)va_image_data + va_image->offsets[i];
        image->pitches[i] = va_image->pitches[i];
    }
    return 0;
}

static int release_image(VAImage *va_image)
{
    VAAPIContext * const vaapi = vaapi_get_context();
    VAStatus status;

    status = vaUnmapBuffer(vaapi->display, va_image->buf);
    if (!vaapi_check_status(status, "vaUnmapBuffer()"))
        return -1;
    return 0;
}

int get_image(VASurfaceID surface, Image *dst_img)
{
    VAAPIContext * const vaapi = vaapi_get_context();
    VAImage image;
    VAImageFormat *image_format = NULL;
    VAStatus status;
    Image bound_image;
    int i, is_bound_image = 0, is_derived_image = 0, error = -1;

    image.image_id = VA_INVALID_ID;
    image.buf      = VA_INVALID_ID;

    if (!image_format) {
        status = vaDeriveImage(vaapi->display, surface, &image);
        if (vaapi_check_status(status, "vaDeriveImage()")) {
            if (image.image_id != VA_INVALID_ID && image.buf != VA_INVALID_ID) {
                D(bug("using vaDeriveImage()\n"));
                is_derived_image = 1;
                image_format = &image.format;
            }
            else {
                D(bug("vaDeriveImage() returned success but VA image is invalid. Trying vaGetImage()\n"));
            }
        }
    }

    if (!image_format) {
        for (i = 0; image_formats[i] != 0; i++) {
            if (get_image_format(vaapi, image_formats[i], &image_format))
                break;
        }
    }

    if (!image_format)
        goto end;
    D(bug("selected %s image format for getimage\n",
          string_of_VAImageFormat(image_format)));

    if (!is_derived_image) {
        status = vaCreateImage(vaapi->display, image_format,
                               vaapi->picture_width, vaapi->picture_height,
                               &image);
        if (!vaapi_check_status(status, "vaCreateImage()"))
            goto end;
        D(bug("created image with id 0x%08x and buffer id 0x%08x\n",
              image.image_id, image.buf));

        VARectangle src_rect;

        src_rect.x      = 0;
        src_rect.y      = 0;
        src_rect.width  = vaapi->picture_width;
        src_rect.height = vaapi->picture_height;

        D(bug("src rect (%d,%d):%ux%u\n",
              src_rect.x, src_rect.y, src_rect.width, src_rect.height));

        status = vaGetImage(
            vaapi->display, vaapi->surface_id,
            src_rect.x, src_rect.y, src_rect.width, src_rect.height,
            image.image_id
        );
        if (!vaapi_check_status(status, "vaGetImage()")) {
            vaDestroyImage(vaapi->display, image.image_id);
            goto end;
        }
    }

    if (bind_image(&image, &bound_image) < 0)
        goto end;
    is_bound_image = 1;

    if (image_convert(dst_img, &bound_image) < 0)
        goto end;

    error = 0;
end:
    if (is_bound_image) {
        if (release_image(&image) < 0)
            error = -1;
    }

    if (image.image_id != VA_INVALID_ID) {
        status = vaDestroyImage(vaapi->display, image.image_id);
        if (!vaapi_check_status(status, "vaDestroyImage()"))
            error = -1;
    }
    return error;
}

