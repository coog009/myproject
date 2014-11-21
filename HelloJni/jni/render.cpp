#include <jni.h>
#include <android/log.h>

#include <GLES/gl.h>
#include <GLES/glext.h>
#include <android/bitmap.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define  LOG_TAG    "libgljni"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

#define POSITION_DATA_SIZE 3
#define COLOR_DATA_SIZE 4

//四边形的顶点坐标系
const float vertex[] = {
                -1.0f, -1.0f,
                1.0f, -1.0f,
                -1.0f, 1.0f,
                1.0f, 1.0f
};
//纹理坐标系
const float coord[] = {
                0,1.0f,
                1.0f,1.0f,
                0,0,
                1.0f,0
};
//纹理存储定义，一般用来存名称
GLuint textures[1] = {0};

static void *bitmap_pixels = NULL;
static int bitmap_width = 0;
static int bitmap_height = 0;
static bool isFirstTime = true;

extern "C"
{
JNIEXPORT void JNICALL Java_com_example_hellojni_LibOpengles_create(JNIEnv * env, jclass object, jint width, jint height);
JNIEXPORT void JNICALL Java_com_example_hellojni_LibOpengles_init(JNIEnv * env, jclass object);
JNIEXPORT void JNICALL Java_com_example_hellojni_LibOpengles_draw(JNIEnv * env, jclass object);
JNIEXPORT void JNICALL Java_com_example_hellojni_LibOpengles_setbitmap(JNIEnv * env, jclass object, jobject bitmap, jint width, jint height);
}

JNIEXPORT void JNICALL Java_com_example_hellojni_LibOpengles_create(JNIEnv * env, jclass object, jint width, jint height)
{
    LOGI("create");
    //设置场景大小
    glViewport(0, 0, width, height);
    float ratio = (float) width / height;
    //投影矩阵
    glMatrixMode(GL_PROJECTION);
    //重置视图
    glLoadIdentity();
}

JNIEXPORT void JNICALL Java_com_example_hellojni_LibOpengles_init(JNIEnv * env, jclass object)
{
    LOGI("init");

    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    // 黑色背景色
    glClearColorx(0, 0, 0, 0);
    // 启用阴影平滑
    glShadeModel(GL_SMOOTH);
    // 启用深度测试
    glEnable(GL_DEPTH_TEST);
    // 深度测试类型
    glDepthFunc(GL_LEQUAL);
    // 设置深度缓存
    glClearDepthf(1.0f);

    // 启用纹理
    glEnable(GL_TEXTURE_2D);
    // 创建纹理
    glGenTextures(1, textures);
//     绑定纹理
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    //生成纹理
//      GLES11.glTexImage2D(GLES11.GL_TEXTURE_2D, 0, GLES11.GL_RGBA, 256, 256, 0, GLES11.GL_RGBA,
//              GLES11.GL_UNSIGNED_BYTE, bitmap);
//    glTexImage2D(GL_TEXTURE_2D, 0, bitmap, 0);

    //线性滤波
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);//放大时
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);//缩小时
//      GLES11.glPointSize(1.0f);
}

JNIEXPORT void JNICALL Java_com_example_hellojni_LibOpengles_draw(JNIEnv * env, jclass object)
{
//    LOGI("step");
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    //开启顶点和纹理缓冲
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    //往里面进去一点
    //      GLES11.glTranslatef(0.0f, 0.0f, -6.0f);
    //设置顶点和纹理的位置、类型
    glVertexPointer(2, GL_FLOAT, 0, vertex);
    glTexCoordPointer(2, GL_FLOAT, 0, coord);

    glBindTexture(GL_TEXTURE_2D, textures[0]);
    if (isFirstTime) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap_width, bitmap_height, 0,
                        GL_RGBA, GL_UNSIGNED_BYTE, bitmap_pixels);
        isFirstTime = false;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bitmap_width, bitmap_height,
                        GL_RGBA, GL_UNSIGNED_BYTE, bitmap_pixels);
    }

    //绘图
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    //取消缓冲
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    //结束绘图
    glFinish();
}

JNIEXPORT void JNICALL Java_com_example_hellojni_LibOpengles_setbitmap(JNIEnv * env, jclass object, jobject bitmap, jint width, jint height)
{
    int ret;
    void *pixels;

    AndroidBitmapInfo info;

    AndroidBitmap_getInfo(env, bitmap, &info);
    if(info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGE("Bitmap format is not RGBA_8888!");
        return;
    } else {
        LOGE("Bitmap format is RGBA_8888!");
    }

    if ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0) {
        return;
    }

    if (bitmap_pixels == NULL) {
        bitmap_pixels = new unsigned char[width * height * 4];
    }

    memcpy(bitmap_pixels, pixels, width * height * 4);
    bitmap_width = width;
    bitmap_height = height;

    LOGE("width:%d height:%d", width, height);

//    copy_pixel_buffer(pixels, (UINT8*)(g_spice_conn->primary_buffer), x, y, width, height,
//            g_spice_conn->width, g_spice_conn->height, 4);

    AndroidBitmap_unlockPixels(env, bitmap);
}
