import 'package:http/http.dart' as http;
import 'dart:async';
import 'dart:convert';

import 'package:app/tokenhttp.dart';

 Future<List<Subnet>> fetchSubnets() async {
  final response = await performAuthenticatedRequest(
    (headers) => http.get(
      Uri.parse('http://localhost:8888/subnet'),
      headers: headers,
    ),
  );

  if (response.statusCode == 200) {
    final List<dynamic> data = jsonDecode(response.body);
    return data.map((json) => Subnet.fromJson(json)).toList();
  } else {
    throw Exception('Failed to load subnets');
  }
}

class Subnet {
  final String name;
  final int id;
  final int node;
  final int mask;
  final int created;

  const Subnet({
    required this.name,
    required this.id,
    required this.node,
    required this.mask,
    required this.created
  });

  factory Subnet.fromJson(Map<String, dynamic> json) {
    return switch (json) {
      {
        'name': String name,
        'id': int id,
        'node': int node,
        'mask': int mask,
        'created': int created,
      } =>
        Subnet(
          name: name,
          id: id,
          node: node,
          mask: mask,
          created: created,
        ),
      _ => throw const FormatException('Failed to load subnets.'),
    };
  }
}