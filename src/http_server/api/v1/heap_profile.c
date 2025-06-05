/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2015-2024 The Fluent Bit Authors
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <fluent-bit/flb_info.h>
#include <fluent-bit/flb_http_server.h>
#include <fluent-bit/flb_mem.h>
#include <fluent-bit/flb_log.h>

#ifdef FLB_HAVE_JEMALLOC_PROFILING

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#ifdef FLB_HAVE_JEMALLOC
#include <jemalloc/jemalloc.h>
#endif

/* API: Generate heap profile */
static void cb_heap_profile(mk_request_t *request, void *data)
{
    char temp_template[] = "/tmp/fluent-bit-heap-XXXXXX";
    int temp_fd;
    FILE *profile_file = NULL;
    char *profile_data = NULL;
    long profile_size = 0;
    struct stat st;
    const char *filename;
    size_t bytes_read;

#ifdef FLB_HAVE_JEMALLOC
    bool prof_active;
    size_t sz;
#endif

#ifdef FLB_HAVE_JEMALLOC
    /* Check if profiling is active */
    prof_active = false;
    sz = sizeof(prof_active);
    
    if (mallctl("prof.active", &prof_active, &sz, NULL, 0) != 0) {
        flb_error("[heap_profile] Failed to check if profiling is active");
        mk_http_status(request, 500);
        mk_http_send(request, "Profiling status check failed", 30, NULL);
        mk_http_done(request);
        return;
    }

    if (!prof_active) {
        mk_http_status(request, 503);
        mk_http_send(request, "Heap profiling is not active. Enable with prof:true jemalloc option.", 70, NULL);
        mk_http_done(request);
        return;
    }

    /* Create secure temporary file for profile dump */
    temp_fd = mkstemp(temp_template);
    if (temp_fd == -1) {
        flb_error( "[heap_profile] Failed to create temporary file");
        mk_http_status(request, 500);
        mk_http_send(request, "Failed to create temporary file", 31, NULL);
        mk_http_done(request);
        return;
    }
    close(temp_fd);  /* Close fd, we just need the filename */

    /* Dump heap profile to temporary file */
    filename = temp_template;
    if (mallctl("prof.dump", NULL, NULL, &filename, sizeof(filename)) != 0) {
        flb_error( "[heap_profile] Failed to dump heap profile");
        unlink(temp_template);
        mk_http_status(request, 500);
        mk_http_send(request, "Failed to dump heap profile", 28, NULL);
        mk_http_done(request);
        return;
    }

    /* Read the profile file */
    if (stat(temp_template, &st) != 0) {
        flb_error( "[heap_profile] Failed to stat profile file: %s", temp_template);
        unlink(temp_template);
        mk_http_status(request, 500);
        mk_http_send(request, "Failed to access profile file", 30, NULL);
        mk_http_done(request);
        return;
    }

    profile_size = st.st_size;
    if (profile_size <= 0) {
        flb_error( "[heap_profile] Profile file is empty");
        unlink(temp_template);
        mk_http_status(request, 500);
        mk_http_send(request, "Profile file is empty", 22, NULL);
        mk_http_done(request);
        return;
    }

    profile_data = flb_malloc(profile_size);
    if (!profile_data) {
        flb_error( "[heap_profile] Failed to allocate memory for profile data");
        unlink(temp_template);
        mk_http_status(request, 500);
        mk_http_send(request, "Memory allocation failed", 25, NULL);
        mk_http_done(request);
        return;
    }

    profile_file = fopen(temp_template, "rb");
    if (!profile_file) {
        flb_error( "[heap_profile] Failed to open profile file: %s", temp_template);
        flb_free(profile_data);
        unlink(temp_template);
        mk_http_status(request, 500);
        mk_http_send(request, "Failed to open profile file", 28, NULL);
        mk_http_done(request);
        return;
    }

    bytes_read = fread(profile_data, 1, profile_size, profile_file);
    fclose(profile_file);
    unlink(temp_template);

    if (bytes_read != profile_size) {
        flb_error( "[heap_profile] Failed to read complete profile file");
        flb_free(profile_data);
        mk_http_status(request, 500);
        mk_http_send(request, "Failed to read profile file", 28, NULL);
        mk_http_done(request);
        return;
    }

    /* Send the profile data */
    mk_http_status(request, 200);
    mk_http_header(request, "Content-Type", 12, "application/octet-stream", 24);
    mk_http_header(request, "Content-Disposition", 19, "attachment; filename=\"heap.prof\"", 33);
    mk_http_send(request, profile_data, profile_size, NULL);
    mk_http_done(request);
    
    flb_free(profile_data);
#else
    mk_http_status(request, 501);
    mk_http_send(request, "Jemalloc not available", 23, NULL);
    mk_http_done(request);
#endif
}

/* Perform registration */
int api_v1_heap_profile(struct flb_hs *hs)
{
    mk_vhost_handler(hs->ctx, hs->vid, "/api/v1/heap_profile", cb_heap_profile, hs);
    return 0;
}

#endif /* FLB_HAVE_JEMALLOC_PROFILING */