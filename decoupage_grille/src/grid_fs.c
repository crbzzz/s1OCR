#include "grid_splitter.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

int creer_dossier(const char *chem) {
    if (!chem || !*chem)
        return -1;



#ifdef _WIN32
    if (_mkdir(chem) == 0 || errno == EEXIST)
        return 0;
    if (errno == ENOENT) {
        char tmp[1024];
        char *sep = strrchr(chem, '\\');
        if (!sep)
            sep = strrchr(chem, '/');
        if (!sep)
            return -1;
        size_t len = (size_t)(sep - chem);
        if (len >= sizeof(tmp))
            return -1;
        memcpy(tmp, chem, len);
        tmp[len] = '\0';
        if (creer_dossier(tmp) != 0)
            return -1;
        return _mkdir(chem) == 0 || errno == EEXIST ? 0 : -1;
    }
    return -1;

#else
    if (mkdir(chem, 0755) == 0)
        return 0;

    if (errno == EEXIST)
        return 0;

    if (errno == ENOENT) {
        char tmp[1024];
        char *sep = strrchr(chem, '/');
        
        if (!sep)
            return -1;
        size_t len = (size_t)(sep - chem);
        if (len >= sizeof(tmp))
            return -1;
        memcpy(tmp, chem, len);
        tmp[len] = '\0';
        if (creer_dossier(tmp) != 0)
            return -1;
        return mkdir(chem, 0755) == 0 || errno == EEXIST ? 0 : -1;
    }
    return -1;
#endif
}


int vider_dossier(const char *chem) {
    if (!chem || !*chem)
        return -1;

#ifdef _WIN32
    char pattern[1024];
    int pat_len = snprintf(pattern, sizeof(pattern), "%s\\*.*", chem);
    if (pat_len < 0 || (size_t)pat_len >= sizeof(pattern))
        return -1;

    struct _finddata_t data;
    intptr_t handle = _findfirst(pattern, &data);
    if (handle == -1) {
        if (errno == ENOENT)
            return 0;
        return 0;
    }



    int rc = 0;
    do {
        if (strcmp(data.name, ".") == 0 || strcmp(data.name, "..") == 0)
            continue;
        char full[1024];
        int len = snprintf(full, sizeof(full), "%s\\%s", chem, data.name);
        if (len < 0 || (size_t)len >= sizeof(full)) {
            rc = -1;
            continue;
        }
        if (data.attrib & _A_SUBDIR) {
            if (vider_dossier(full) != 0)
                rc = -1;
            if (_rmdir(full) != 0)
                rc = -1;
        } else {
            if (remove(full) != 0)
                rc = -1;
        }

    } while (_findnext(handle, &data) == 0);
    _findclose(handle);
    return rc;
#else
    DIR *dir = opendir(chem);
    if (!dir) {
        if (errno == ENOENT)
            return 0;
        return -1;
    }

    int rc = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full[1024];
        int n = snprintf(full, sizeof(full), "%s/%s", chem, entry->d_name);
        if (n < 0 || (size_t)n >= sizeof(full)) {
            rc = -1;
            continue;
        }
        struct stat st;
        if (stat(full, &st) != 0) {
            rc = -1;
            continue;

        }
        if (S_ISDIR(st.st_mode)) {
            if (vider_dossier(full) != 0)

                rc = -1;
            if (rmdir(full) != 0)
                rc = -1;
        } else {

            if (remove(full) != 0)
                rc = -1;
        }
    }
    closedir(dir);
    return rc;
    
#endif
}

int pret_dossier(const char *chem) {
    if (creer_dossier(chem) != 0)
        return -1;

    if (vider_dossier(chem) != 0)
        return -1;
    return 0;
}

