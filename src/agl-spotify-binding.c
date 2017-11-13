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
#include <sys/wait.h>

#include <json-c/json.h>

#define AFB_BINDING_VERSION 2
#include <afb/afb-binding.h>

#include "curl-wrap.h"

static char *user;
static char *reftok;
static char *bearer;
static int expire;
static time_t endat;
static char endpoint[] = "https://agl-graphapi.forgerocklabs.org";
static pid_t pid;

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
	struct json_object *data, *resp;

	rc = afb_service_call_sync("identity", "get", NULL, &data);
	if (rc == 0) {
		if (data) {
			if (json_object_object_get_ex(data, "response", &resp)) {
				objsetstr(resp, "spotify_refresh_token", &reftok, NULL);
				objsetstr(resp, "name", &user, NULL);
			}
			json_object_put(data);
		} else {
			free(reftok);
			reftok = NULL;
			free(user);
			user = NULL;
		}
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
	pid_t p = pid, r;

	if (p) {
		pid = 0;
		expire = 0;
		endat = 0;
		r = waitpid(p, NULL, WNOHANG);
		if (r == 0) {
			kill(p, SIGKILL);
			waitpid(p, NULL, 0);
		}
	}
}

static void do_start()
{
	if (user && !pid) {
		pid = fork();
		if (!pid) {
			execl("/bin/bash", "/bin/bash", "/usr/libexec/spotify/playspot", user, NULL);
			_exit(1);
		}
	}
}

static void run()
{
	get_data();
	do_start();
	do_refresh();
}

static void return_bearer (struct afb_req request)
{
	if (bearer)
		afb_req_success(request, json_object_new_string(bearer), NULL);
	else
		afb_req_fail(request, "no-bearer", NULL);
}

static void token (struct afb_req request)
{
	do_refresh();
	return_bearer(request);
}

static void player (struct afb_req request)
{
	const char *v;

	do_stop();
	v = afb_req_value(request, "stop");
	if (!v || (strcasecmp(v,"false") && strcmp(v,"0")))
		run();
	return_bearer(request);
}

static int init()
{
	atexit(do_stop);
	afb_daemon_require_api("identity", 1);
	afb_service_call("identity", "subscribe", NULL, NULL, NULL);
	run();
	return 0;
}

static void onevent_job(int signum, void *arg)
{
	if (!signum) {
		free(user); user = NULL;
		free(reftok); reftok = NULL;
		free(bearer); bearer = NULL;
		do_stop();
		if (arg)
			run();
	}
}

static void onevent(const char *event, struct json_object *object)
{
	struct json_object *evtname;
	const char *evt;

	AFB_NOTICE("Received event: %s (%s)", event, json_object_to_json_string(object));
	if (json_object_object_get_ex(object, "eventName", &evtname)) {
		evt = json_object_get_string(evtname);
		if (!strcmp("logout", evt))
			afb_daemon_queue_job(onevent_job, NULL, onevent_job, 0);
		else if (!strcmp("login", evt))
			afb_daemon_queue_job(onevent_job, onevent_job, onevent_job, 0);
	}
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
	.onevent = onevent,
	.noconcurrency = 0
};

/* vim: set colorcolumn=80: */

