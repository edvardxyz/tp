import 'package:http/http.dart' as http;
import 'dart:async';
import 'dart:convert';
import 'dart:io';

Future<List<Param>> fetchParams() async {
  final response = await http.get(
    Uri.parse('https://raw.githubusercontent.com/edvardxyz/tp/refs/heads/master/app/apitest/param'),
    // Send authorization headers to the backend.
    // headers: {
    //   HttpHeaders.authorizationHeader: 'Basic your_api_token_here',
    // },
  );
  if(response.statusCode == 200){
    final List<dynamic> data = jsonDecode(response.body);
    return data.map((json) => Param.fromJson(json)).toList();
  } else {
    throw Exception('Failed to load parameter');
  }
}

class Param {
  final int id;
  final int paramId;
  final String name;

  // A List of numbers (int or double)
  final List<num> value;

  const Param({
    required this.id,
    required this.paramId,
    required this.name,
    required this.value,
  });

  factory Param.fromJson(Map<String, dynamic> json) {
    // Safely parse "value" from the JSON.
    // It might be missing, or might be a list of int/double.
    final rawValue = json['value'];
    List<num> valueList = [];

    if (rawValue is List) {
      // Convert each element in the list to num (int or double)
      valueList = rawValue.map<num>((item) {
        if (item is num) {
          return item; // Already int or double
        } else {
          // Attempt to parse if it's something else (e.g., String)
          return num.parse(item.toString());
        }
      }).toList();
    }

    return Param(
      id: json['id'] as int,
      paramId: json['param_id'] as int,
      name: json['name'] as String,
      value: valueList,
    );
  }
}
