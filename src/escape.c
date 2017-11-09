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

/*
 * Test if 'c' is to be escaped or not.
 * Any character that is not in [-.0-9A-Z_a-z~]
 * must be escaped.
 * Note that space versus + is not managed here.
 */
static inline int should_escape(char c)
{
/* ASCII CODES OF UNESCAPED CHARS
car hx/oct  idx
'-' 2d/055  1
'.' 2e/056  2
'0' 30/060  3
...         ..
'9' 39/071  12
'A' 41/101  13
...         ..
'Z' 5a/132  38
'_' 5f/137  39
'a' 61/141  40
...         ..
'z' 7a/172  65
'~' 7e/176  66
*/
	/* [-.0-9A-Z_a-z~] */
	if (c <= 'Z') {
		/* [-.0-9A-Z] */
		if (c < '0') {
			/* [-.] */
			return c != '-' && c != '.';
		} else {
			/* [0-9A-Z] */
			return c < 'A' && c > '9';
		}
	} else {
		/* [_a-z~] */
		if (c <= 'z') {
			/* [_a-z] */
			return c < 'a' && c != '_';
		} else {
			/* [~] */
			return c != '~';
		}
	}
}

/*
 * returns the ASCII char for the hexadecimal
 * digit of the binary value 'f'.
 * returns 0 if f isn't in [0 ... 15].
 */
static inline char bin2hex(int f)
{
	if ((f & 15) != f)
		f = 0;
	else if (f < 10)
		f += '0';
	else
		f += 'A' - 10;
	return (char)f;
}

/*
 * returns the binary value for the hexadecimal
 * digit whose char is 'c'.
 * returns -1 if c isn't i n[0-9A-Fa-f]
 */
static inline int hex2bin(char c)
{
	/* [0-9A-Fa-f] */
	if (c <= 'F') {
		/* [0-9A-F] */
		if (c <= '9') {
			/* [0-9] */
			if (c >= '0') {
				return (int)(c - '0');
			}
		} else if (c >= 'A') {
			/* [A-F] */
			return (int)(c - ('A' - 10));
		}
	} else {
		/* [a-f] */
		if (c >= 'a' && c <= 'f') {
			return (int)(c - ('a' - 10));
		}
	}
	return -1;
}

/*
 * returns the length that will have the text 'itext' of length
 * 'ilen' when escaped. When 'ilen' == 0, strlen is used to
 * compute the size of 'itext'.
 */
static size_t escaped_length(const char *itext, size_t ilen)
{
	char c;
	size_t i, r;

	if (!ilen)
		ilen = strlen(itext);
	c = itext[i = r = 0];
	while (i < ilen) {
		r += c != ' ' && should_escape(c) ? 3 : 1;
		c = itext[++i];
	}
	return r;
}

/*
 * Escapes the text 'itext' of length 'ilen'.
 * When 'ilen' == 0, strlen is used to compute the size of 'itext'.
 * The escaped text is put in 'otext' of length 'olen'.
 * Returns the length of the escaped text (it can be greater than 'olen').
 * When 'olen' is greater than the needed length, an extra null terminator
 * is appened to the escaped string.
 */
static size_t escape_to(const char *itext, size_t ilen, char *otext, size_t olen)
{
	char c;
	size_t i, r;

	if (!ilen)
		ilen = strlen(itext);
	c = itext[i = r = 0];
	while (i < ilen) {
		if (c == ' ')
			c = '+';
		else if (should_escape(c)) {
			if (r < olen)
				otext[r] = '%';
			r++;
			if (r < olen)
				otext[r] = bin2hex((c >> 4) & 15);
			r++;
			c = bin2hex(c & 15);
		}
		if (r < olen)
			otext[r] = c;
		r++;
		c = itext[++i];
	}
	if (r < olen)
		otext[r] = 0;
	return r;
}

/*
 * returns the length of 'itext' of length 'ilen' that can be unescaped.
 * compute the size of 'itext' when 'ilen' == 0.
 */
static size_t unescapable_length(const char *itext, size_t ilen)
{
	char c;
	size_t i;

	c = itext[i = 0];
	while (i < ilen) {
		if (c != '%')
			i++;
		else {
			if (i + 3 > ilen
			 || hex2bin(itext[i + 1]) < 0
			 || hex2bin(itext[i + 2]) < 0)
				break;
			i += 3;
		}
		c = itext[i];
	}
	return i;
}

/*
 * returns the length that will have the text 'itext' of length
 * 'ilen' when escaped. When 'ilen' == 0, strlen is used to
 * compute the size of 'itext'.
 */
static size_t unescaped_length(const char *itext, size_t ilen)
{
	char c;
	size_t i, r;

	c = itext[i = r = 0];
	while (i < ilen) {
		i += (size_t)(1 + ((c == '%') << 1));
		r++;
		c = itext[i];
	}
	return r;
}

static size_t unescape_to(const char *itext, size_t ilen, char *otext, size_t olen)
{
	char c;
	size_t i, r;
	int h, l;

	ilen = unescapable_length(itext, ilen);
	c = itext[i = r = 0];
	while (i < ilen) {
		if (c != '%') {
			if (c == '+')
				c = ' ';
			i++;
		} else {
			if (i + 2 >= ilen)
				break;
			h = hex2bin(itext[i + 1]);
			l = hex2bin(itext[i + 2]);
			c = (char)((h << 4) | l);
			i += 3;
		}
		if (r < olen)
			otext[r] = c;
		r++;
		c = itext[i];
	}
	if (r < olen)
		otext[r] = 0;
	return r;
}

/* create an url */
char *escape_url(const char *base, const char *path, const char * const *args, size_t *length)
{
	int i;
	size_t lb, lp, lq, l, L;
	const char *null;
	char *result;

	/* ensure args */
	if (!args) {
		null = NULL;
		args = &null;
	}

	/* compute lengths */
	lb = base ? strlen(base) : 0;
	lp = path ? strlen(path) : 0;
	lq = 0;
	i = 0;
	while (args[i]) {
		lq += 1 + escaped_length(args[i], strlen(args[i]));
		i++;
		if (args[i])
			lq += 1 + escaped_length(args[i], strlen(args[i]));
		i++;
	}

	/* allocation */
	L = lb + lp + lq + 1;
	result = malloc(L + 1);
	if (result) {
		/* make the resulting url */
		l = lb;
		if (lb) {
			memcpy(result, base, lb);
			if (result[l - 1] != '/' && path && path[0] != '/')
				result[l++] = '/';
		}
		if (lp) {
			memcpy(result + l, path, lp);
			l += lp;
		}
		i = 0;
		while (args[i]) {
			if (i) {
				result[l++] = '&';
			} else if (base || path) {
				result[l] = memchr(result, '?', l) ? '&' : '?';
				l++;
			}
			l += escape_to(args[i], strlen(args[i]), result + l, L - l);
			i++;
			if (args[i]) {
				result[l++] = '=';
				l += escape_to(args[i], strlen(args[i]), result + l, L - l);
			}
			i++;
		}
		result[l] = 0;
		if (length)
			*length = l;
	}
	return result;
}

char *escape_args(const char * const *args, size_t *length)
{
	return escape_url(NULL, NULL, args, length);
}

const char **unescape_args(const char *args)
{
	const char **r, **q;
	char c, *p;
	size_t j, z, l, n, lt;

	lt = n = 0;
	if (args[0]) {
		z = 0;
		do {
			l = strcspn(&args[z], "&=");
			j = 1 + unescaped_length(&args[z], l);
			lt += j;
			z += l;
			c = args[z++];
			if (c == '=') {
				l = strcspn(&args[z], "&");
				j = 1 + unescaped_length(&args[z], l);
				lt += j;
				z += l;
				c = args[z++];
			}
			n++;
		} while(c);
	}

	l = lt + (2 * n + 1) * sizeof(char *);
	r = malloc(l);
	if (!r)
		return r;

	q = r;
	p = (void*)&r[2 * n + 1];
	if (args[0]) {
		z = 0;
		do {
			q[0] = p;
			l = strcspn(&args[z], "&=");
			j = 1 + unescape_to(&args[z], l, p, lt);
			lt -= j;
			p += j;
			z += l;
			c = args[z++];
			if (c != '=')
				q[1] = NULL;
			else {
				q[1] = p;
				l = strcspn(&args[z], "&");
				j = 1 + unescape_to(&args[z], l, p, lt);
				lt -= j;
				p += j;
				z += l;
				c = args[z++];
			}
			q = &q[2];
		} while(c);
	}
	q[0] = NULL;
	return r;
}

char *escape(const char *text, size_t textlen, size_t *reslength)
{
	size_t len;
	char *result;

	len = 1 + escaped_length(text, textlen);
	result = malloc(len);
	if (result)
		escape_to(text, textlen, result, len);
	if (reslength)
		*reslength = len - 1;
	return result;
}

char *unescape(const char *text, size_t textlen, size_t *reslength)
{
	size_t len;
	char *result;

	len = 1 + unescaped_length(text, textlen);
	result = malloc(len);
	if (result)
		unescape_to(text, textlen, result, len);
	if (reslength)
		*reslength = len - 1;
	return result;
}

#if 1
#include <stdio.h>
int main(int ac, char **av)
{
	int i;
	char *x = escape_args((void*)++av, NULL);
	char *y = escape(x, strlen(x), NULL);
	char *z = unescape(y, strlen(y), NULL);
	const char **v = unescape_args(x);

	printf("%s\n%s\n%s\n", x, y, z);
	free(x);
	free(y);
	free(z);
	i = 0;
	while(v[i]) {
		printf("%s=%s / %s=%s\n", av[i], av[i+1], v[i], v[i+1]);
		i += 2;
	}
	free(v);
	return 0;
}
#endif
/* vim: set colorcolumn=80: */
