//
// Copyright(C) 2018 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// File globbing API. This allows the contents of the filesystem
// to be interrogated.
//

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "i_glob.h"
#include "m_misc.h"
#include "config.h"

// Only the fields d_name and (as an XSI extension) d_ino are specified
// in POSIX.1.  Other than Linux, the d_type field is available mainly
// only on BSD systems.  The remaining fields are available on many, but
// not all systems.
static boolean IsDirectory(char *dir, FILINFO *de)
{
    /*char *filename;
    struct stat sb;
    int result;

    filename = M_StringJoin(dir, DIR_SEPARATOR_S, de->fname, NULL);
    result = M_stat(filename, &sb);
    free(filename);

    if (result != 0)
    {
        return false;
    }*/

    return de->fattrib & AM_DIR != 0;
}

struct glob_s
{
    char **globs;
    int num_globs;
    int flags;
    DIR dir;
    char *directory;
    char *last_filename;
    // These fields are only used when the GLOB_FLAG_SORTED flag is set:
    char **filenames;
    int filenames_len;
    int next_index;
};

static void FreeStringList(char **globs, int num_globs)
{
    int i;
    for (i = 0; i < num_globs; ++i)
    {
        free(globs[i]);
    }
    free(globs);
}

glob_t *I_StartMultiGlob(const char *directory, int flags,
                         const char *glob, ...)
{
    char **globs;
    int num_globs;
    glob_t *result;
    va_list args;

    globs = malloc(sizeof(char *));
    if (globs == NULL)
    {
        return NULL;
    }
    globs[0] = M_StringDuplicate(glob);
    num_globs = 1;

    va_start(args, glob);
    for (;;)
    {
        const char *arg = va_arg(args, const char *);
        char **new_globs;

        if (arg == NULL)
        {
            break;
        }

        new_globs = realloc(globs, sizeof(char *) * (num_globs + 1));
        if (new_globs == NULL)
        {
            FreeStringList(globs, num_globs);
        }
        globs = new_globs;
        globs[num_globs] = M_StringDuplicate(arg);
        ++num_globs;
    }
    va_end(args);

    result = malloc(sizeof(glob_t));
    if (result == NULL)
    {
        FreeStringList(globs, num_globs);
        return NULL;
    }

    FRESULT res = f_opendir(&result->dir, directory);
    if (res != FR_OK)
    {
        FreeStringList(globs, num_globs);
        free(result);
        return NULL;
    }

    result->directory = M_StringDuplicate(directory);
    result->globs = globs;
    result->num_globs = num_globs;
    result->flags = flags;
    result->last_filename = NULL;
    result->filenames = NULL;
    result->filenames_len = 0;
    result->next_index = -1;
    return result;
}

glob_t *I_StartGlob(const char *directory, const char *glob, int flags)
{
    return I_StartMultiGlob(directory, flags, glob, NULL);
}

void I_EndGlob(glob_t *glob)
{
    if (glob == NULL)
    {
        return;
    }

    FreeStringList(glob->globs, glob->num_globs);
    FreeStringList(glob->filenames, glob->filenames_len);

    free(glob->directory);
    free(glob->last_filename);
    f_closedir(&glob->dir);
    free(glob);
}

static boolean MatchesGlob(const char *name, const char *glob, int flags)
{
    int n, g;

    while (*glob != '\0')
    {
        n = *name;
        g = *glob;

        if ((flags & GLOB_FLAG_NOCASE) != 0)
        {
            n = tolower(n);
            g = tolower(g);
        }

        if (g == '*')
        {
            // To handle *-matching we skip past the * and recurse
            // to check each subsequent character in turn. If none
            // match then the whole match is a failure.
            while (*name != '\0')
            {
                if (MatchesGlob(name, glob + 1, flags))
                {
                    return true;
                }
                ++name;
            }
            return glob[1] == '\0';
        }
        else if (g != '?' && n != g)
        {
            // For normal characters the name must match the glob,
            // but for ? we don't care what the character is.
            return false;
        }

        ++name;
        ++glob;
    }

    // Match successful when glob and name end at the same time.
    return *name == '\0';
}

static boolean MatchesAnyGlob(const char *name, glob_t *glob)
{
    int i;

    for (i = 0; i < glob->num_globs; ++i)
    {
        if (MatchesGlob(name, glob->globs[i], glob->flags))
        {
            return true;
        }
    }
    return false;
}

static char *NextGlob(glob_t *glob)
{
    FILINFO info;

    do
    {
        if (f_readdir(&glob->dir, &info) != FR_OK)
        {
            return NULL;
        }
        if (info.fname == NULL || strlen(info.fname) == 0)
        {
            return NULL;
        }

        printf("GLOB %s\n", info.fname);
    } while (IsDirectory(glob->directory, &info)
          || !MatchesAnyGlob(info.fname, glob));

    // Return the fully-qualified path, not just the bare filename.
    return M_StringJoin(glob->directory, DIR_SEPARATOR_S, info.fname, NULL);
}

static void ReadAllFilenames(glob_t *glob)
{
    char *name;

    glob->filenames = NULL;
    glob->filenames_len = 0;
    glob->next_index = 0;

    for (;;)
    {
        name = NextGlob(glob);
        if (name == NULL)
        {
            break;
        }
        glob->filenames = realloc(glob->filenames,
                                  (glob->filenames_len + 1) * sizeof(char *));
        glob->filenames[glob->filenames_len] = name;
        ++glob->filenames_len;
    }
}

static void SortFilenames(char **filenames, int len, int flags)
{
    char *pivot, *tmp;
    int i, left_len, cmp;

    if (len <= 1)
    {
        return;
    }
    pivot = filenames[len - 1];
    left_len = 0;
    for (i = 0; i < len-1; ++i)
    {
        if ((flags & GLOB_FLAG_NOCASE) != 0)
        {
            cmp = strcasecmp(filenames[i], pivot);
        }
        else
        {
            cmp = strcmp(filenames[i], pivot);
        }

        if (cmp < 0)
        {
            tmp = filenames[i];
            filenames[i] = filenames[left_len];
            filenames[left_len] = tmp;
            ++left_len;
        }
    }
    filenames[len - 1] = filenames[left_len];
    filenames[left_len] = pivot;

    SortFilenames(filenames, left_len, flags);
    SortFilenames(&filenames[left_len + 1], len - left_len - 1, flags);
}

const char *I_NextGlob(glob_t *glob)
{
    const char *result;

    if (glob == NULL)
    {
        return NULL;
    }

    // In unsorted mode we just return the filenames as we read
    // them back from the system API.
    if ((glob->flags & GLOB_FLAG_SORTED) == 0)
    {
        free(glob->last_filename);
        glob->last_filename = NextGlob(glob);
        return glob->last_filename;
    }

    // In sorted mode we read the whole list of filenames into memory,
    // sort them and return them one at a time.
    if (glob->next_index < 0)
    {
        ReadAllFilenames(glob);
        SortFilenames(glob->filenames, glob->filenames_len, glob->flags);
    }
    if (glob->next_index >= glob->filenames_len)
    {
        return NULL;
    }
    result = glob->filenames[glob->next_index];
    ++glob->next_index;
    return result;
}
