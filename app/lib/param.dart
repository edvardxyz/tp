import 'package:http/http.dart' as http;
import 'dart:async';
import 'dart:convert';
import 'package:app/node.dart';

class Param {
  final int paramId;
  final String name;
  final int size;
  final List<num> value;

  const Param({
    required this.paramId,
    required this.name,
    required this.size,
    required this.value,
  });

  factory Param.fromJson(Map<String, dynamic> json) {
    final rawValue = json['value'];
    List<num> valueList = [];
    if (rawValue is List) {
      valueList = rawValue.map<num>((item) {
        if (item is num) return item;
        return num.parse(item.toString());
      }).toList();
    }
    return Param(
      paramId: json['param_id'] as int,
      name: json['name'] as String,
      size: json['size'] as int,
      value: valueList,
    );
  }

  Param copyWith({List<num>? value}) {
    return Param(
      paramId: paramId,
      name: name,
      size: size,
      value: value ?? this.value,
    );
  }
}

Future<List<Param>> fetchParams(Node node) async {
  final response = await http.get(
    Uri.parse('http://localhost:8888/node/${node.node}/param'),
  );
  if (response.statusCode == 200) {
    final List<dynamic> data = jsonDecode(response.body);
    return data.map((json) => Param.fromJson(json)).toList();
  } else {
    throw Exception('Failed to load parameters');
  }
}

Future<List<num>> fetchParamValues(Node node, Param param) async {
  final url =
      'http://localhost:8888/node/${node.node}/param/${param.paramId}/value/${param.size}';
  final response = await http.get(Uri.parse(url));
  if (response.statusCode == 200) {
    final List<dynamic> data = jsonDecode(response.body);
    return data.map<num>((json) => json['value'] as num).toList();
  } else {
    return [];
  }
}

Future<List<Param>> loadAndCombineParams(Node node) async {
  List<Param> params = await fetchParams(node);
  List<Param> combined = [];
  for (var param in params) {
    List<num> values = await fetchParamValues(node, param);
    combined.add(param.copyWith(value: values));
  }
  return combined;
}