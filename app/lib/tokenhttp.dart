import 'dart:convert';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:http/http.dart' as http;
import 'package:app/constants.dart';

final FlutterSecureStorage secureStorage = const FlutterSecureStorage();

Future<Map<String, String>> getAuthHeaders() async {
  final token = await secureStorage.read(key: 'token');
  return {
    'Content-Type': 'application/json',
    'Authorization': 'Bearer $token',
  };
}

Future<bool> refreshToken() async {
  final refreshToken = await secureStorage.read(key: 'refreshToken');
  if (refreshToken == null) return false;

  final response = await http.post(
    Uri.parse('https://$serverUrl/refresh'),
    headers: {
      'Content-Type': 'application/json',
    },
    body: jsonEncode({
      'refreshToken': refreshToken,
    }),
  );

  if (response.statusCode == 200) {
    final data = jsonDecode(response.body);
    // Save the new tokens.
    await secureStorage.write(key: 'token', value: data['token']);
    await secureStorage.write(key: 'refreshToken', value: data['refreshToken']);
    return true;
  } else {
    return false;
  }
}

Future<http.Response> performAuthenticatedRequest(
  Future<http.Response> Function(Map<String, String> headers) requestFunction,
) async {
  var headers = await getAuthHeaders();
  var response = await requestFunction(headers);

  // If unauthorized, try to refresh the token.
  if (response.statusCode == 401) {
    bool success = await refreshToken();
    if (success) {
      headers = await getAuthHeaders();
      response = await requestFunction(headers);
    }
  }
  return response;
}
