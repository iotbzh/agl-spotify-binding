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

#include <json-c/json.h>

#define AFB_BINDING_VERSION 2
#include <afb/afb-binding.h>


static void token (struct afb_req request)
{
	afb_req_success(request, NULL, NULL);
}

static void player (struct afb_req request)
{
	afb_req_success(request, NULL, NULL);
}

static int service_init()
{
	atexit(do_stop);
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

