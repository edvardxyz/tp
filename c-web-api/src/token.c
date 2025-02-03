#include <jwt.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define ISS "skema-api"
static const unsigned char JWT_SECRET_KEY[] = "123123";

char * create_jwt_hs256(const char * username) {

	jwt_t * jwt = NULL;
	int ret;
	char * out = NULL;

	ret = jwt_new(&jwt);
	if (ret != 0) {
		fprintf(stderr, "Failed to create JWT object.\n");
		return NULL;
	}

	ret = jwt_set_alg(jwt, JWT_ALG_HS256, JWT_SECRET_KEY, sizeof(JWT_SECRET_KEY));
	if (ret != 0) {
		fprintf(stderr, "Failed to set JWT algorithm.\n");
		jwt_free(jwt);
		return NULL;
	}

	jwt_add_grant(jwt, "sub", username);
	jwt_add_grant(jwt, "iss", ISS);

	time_t now = time(NULL);
	jwt_add_grant_int(jwt, "iat", (long)now);

	long exp_time = now + 3600;
	jwt_add_grant_int(jwt, "exp", exp_time);

	jwt_add_grant(jwt, "role", "admin");

	out = jwt_encode_str(jwt);
	jwt_free(jwt);

	return out;
}

int verify_jwt_hs256(const char * token) {
	jwt_t * jwt = NULL;
	int ret;

	ret = jwt_decode(&jwt, token, JWT_SECRET_KEY, sizeof(JWT_SECRET_KEY));
	if (ret != 0) {
		fprintf(stderr, "JWT decode failed: %d\n", ret);
		return 0;
	}

	time_t now = time(NULL);

	time_t exp = (time_t)jwt_get_grant_int(jwt, "exp");
	if (exp < now) {
		fprintf(stderr, "Token expired.\n");
		jwt_free(jwt);
		return 0;
	}

	const char * issuer = jwt_get_grant(jwt, "iss");
	if (!issuer || strcmp(issuer, ISS) != 0) {
		fprintf(stderr, "Issuer mismatch.\n");
		jwt_free(jwt);
		return 0;
	}

	jwt_free(jwt);
	return 1;
}

int authenticate(http_s * request) {

	uint64_t auth_hash = fiobj_hash_string("authorization", 13);
	if (!request->headers) {
		http_send_error(request, 401);
		return 0;
	}
	FIOBJ auth_value = fiobj_hash_get2(request->headers, auth_hash);
	if (!auth_value) {
		http_send_error(request, 401);
		return 0;
	}

	fio_str_info_s auth_str = fiobj_obj2cstr(auth_value);

	const char * prefix = "Bearer ";
	if (auth_str.len < strlen(prefix) ||
		strncmp(auth_str.data, prefix, strlen(prefix)) != 0) {
		http_send_error(request, 401);
		return 0;
	}

	const char * token = auth_str.data + strlen(prefix);

	if (!verify_jwt_hs256(token)) {
		http_send_error(request, 401);
		return 0;
	}

	return 1;
}
