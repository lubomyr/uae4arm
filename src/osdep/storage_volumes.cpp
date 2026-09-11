/*
 * uae4arm - find the storage volumes the file dialogs can reach
 *
 * A volume is only ever reachable by its full path: /storage itself cannot
 * be listed by an app, so the list has to come from StorageManager rather
 * than from readdir. It is also live - a stick pulled out disappears from
 * it - so nothing here may be cached.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <string>
#include <vector>

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"

#ifdef ANDROID
#include <jni.h>
/* Declared here rather than through SDL_android.h: that header forward
   declares its own JavaVM, which clashes with the typedef in jni.h. The
   symbol has C linkage and the pointer is the same either way. */
extern "C" JavaVM *SDL_ANDROID_JavaVM(void);

extern const char *GetInternalStoragePath(void);

static bool jni_detached_by_us = false;

static JNIEnv *jni_begin(void)
{
  JavaVM *vm = SDL_ANDROID_JavaVM();
  JNIEnv *env = NULL;
  if(vm == NULL)
    return NULL;
  if(vm->GetEnv((void **)&env, JNI_VERSION_1_4) == JNI_EDETACHED) {
    if(vm->AttachCurrentThread(&env, NULL) != 0)
      return NULL;
    jni_detached_by_us = true;
  }
  return env;
}


static void jni_end(void)
{
  if(jni_detached_by_us) {
    JavaVM *vm = SDL_ANDROID_JavaVM();
    if(vm != NULL)
      vm->DetachCurrentThread();
    jni_detached_by_us = false;
  }
}


/* Anything may be missing on a given API level, so never let a pending
   exception leak into the next call. */
static bool jni_failed(JNIEnv *env)
{
  if(env->ExceptionCheck()) {
    env->ExceptionClear();
    return true;
  }
  return false;
}


static void jstr(JNIEnv *env, jstring s, char *out, size_t outlen)
{
  const char *c;
  out[0] = '\0';
  if(s == NULL) {
    strncpy(out, "(null)", outlen - 1);
    out[outlen - 1] = '\0';
    return;
  }
  c = env->GetStringUTFChars(s, NULL);
  if(c != NULL) {
    strncpy(out, c, outlen - 1);
    out[outlen - 1] = '\0';
    env->ReleaseStringUTFChars(s, c);
  }
}


/* File.getAbsolutePath() into out; out is "(null)" for a null File. */
static void file_path(JNIEnv *env, jobject file, char *out, size_t outlen)
{
  jclass cls;
  jmethodID mid;
  jstring s;

  strncpy(out, "(null)", outlen - 1);
  out[outlen - 1] = '\0';
  if(file == NULL)
    return;
  cls = env->GetObjectClass(file);
  mid = env->GetMethodID(cls, "getAbsolutePath", "()Ljava/lang/String;");
  if(mid == NULL) {
    env->ExceptionClear();
    env->DeleteLocalRef(cls);
    return;
  }
  s = (jstring)env->CallObjectMethod(file, mid);
  if(!jni_failed(env)) {
    jstr(env, s, out, outlen);
    if(s != NULL)
      env->DeleteLocalRef(s);
  }
  env->DeleteLocalRef(cls);
}


static jobject get_context(JNIEnv *env)
{
  jclass cls = env->FindClass("android/app/ActivityThread");
  jmethodID mid;
  jobject ctx;

  if(cls == NULL) {
    env->ExceptionClear();
    return NULL;
  }
  mid = env->GetStaticMethodID(cls, "currentApplication", "()Landroid/app/Application;");
  if(mid == NULL) {
    env->ExceptionClear();
    env->DeleteLocalRef(cls);
    return NULL;
  }
  ctx = env->CallStaticObjectMethod(cls, mid);
  env->DeleteLocalRef(cls);
  if(jni_failed(env) || ctx == NULL)
    return NULL;
  return ctx;
}

#endif /* ANDROID */


/* ---------- the list the dialogs use ---------- */

#ifdef ANDROID

/* Before API 30 there is no getDirectory() and getPath() is blocked, but
   getExternalFilesDirs() names the same volumes: cut off the private tail
   and what is left is the root of the volume. */
static void volume_root_of(const char *files_dir, char *out, size_t outlen)
{
  const char *tail = strstr(files_dir, "/Android/data/");
  size_t n;

  if(tail == NULL)
    n = strlen(files_dir);
  else
    n = (size_t)(tail - files_dir);
  if(n > outlen - 1)
    n = outlen - 1;
  memcpy(out, files_dir, n);
  out[n] = '\0';
}


static bool can_list(const char *path)
{
  DIR *dir = opendir(path);
  if(dir == NULL)
    return false;
  closedir(dir);
  return true;
}


/* guichan renders text through TTF_RenderText_*, which reads the string as
   Latin-1 - a UTF-8 description such as the Ukrainian name Android gives the
   internal volume comes out as garbage. Only ASCII may reach the list. */
static bool is_ascii(const char *s)
{
  const unsigned char *p = (const unsigned char *)s;
  for(; *p != '\0'; ++p) {
    if(*p < 0x20 || *p > 0x7e)
      return false;
  }
  return true;
}


static const char *path_tail(const char *path)
{
  const char *slash = strrchr(path, '/');
  if(slash != NULL && slash[1] != '\0')
    return slash + 1;
  return path;
}


static void add_volume(std::vector<std::string> *names, std::vector<std::string> *paths,
  const char *name, const char *path)
{
  size_t i;

  if(path == NULL || path[0] != '/')
    return;
  for(i = 0; i < paths->size(); ++i) {
    if((*paths)[i] == path)
      return;
  }
  /* Listed but unreadable is worse than absent: it would look like a broken
     dialog rather than a volume this app may not touch. */
  if(!can_list(path))
    return;
  /* Falling back to the last path element gives the volume's uuid, which is
     what the system prints on the card's own label anyway. */
  if(name == NULL || name[0] == '\0' || !is_ascii(name))
    name = path_tail(path);
  names->push_back(name);
  paths->push_back(path);
}


static void volumes_from_storage_manager(JNIEnv *env, jobject ctx,
  std::vector<std::string> *names, std::vector<std::string> *paths)
{
  jclass ctxcls = env->GetObjectClass(ctx);
  jmethodID gss = env->GetMethodID(ctxcls, "getSystemService",
    "(Ljava/lang/String;)Ljava/lang/Object;");
  jstring svcname;
  jobject sm, list;
  jclass smcls, listcls, volcls;
  jmethodID gsv, sizem, getm;
  jint n, i;

  env->DeleteLocalRef(ctxcls);
  if(gss == NULL) {
    env->ExceptionClear();
    return;
  }
  svcname = env->NewStringUTF("storage");
  sm = env->CallObjectMethod(ctx, gss, svcname);
  env->DeleteLocalRef(svcname);
  if(env->ExceptionCheck()) { env->ExceptionClear(); return; }
  if(sm == NULL)
    return;

  smcls = env->GetObjectClass(sm);
  gsv = env->GetMethodID(smcls, "getStorageVolumes", "()Ljava/util/List;");
  env->DeleteLocalRef(smcls);
  if(gsv == NULL) {
    env->ExceptionClear();
    env->DeleteLocalRef(sm);
    return;
  }
  list = env->CallObjectMethod(sm, gsv);
  env->DeleteLocalRef(sm);
  if(env->ExceptionCheck()) { env->ExceptionClear(); return; }
  if(list == NULL)
    return;

  listcls = env->GetObjectClass(list);
  sizem = env->GetMethodID(listcls, "size", "()I");
  getm = env->GetMethodID(listcls, "get", "(I)Ljava/lang/Object;");
  env->DeleteLocalRef(listcls);
  if(sizem == NULL || getm == NULL) {
    env->ExceptionClear();
    env->DeleteLocalRef(list);
    return;
  }
  n = env->CallIntMethod(list, sizem);
  if(env->ExceptionCheck()) {
    env->ExceptionClear();
    env->DeleteLocalRef(list);
    return;
  }

  volcls = env->FindClass("android/os/storage/StorageVolume");
  if(volcls == NULL)
    env->ExceptionClear();

  for(i = 0; i < n; ++i) {
    jobject vol = env->CallObjectMethod(list, getm, i);
    char desc[128] = "", state[64] = "", dir[MAX_DPATH] = "";
    jmethodID mid;

    if(env->ExceptionCheck()) { env->ExceptionClear(); continue; }
    if(vol == NULL || volcls == NULL)
      continue;

    mid = env->GetMethodID(volcls, "getState", "()Ljava/lang/String;");
    if(mid != NULL) {
      jstring js = (jstring)env->CallObjectMethod(vol, mid);
      if(!env->ExceptionCheck()) {
        jstr(env, js, state, sizeof(state));
        if(js != NULL) env->DeleteLocalRef(js);
      } else
        env->ExceptionClear();
    } else
      env->ExceptionClear();

    /* An unmounted volume has no usable path - and a stick being ejected
       while the dialog is open shows up exactly like this. */
    if(state[0] && strcmp(state, "mounted") != 0 &&
       strcmp(state, "mounted_ro") != 0) {
      env->DeleteLocalRef(vol);
      continue;
    }

    mid = env->GetMethodID(volcls, "getDescription",
      "(Landroid/content/Context;)Ljava/lang/String;");
    if(mid != NULL) {
      jstring js = (jstring)env->CallObjectMethod(vol, mid, ctx);
      if(!env->ExceptionCheck()) {
        jstr(env, js, desc, sizeof(desc));
        if(js != NULL) env->DeleteLocalRef(js);
      } else
        env->ExceptionClear();
    } else
      env->ExceptionClear();
    if(!strcmp(desc, "(null)"))
      desc[0] = '\0';

    mid = env->GetMethodID(volcls, "getDirectory", "()Ljava/io/File;");
    if(mid != NULL) {
      jobject f = env->CallObjectMethod(vol, mid);
      if(!env->ExceptionCheck()) {
        file_path(env, f, dir, sizeof(dir));
        if(f != NULL) env->DeleteLocalRef(f);
      } else
        env->ExceptionClear();
    } else
      env->ExceptionClear();

    /* The primary volume's description is whatever language the phone is
       set to, so give it a name of our own instead. */
    mid = env->GetMethodID(volcls, "isPrimary", "()Z");
    if(mid != NULL) {
      jboolean b = env->CallBooleanMethod(vol, mid);
      if(!env->ExceptionCheck()) {
        if(b)
          strcpy(desc, "Internal storage");
      } else
        env->ExceptionClear();
    } else
      env->ExceptionClear();

    if(dir[0] == '/')
      add_volume(names, paths, desc, dir);

    env->DeleteLocalRef(vol);
  }

  if(volcls != NULL)
    env->DeleteLocalRef(volcls);
  env->DeleteLocalRef(list);
}


static void volumes_from_files_dirs(JNIEnv *env, jobject ctx,
  std::vector<std::string> *names, std::vector<std::string> *paths)
{
  jclass cls = env->GetObjectClass(ctx);
  jmethodID mid = env->GetMethodID(cls, "getExternalFilesDirs",
    "(Ljava/lang/String;)[Ljava/io/File;");
  jobjectArray arr;
  jsize n, i;

  env->DeleteLocalRef(cls);
  if(mid == NULL) {
    env->ExceptionClear();
    return;
  }
  arr = (jobjectArray)env->CallObjectMethod(ctx, mid, (jstring)NULL);
  if(env->ExceptionCheck()) { env->ExceptionClear(); return; }
  if(arr == NULL)
    return;
  n = env->GetArrayLength(arr);
  for(i = 0; i < n; ++i) {
    jobject f = env->GetObjectArrayElement(arr, i);
    char files[MAX_DPATH], root[MAX_DPATH];
    file_path(env, f, files, sizeof(files));
    if(f != NULL)
      env->DeleteLocalRef(f);
    if(files[0] != '/')
      continue;
    volume_root_of(files, root, sizeof(root));
    /* No description available this way; the last path element is the
       volume's uuid, which is at least something to tell them apart. */
    add_volume(names, paths, NULL, root);
  }
  env->DeleteLocalRef(arr);
}

#endif /* ANDROID */


/* Live: call it every time the list is shown, never keep the result. */
void GetStorageVolumes(std::vector<std::string> *names, std::vector<std::string> *paths)
{
  names->clear();
  paths->clear();

#ifdef ANDROID
  {
    JNIEnv *env = jni_begin();
    if(env != NULL) {
      jobject ctx = get_context(env);
      if(ctx != NULL) {
        volumes_from_storage_manager(env, ctx, names, paths);
        if(paths->empty())
          volumes_from_files_dirs(env, ctx, names, paths);
        env->DeleteLocalRef(ctx);
      }
      jni_end();
    }
  }

  /* Whatever else happened, the user must still be able to reach the place
     their files are. */
  if(paths->empty()) {
    const char *root = GetInternalStoragePath();
    if(root != NULL)
      add_volume(names, paths, "Internal storage", root);
  }
#endif
}
