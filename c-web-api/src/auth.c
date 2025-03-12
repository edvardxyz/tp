#include <jwt.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ulfius.h>

#define ISS "snif-api"
static const unsigned char JWT_SECRET_KEY[] = "secret32329";
#include "db.h"
#include <bcrypt.h>

char * print_map(const struct _u_map * map) {
	char *line, *to_return = NULL;
	const char **keys, *value;
	int len, i;
	if (map != NULL) {
		keys = u_map_enum_keys(map);
		for (i = 0; keys[i] != NULL; i++) {
			value = u_map_get(map, keys[i]);
			len = snprintf(NULL, 0, "key is %s, value is %s", keys[i], value);
			line = o_malloc((size_t)(len + 1));
			snprintf(line, (size_t)(len + 1), "key is %s, value is %s", keys[i], value);
			if (to_return != NULL) {
				len = (int)(o_strlen(to_return) + o_strlen(line) + 1);
				to_return = o_realloc(to_return, (size_t)(len + 1));
				if (o_strlen(to_return) > 0) {
					strcat(to_return, "\n");
				}
			} else {
				to_return = o_malloc((o_strlen(line) + 1));
				to_return[0] = 0;
			}
			strcat(to_return, line);
			o_free(line);
		}
		return to_return;
	} else {
		return NULL;
	}
}

char * create_jwt_hs256(const char * username);

char * create_jwt_hs256_refresh(const char * username) {
	jwt_t * jwt = NULL;
	int ret;
	char * out = NULL;

	ret = jwt_new(&jwt);
	if (ret != 0) {
		fprintf(stderr, "Failed to create JWT object for refresh token.\n");
		return NULL;
	}

	ret = jwt_set_alg(jwt, JWT_ALG_HS256, JWT_SECRET_KEY, sizeof(JWT_SECRET_KEY));
	if (ret != 0) {
		fprintf(stderr, "Failed to set JWT algorithm for refresh token.\n");
		jwt_free(jwt);
		return NULL;
	}

	// Add the subject and issuer claims
	jwt_add_grant(jwt, "sub", username);
	jwt_add_grant(jwt, "iss", ISS);

	// Add an extra claim to denote that this is a refresh token
	jwt_add_grant(jwt, "type", "refresh");

	time_t now = time(NULL);
	jwt_add_grant_int(jwt, "iat", (long)now);

	// Set expiration time for the refresh token to 7 days
	long exp_time = now + (7 * 24 * 3600);
	jwt_add_grant_int(jwt, "exp", exp_time);

	out = jwt_encode_str(jwt);
	jwt_free(jwt);

	return out;
}

int callback_refresh(const struct _u_request * request, struct _u_response * response, void * user_data) {
	// Retrieve JSON payload from the request.
	json_t * json_payload = ulfius_get_json_body_request(request, NULL);
	if (!json_payload) {
		ulfius_set_json_body_response(response, 400, json_pack("{s:s}", "error", "Missing JSON payload"));
		return U_CALLBACK_COMPLETE;
	}

	// Extract the refresh token from the JSON payload.
	const char * refreshToken = NULL;
	if (json_unpack(json_payload, "{s:s}", "refreshToken", &refreshToken) != 0) {
		ulfius_set_json_body_response(response, 400, json_pack("{s:s}", "error", "Invalid JSON payload"));
		json_decref(json_payload);
		return U_CALLBACK_COMPLETE;
	}

	// Decode the refresh token.
	jwt_t * jwt = NULL;
	int ret = jwt_decode(&jwt, refreshToken, JWT_SECRET_KEY, sizeof(JWT_SECRET_KEY));
	if (ret != 0) {
		ulfius_set_json_body_response(response, 401, json_pack("{s:s}", "error", "Invalid refresh token"));
		json_decref(json_payload);
		return U_CALLBACK_COMPLETE;
	}

	// Check that the token has the correct type.
	const char * type = jwt_get_grant(jwt, "type");
	if (!type || strcmp(type, "refresh") != 0) {
		ulfius_set_json_body_response(response, 401, json_pack("{s:s}", "error", "Invalid token type"));
		jwt_free(jwt);
		json_decref(json_payload);
		return U_CALLBACK_COMPLETE;
	}

	// Check if the refresh token is expired.
	time_t now = time(NULL);
	time_t exp = (time_t)jwt_get_grant_int(jwt, "exp");
	if (exp < now) {
		ulfius_set_json_body_response(response, 401, json_pack("{s:s}", "error", "Refresh token expired"));
		jwt_free(jwt);
		json_decref(json_payload);
		return U_CALLBACK_COMPLETE;
	}

	// Extract the username (subject) from the refresh token.
	const char * username = jwt_get_grant(jwt, "sub");
	jwt_free(jwt);
	if (!username) {
		ulfius_set_json_body_response(response, 401, json_pack("{s:s}", "error", "Invalid refresh token payload"));
		json_decref(json_payload);
		return U_CALLBACK_COMPLETE;
	}

	// Create a new access token and refresh token.
	char * newAccessToken = create_jwt_hs256(username);
	char * newRefreshToken = create_jwt_hs256_refresh(username);
	if (!newAccessToken || !newRefreshToken) {
		ulfius_set_json_body_response(response, 500, json_pack("{s:s}", "error", "Failed to create new tokens"));
		if (newAccessToken) free(newAccessToken);
		if (newRefreshToken) free(newRefreshToken);
		json_decref(json_payload);
		return U_CALLBACK_COMPLETE;
	}

	// Return the new tokens in a JSON response.
	ulfius_set_json_body_response(response, 200, json_pack("{s:s, s:s}", "token", newAccessToken, "refreshToken", newRefreshToken));

	free(newAccessToken);
	free(newRefreshToken);
	json_decref(json_payload);
	return U_CALLBACK_COMPLETE;
}

int callback_login(const struct _u_request * request, struct _u_response * response, void * user_data) {
	json_t * json_payload = ulfius_get_json_body_request(request, NULL);
	if (!json_payload) {
		ulfius_set_json_body_response(response, 400, json_pack("{s:s}", "error", "Missing JSON payload"));
		return U_CALLBACK_COMPLETE;
	}

	const char * username = NULL;
	const char * password = NULL;
	if (json_unpack(json_payload, "{s:s, s:s}", "username", &username, "password", &password) != 0) {
		ulfius_set_json_body_response(response, 400, json_pack("{s:s}", "error", "Invalid JSON payload"));
		json_decref(json_payload);
		return U_CALLBACK_COMPLETE;
	}

	const char * sql = "SELECT hash FROM login WHERE user = ?;";
	sqlite3_stmt * stmt = NULL;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to prepare select statement: %s\n", sqlite3_errmsg(db));
		ulfius_set_json_body_response(response, 500, json_pack("{s:s}", "error", "Database error"));
		json_decref(json_payload);
		return U_CALLBACK_COMPLETE;
	}

	rc = sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to bind username: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		ulfius_set_json_body_response(response, 500, json_pack("{s:s}", "error", "Database error"));
		json_decref(json_payload);
		return U_CALLBACK_COMPLETE;
	}

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		sqlite3_finalize(stmt);
		ulfius_set_json_body_response(response, 401, json_pack("{s:s}", "error", "Invalid username or password"));
		json_decref(json_payload);
		return U_CALLBACK_COMPLETE;
	}

	const char * hash = (const char *)sqlite3_column_text(stmt, 0);

	if (bcrypt_checkpw(password, hash) != 0) {
		fprintf(stderr, "Password mismatch.\n");
		sqlite3_finalize(stmt);
		ulfius_set_json_body_response(response, 401, json_pack("{s:s}", "error", "Invalid username or password"));
		json_decref(json_payload);
		return U_CALLBACK_COMPLETE;
	}

	sqlite3_finalize(stmt);

	// Create the access token (short-lived)
	char * token = create_jwt_hs256(username);
	// Create the refresh token (longer expiration)
	char * refreshToken = create_jwt_hs256_refresh(username);
	if (!token || !refreshToken) {
		ulfius_set_json_body_response(response, 500, json_pack("{s:s}", "error", "Failed to create token(s)"));
		if (token) free(token);
		if (refreshToken) free(refreshToken);
		json_decref(json_payload);
		return U_CALLBACK_COMPLETE;
	}

	// Return both tokens in the response JSON
	ulfius_set_json_body_response(response, 200, json_pack("{s:s, s:s}", "token", token, "refreshToken", refreshToken));

	free(token);
	free(refreshToken);
	json_decref(json_payload);
	return U_CALLBACK_COMPLETE;
}

int callback_create_user(const struct _u_request * request, struct _u_response * response, void * user_data) {
	json_t * json_payload = ulfius_get_json_body_request(request, NULL);
	if (!json_payload) {
		ulfius_set_json_body_response(response, 400, json_pack("{s:s}", "error", "Missing JSON payload"));
		return U_CALLBACK_COMPLETE;
	}

	const char * username = NULL;
	const char * password = NULL;
	if (json_unpack(json_payload, "{s:s, s:s}", "username", &username, "password", &password) != 0) {
		ulfius_set_json_body_response(response, 400, json_pack("{s:s}", "error", "Invalid JSON payload"));
		json_decref(json_payload);
		return U_CALLBACK_COMPLETE;
	}

	const char * sql = "INSERT INTO login (user, hash) VALUES (?, ?);";
	sqlite3_stmt * stmt = NULL;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to prepare insert statement: %s\n", sqlite3_errmsg(db));
		ulfius_set_json_body_response(response, 500, json_pack("{s:s}", "error", "Database error"));
		json_decref(json_payload);
		return U_CALLBACK_COMPLETE;
	}

	char salt[BCRYPT_HASHSIZE];
	char hash[BCRYPT_HASHSIZE];
	int ret = bcrypt_gensalt(12, salt);
	if (ret != 0) {
		fprintf(stderr, "Failed to generate salt.\n");
		sqlite3_finalize(stmt);
		ulfius_set_json_body_response(response, 500, json_pack("{s:s}", "error", "Internal server error"));
		json_decref(json_payload);
		return U_CALLBACK_COMPLETE;
	}

	ret = bcrypt_hashpw(password, salt, hash);
	if (ret != 0) {
		fprintf(stderr, "Failed to hash password.\n");
		sqlite3_finalize(stmt);
		ulfius_set_json_body_response(response, 500, json_pack("{s:s}", "error", "Internal server error"));
		json_decref(json_payload);
		return U_CALLBACK_COMPLETE;
	}

	// Bind values into the SQL statement.
	rc = sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
	rc = sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_STATIC);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to bind values: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		ulfius_set_json_body_response(response, 500, json_pack("{s:s}", "error", "Database error"));
		json_decref(json_payload);
		return U_CALLBACK_COMPLETE;
	}

	// Execute the insert statement.
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		fprintf(stderr, "Failed to execute insert: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		ulfius_set_json_body_response(response, 500, json_pack("{s:s}", "error", "Database error"));
		json_decref(json_payload);
		return U_CALLBACK_COMPLETE;
	}

	// Finalize the statement and free allocated memory.
	sqlite3_finalize(stmt);

	ulfius_set_json_body_response(response, 201, json_pack("{s:s}", "message", "User created successfully"));
	json_decref(json_payload);
	return U_CALLBACK_COMPLETE;
}

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

int callback_auth(const struct _u_request * request, struct _u_response * response, void * user_data) {

	const char * auth = u_map_get(request->map_header, "Authorization");
	const char * token = NULL;

	printf("%s\n", print_map(request->map_header));

	if (strcmp(request->url_path, "/wsprint") == 0) {
		printf("Authentication for websocket endpoint.\n");
		token = u_map_get(request->map_url, "token");
		if (!token) {
			printf("Token missing.\n");
			ulfius_set_json_body_response(response, 401, json_pack("{s:s}", "error", "Token missing"));
			return U_CALLBACK_UNAUTHORIZED;
		}
		if (!verify_jwt_hs256(token)) {
			printf("Invalid token. %s \n", token);
			ulfius_set_json_body_response(response, 401, json_pack("{s:s}", "error", "Invalid token"));
			return U_CALLBACK_UNAUTHORIZED;
		}
		return U_CALLBACK_CONTINUE;
	}

	if (!auth) {
		printf("Authorization header missing.\n");
		ulfius_set_json_body_response(response, 401, json_pack("{s:s}", "error", "Authorization header missing"));
		return U_CALLBACK_UNAUTHORIZED;
	}

	if(strlen(auth) < 7) {
		printf("Invalid token.\n");
		ulfius_set_json_body_response(response, 401, json_pack("{s:s}", "error", "Invalid token"));
		return U_CALLBACK_UNAUTHORIZED;
	}

	// Expected format: "Bearer <token>"
	token = auth + 7;
	if (!verify_jwt_hs256(token)) {
		printf("Invalid token. verify\n");
		ulfius_set_json_body_response(response, 401, json_pack("{s:s}", "error", "Invalid token"));
		return U_CALLBACK_UNAUTHORIZED;
	}
	return U_CALLBACK_CONTINUE;
}
