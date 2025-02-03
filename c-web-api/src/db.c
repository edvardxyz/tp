#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sqlite3.h>
#include <jansson.h>

#define DB_PATH "../database.db"
sqlite3 * db = NULL;

int init_db() {
	int rc;

	rc = sqlite3_open_v2(DB_PATH, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to open db con: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}
	return 0;
}

/**
 * fetch_query_results:
 *   Executes a given SQL query with optional parameter bindings
 *   (using a format string similar to printf) and returns a JSON array
 *   containing one JSON object per result row.
 *
 *   Supported format specifiers in `fmt`:
 *     %s: const char *
 *     %d: int
 *     %l: long
 *     %f: double
 *
 * Returns:
 *   A json_t* (which is a JSON array) on success or NULL on failure.
 */
json_t * fetch_query_results(const char * sql, const char * fmt, ...) {
	sqlite3_stmt * stmt = NULL;
	int rc;

	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
		return NULL;
	}

	/* Bind parameters based on the format string */
	va_list args;
	va_start(args, fmt);
	int param_index = 1;  // SQLite parameters are 1-indexed
	size_t fmt_len = strlen(fmt);
	for (size_t i = 0; i < fmt_len; i++) {
		if (fmt[i] == '%') {
			i++;  // Move to the specifier character
			switch (fmt[i]) {
				case 's': {
					const char * str_val = va_arg(args, const char *);
					sqlite3_bind_text(stmt, param_index++, str_val, -1, SQLITE_TRANSIENT);
				} break;
				case 'd': {
					int int_val = va_arg(args, int);
					sqlite3_bind_int(stmt, param_index++, int_val);
				} break;
				case 'l': {
					long long_val = va_arg(args, long);
					sqlite3_bind_int64(stmt, param_index++, (sqlite3_int64)long_val);
				} break;
				case 'f': {
					double dbl_val = va_arg(args, double);
					sqlite3_bind_double(stmt, param_index++, dbl_val);
				} break;
				default:
					fprintf(stderr, "Unsupported format specifier: %c\n", fmt[i]);
					break;
			}
		}
	}
	va_end(args);

	/* Create a JSON array to hold all result rows */
	json_t * json_arr = json_array();
	if (!json_arr) {
		fprintf(stderr, "Failed to create JSON array\n");
		sqlite3_finalize(stmt);
		return NULL;
	}

	int col_count = sqlite3_column_count(stmt);
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		/* Create a JSON object for this row */
		json_t * row_obj = json_object();
		if (!row_obj) {
			fprintf(stderr, "Failed to create JSON object\n");
			json_decref(json_arr);
			sqlite3_finalize(stmt);
			return NULL;
		}

		for (int i = 0; i < col_count; i++) {
			const char * col_name = sqlite3_column_name(stmt, i);
			int col_type = sqlite3_column_type(stmt, i);
			json_t * value = NULL;

			switch (col_type) {
				case SQLITE_INTEGER: {
					sqlite3_int64 ival = sqlite3_column_int64(stmt, i);
					value = json_integer(ival);
				} break;
				case SQLITE_FLOAT: {
					double fval = sqlite3_column_double(stmt, i);
					value = json_real(fval);
				} break;
				case SQLITE_TEXT: {
					const unsigned char * text = sqlite3_column_text(stmt, i);
					value = json_string((const char *)text);
				} break;
				case SQLITE_NULL: {
					value = json_null();
				} break;
				default: {
					/* For blobs or unknown types, try to treat as text */
					const unsigned char * blob = sqlite3_column_text(stmt, i);
					value = json_string((const char *)blob);
				} break;
			}

			json_object_set_new(row_obj, col_name, value);
		}

		/* Append the row object to the JSON array */
		json_array_append_new(json_arr, row_obj);
	}

	if (rc != SQLITE_DONE) {
		fprintf(stderr, "Error stepping through rows: %s\n", sqlite3_errmsg(db));
		json_decref(json_arr);
		sqlite3_finalize(stmt);
		return NULL;
	}

	sqlite3_finalize(stmt);

	return json_arr;
}
