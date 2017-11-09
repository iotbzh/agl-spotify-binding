/*
 * Copyright (C) 2015, 2016 "IoT.bzh"
 * Author: José Bollo <jose.bollo@iot.bzh>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#pragma once

#include <curl/curl.h>

extern char *curl_wrap_url (const char *base, const char *path,
                            const char *const *query, size_t * size);

extern int curl_wrap_perform (CURL * curl, char **result, size_t * size);

extern void curl_wrap_do(CURL *curl, void (*callback)(void *closure, int status, CURL *curl, const char *result, size_t size), void *closure);

extern int curl_wrap_content_type_is (CURL * curl, const char *value);

extern CURL *curl_wrap_prepare_get_url(const char *url);

extern CURL *curl_wrap_prepare_get(const char *base, const char *path, const char * const *args);

extern CURL *curl_wrap_prepare_post_url_data(const char *url, const char *datatype, const char *data, size_t szdata);

extern CURL *curl_wrap_prepare_post(const char *base, const char *path, const char * const *args);

extern int curl_wrap_add_header(CURL *curl, const char *header);

extern int curl_wrap_add_header_value(CURL *curl, const char *name, const char *value);

/* vim: set colorcolumn=80: */

