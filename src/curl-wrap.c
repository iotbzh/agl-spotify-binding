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
 */
#define _GNU_SOURCE

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <curl/curl.h>

#include "curl-wrap.h"
#include "escape.h"


/* internal representation of buffers */
struct buffer {
	size_t size;
	char *data;
};

/* write callback for filling buffers with the response */
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	struct buffer *buffer = userdata;
	size_t sz = size * nmemb;
	size_t old_size = buffer->size;
	size_t new_size = old_size + sz;
	char *data = realloc(buffer->data, new_size + 1);
	if (!data)
		return 0;
	memcpy(&data[old_size], ptr, sz);
	data[new_size] = 0;
	buffer->size = new_size;
	buffer->data = data;
	return sz;
}

/* 
 * Perform the CURL operation for 'curl' and put the result in
 * memory. If 'result' isn't NULL it receives the returned content
 * that then must be freed. If 'size' isn't NULL, it receives the
 * size of the returned content. Note that if not NULL, the real
 * content is one byte greater than the read size and the last byte
 * zero. This facility allows to handle the returned content as a
 * null terminated C-string.
 */
int curl_wrap_perform(CURL *curl, char **result, size_t *size)
{
	int rc;
	struct buffer buffer;
	CURLcode code;

	/* init tthe buffer */
	buffer.size = 0;
	buffer.data = NULL;

	/* Perform the request, res will get the return code */ 
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

	/* Perform the request, res will get the return code */ 
	code = curl_easy_perform(curl);
	rc = code == CURLE_OK;

	/* Check for no errors */ 
	if (rc) {
		/* no error */
		if (size)
			*size = buffer.size;
		if (result)
			*result = buffer.data;
		else
			free(buffer.data);
	} else {
		/* had error */
		if (size)
			*size = 0;
		if (result)
			*result = NULL;
		free(buffer.data);
	}

	return rc;
}

void curl_wrap_do(CURL *curl, void (*callback)(void *closure, int status, CURL *curl, const char *result, size_t size), void *closure)
{
	int rc;
	char *result;
	size_t size;
	char errbuf[CURL_ERROR_SIZE];

	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
	rc = curl_wrap_perform(curl, &result, &size);
	if (rc)
		callback(closure, rc, curl, result, size);
	else
		callback(closure, rc, curl, errbuf, 0);
	free(result);
	curl_easy_cleanup(curl);
}

int curl_wrap_content_type_is(CURL *curl, const char *value)
{
	char *actual;
	CURLcode code;

	code = curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &actual);
	if (code != CURLE_OK || !actual)
		return 0;

	return !strncasecmp(actual, value, strcspn(actual, "; "));
}

CURL *curl_wrap_prepare_get_url(const char *url)
{
	CURL *curl;
	CURLcode code;

	curl = curl_easy_init();
	if(curl) {
		code = curl_easy_setopt(curl, CURLOPT_URL, url);
		if (code == CURLE_OK)
			return curl;
		curl_easy_cleanup(curl);
	}
	return NULL;
}

CURL *curl_wrap_prepare_get(const char *base, const char *path, const char * const *args)
{
	CURL *res;
	char *url;

	url = escape_url(base, path, args, NULL);
	res = url ? curl_wrap_prepare_get_url(url) : NULL;
	free(url);
	return res;
}

int curl_wrap_add_header(CURL *curl, const char *header)
{
	int rc;
	struct curl_slist *list;

	list = curl_slist_append(NULL, header);
	rc = list ? curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list) == CURLE_OK : 0;
/*
	curl_slist_free_all(list);
*/
	return rc;
}

int curl_wrap_add_header_value(CURL *curl, const char *name, const char *value)
{
	char *h;
	int rc;

	rc = asprintf(&h, "%s: %s", name, value);
	rc = rc < 0 ? 0 : curl_wrap_add_header(curl, h);
	free(h);
	return rc;
}


CURL *curl_wrap_prepare_post_url_data(const char *url, const char *datatype, const char *data, size_t szdata)
{
	CURL *curl;

	curl = curl_easy_init();
	if (curl
	 && CURLE_OK == curl_easy_setopt(curl, CURLOPT_URL, url)
	 && (!szdata || CURLE_OK == curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, szdata))
	 && CURLE_OK == curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data)
	 && (!datatype || curl_wrap_add_header_value(curl, "content-type", datatype)))
		return curl;
	curl_easy_cleanup(curl);
	return NULL;
}

CURL *curl_wrap_prepare_post(const char *base, const char *path, const char * const *args)
{
	CURL *res;
	char *url;
	char *data;
	size_t szdata;

	url = escape_url(base, path, NULL, NULL);
	data = escape_args(args, &szdata);
	res = url ? curl_wrap_prepare_post_url_data(url, NULL, data, szdata) : NULL;
	free(url);
	return res;
}

/* vim: set colorcolumn=80: */
