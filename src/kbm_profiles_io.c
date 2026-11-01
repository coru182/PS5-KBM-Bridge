/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "kbm_profiles_io.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define KBM_PROFILE_FILE_MAX (32 * 1024)

static void logf_(kbm_log_fn log, void *user, const char *fmt, const char *a, const char *b) {
    if (!log) return;
    char m[256];
    snprintf(m, sizeof(m), fmt, a, b);
    log(user, m);
}

int kbm_profiles_load_dir(KbmConfig *c, const char *dir, kbm_log_fn log, void *user) {
    static char buf[KBM_PROFILE_FILE_MAX + 1];
    DIR *d = opendir(dir);
    if (!d) return 0;
    int loaded = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t n = strlen(e->d_name);
        if (n < 6 || n > 64 || strcmp(e->d_name + n - 5, ".json") || e->d_name[0] == '.') continue;
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        int fd = open(path, O_RDONLY);
        if (fd < 0) continue;
        size_t got = 0;
        ssize_t r;
        while (got < sizeof(buf) - 1 && (r = read(fd, buf + got, sizeof(buf) - 1 - got)) > 0) got += (size_t)r;
        char extra;
        int too_big = got >= sizeof(buf) - 1 && read(fd, &extra, 1) > 0;
        close(fd);
        if (too_big) { logf_(log, user, "custom profile %s: larger than 32 KB, skipped%s", e->d_name, ""); continue; }
        buf[got] = 0;
        char err[128];
        int idx = kbm_custom_load(c, buf, got, err, sizeof(err));
        if (idx < 0) { logf_(log, user, "custom profile %s: %s", e->d_name, err); continue; }
        logf_(log, user, "custom profile %s: loaded '%s'", e->d_name, c->p[idx].id);
        ++loaded;
    }
    closedir(d);
    return loaded;
}

int kbm_profile_file_save(const char *dir, const char *id, const char *data, size_t len) {
    char path[512], tmp[520];
    if (mkdir(dir, 0777) != 0 && errno != EEXIST) return -1;
    snprintf(path, sizeof(path), "%s/%s.json", dir, id);
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return -1;
    size_t off = 0;
    while (off < len) {
        ssize_t w = write(fd, data + off, len - off);
        if (w <= 0) { if (w < 0 && errno == EINTR) continue; close(fd); unlink(tmp); return -1; }
        off += (size_t)w;
    }
    fsync(fd);
    close(fd);
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}

int kbm_profile_file_delete(const char *dir, const char *id) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.json", dir, id);
    if (unlink(path) != 0 && errno != ENOENT) return -1;
    return 0;
}
