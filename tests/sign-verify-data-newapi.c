/*
 * Copyright (C) 2017 Red Hat, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifndef _WIN32
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif
#include <unistd.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/abstract.h>
#include "common-key-tests.h"
#include "utils.h"

/* verifies whether the sign-data and verify-data APIs
 * operate as expected */

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "<%d> %s", level, str);
}

/* sha1 hash of "hello" string */
const gnutls_datum_t raw_data = {
	(void *)"\xaa\xf4\xc6\x1d\xdc\xc5\xe8\xa2\xda\xbe"
		"\xde\x0f\x3b\x48\x2c\xd9\xae\xa9\x43\x4d",
	20
};

const gnutls_datum_t invalid_raw_data = {
	(void *)"\xaa\xf4\xc6\x1d\xdc\xc5\xe8\xa2\xda\xbe"
		"\xde\x0f\x3b\x48\x3c\xd9\xae\xa9\x43\x4d",
	20
};

#define tests common_key_tests
#define testfail(fmt, ...) fail("%s: " fmt, tests[i].name, ##__VA_ARGS__)

void doit(void)
{
	gnutls_x509_crt_t crt;
	gnutls_pubkey_t pubkey;
	gnutls_privkey_t privkey;
	gnutls_sign_algorithm_t sign_algo;
	gnutls_datum_t signature;
	int ret;
	size_t i;

	global_init();

	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(6);

	for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		if (tests[i].pk == GNUTLS_PK_DSA)
			continue;

		success("testing: %s - %s\n", tests[i].name,
			gnutls_sign_algorithm_get_name(tests[i].sigalgo));

		ret = gnutls_pubkey_init(&pubkey);
		if (ret < 0)
			testfail("gnutls_privkey_init\n");

		ret = gnutls_privkey_init(&privkey);
		if (ret < 0)
			testfail("gnutls_pubkey_init\n");

		ret = gnutls_privkey_import_x509_raw(
			privkey, &tests[i].key, GNUTLS_X509_FMT_PEM, NULL, 0);
		if (ret < 0)
			testfail("gnutls_privkey_import_x509\n");

		ret = gnutls_privkey_sign_data2(privkey, tests[i].sigalgo, 0,
						&raw_data, &signature);
		if (ret < 0)
			testfail("gnutls_x509_privkey_sign_hash\n");

		ret = gnutls_x509_crt_init(&crt);
		if (ret < 0)
			testfail("gnutls_x509_crt_init\n");

		ret = gnutls_x509_crt_import(crt, &tests[i].cert,
					     GNUTLS_X509_FMT_PEM);
		if (ret < 0)
			testfail("gnutls_x509_crt_import\n");

		ret = gnutls_pubkey_import_x509(pubkey, crt, 0);
		if (ret < 0)
			testfail("gnutls_x509_pubkey_import\n");

		ret = gnutls_pubkey_verify_data2(pubkey, tests[i].sigalgo, 0,
						 &raw_data, &signature);
		if (ret < 0)
			testfail("gnutls_x509_pubkey_verify_data2\n");

		/* Test functionality of GNUTLS_VERIFY_DISABLE_CA_SIGN flag (see issue #754) */
		ret = gnutls_pubkey_verify_data2(pubkey, tests[i].sigalgo,
						 GNUTLS_VERIFY_DISABLE_CA_SIGN,
						 &raw_data, &signature);
		if (ret < 0)
			testfail("gnutls_x509_pubkey_verify_data2\n");

		/* should fail */
		ret = gnutls_pubkey_verify_data2(pubkey, tests[i].sigalgo, 0,
						 &invalid_raw_data, &signature);
		if (ret != GNUTLS_E_PK_SIG_VERIFY_FAILED)
			testfail(
				"gnutls_x509_pubkey_verify_data2-2 (hashed data)\n");

		sign_algo = gnutls_pk_to_sign(
			gnutls_pubkey_get_pk_algorithm(pubkey, NULL),
			tests[i].digest);
		ret = gnutls_pubkey_verify_data2(pubkey, sign_algo, 0,
						 &raw_data, &signature);
		if (ret < 0)
			testfail(
				"gnutls_x509_pubkey_verify_data2-1 (hashed data)\n");

		/* should fail */
		ret = gnutls_pubkey_verify_data2(pubkey, sign_algo, 0,
						 &invalid_raw_data, &signature);
		if (ret != GNUTLS_E_PK_SIG_VERIFY_FAILED)
			testfail(
				"gnutls_x509_pubkey_verify_data2-2 (hashed data)\n");

		/* test the raw interface */
		gnutls_free(signature.data);
		signature.data = NULL;

		gnutls_free(signature.data);
		gnutls_x509_crt_deinit(crt);
		gnutls_privkey_deinit(privkey);
		gnutls_pubkey_deinit(pubkey);
	}

	gnutls_global_deinit();
}
