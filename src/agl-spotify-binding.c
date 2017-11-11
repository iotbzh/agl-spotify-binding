/*
 * Copyright (C) 2015, 2016, 2017 "IoT.bzh"
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

#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>

#include <json-c/json.h>

#define AFB_BINDING_VERSION 2
#include <afb/afb-binding.h>
#include <systemd/sd-event.h>

#include "curl-wrap.h"

static char *user;
static char *reftok;
static char *bearer;
static int expire;
static time_t endat;
static char endpoint[] = "https://agl-graphapi.forgerocklabs.org";
static pid_t pid;
static struct sd_event_source *srchld;

static void objsetstr(struct json_object *obj, const char *name, char **value, const char *def)
{
        struct json_object *v;
        const char *s;
        char *p;

        s = obj && json_object_object_get_ex(obj, name, &v) ? json_object_get_string(v) : NULL;
        p = *value;
	if (s || p != def) {
		*value = s || def ? strdup(s ?: def) : NULL;
                free(p);
        }
}

static void objsetint(struct json_object *obj, const char *name, int *value, int def)
{
        struct json_object *v;

	*value = (obj && json_object_object_get_ex(obj, name, &v)
		  && json_object_get_type(v) == json_type_int) ? json_object_get_int(v) : def;
}

static void get_data()
{
	int rc;
	struct json_object *data;

	rc = afb_service_call_sync("identity", "get", NULL, &data);
	if (rc == 0) {
		objsetstr(data, "spotify_refresh_token", &reftok, NULL);
		objsetstr(data, "name", &user, NULL);
		json_object_put(data);
	}
}

static void do_refresh()
{
	int rc;
	char *url;
	CURL *curl;
	struct json_object *data;
	char *result;

	if (endat && endat > time(NULL))
		return;

	rc = asprintf(&url, "%s/spotify/token?uid=%s", endpoint, user);
	if (rc >= 0) {
		curl = curl_wrap_prepare_get_url(url);
		if (curl) {
			rc = curl_wrap_perform (curl, &result, NULL);
			if (rc >= 0) {
				data = json_tokener_parse(result);
				if (data) {
					objsetstr(data, "access_token", &bearer, bearer);
					objsetint(data, "expires_in", &expire, 3600);
					json_object_put(data);
					endat = time(NULL) + expire - (expire > 60 ? 60 : 0);
				}
				free(result);
			}
		}
	}
}

static void do_stop()
{
	pid_t p = pid;

	if (p) {
		pid = 0;
		if (srchld) {
			sd_event_source_set_enabled(srchld, SD_EVENT_OFF);
			sd_event_source_unref(srchld);
			srchld = NULL;
		}
		kill(p, SIGKILL);
		expire = 0;
		endat = 0;
	}
}

static int on_sigchild(sd_event_source *s, const siginfo_t *si, void *userdata)
{
	pid = 0;
	sd_event_source_set_enabled(srchld, SD_EVENT_OFF);
	sd_event_source_unref(srchld);
	srchld = NULL;
	return 0;
}

static void do_start()
{
	if (!pid) {
		pid = fork();
		if (pid) {
			if (srchld) {
				sd_event_source_set_enabled(srchld, SD_EVENT_OFF);
				sd_event_source_unref(srchld);
				srchld = NULL;
			}
			sd_event_add_child(afb_daemon_get_event_loop(), &srchld, pid, WEXITED, on_sigchild, NULL);
		} else {
			execl("/usr/libexec/spotify/playspot", "playspot", user, NULL);
			exit(1);
		}
	}
}

static void run()
{
	get_data();
	do_refresh();
	do_start();
}

static void token (struct afb_req request)
{
	do_refresh();
	afb_req_success(request, json_object_new_string(bearer), NULL);
}

static void player (struct afb_req request)
{
	const char *v;
	do_stop();

	v = afb_req_value(request, "stop");
	if (!v || (strcasecmp(v,"false") && strcmp(v,"0")))
		run();
	afb_req_success(request, json_object_new_string(bearer), NULL);
}

static int init()
{
	atexit(do_stop);
	afb_daemon_require_api("identity", 1);
	run();
	return 0;
}


// NOTE: this sample does not use session to keep test a basic as possible
//       in real application most APIs should be protected with AFB_SESSION_CHECK
static const struct afb_verb_v2 verbs[]=
{
  {"player" , player , NULL, "player control" , AFB_SESSION_NONE },
  {"token"  , token  , NULL, "token refresh"  , AFB_SESSION_NONE },
  {NULL}
};

const struct afb_binding_v2 afbBindingV2 =
{
	.api = "spotify",
	.specification = NULL,
	.info = "AGL spotify service",
	.verbs = verbs,
	.preinit = NULL,
	.init = init,
	.onevent = NULL,
	.noconcurrency = 0
};

/* vim: set colorcolumn=80: */

