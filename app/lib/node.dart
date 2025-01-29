import 'package:http/http.dart' as http;
import 'dart:async';
import 'dart:convert';
import 'dart:io';

Future<List<Node>> fetchNodes() async {

  await Future.delayed(const Duration(seconds: 1));
  final response = await http.get(
    Uri.parse('https://raw.githubusercontent.com/edvardxyz/tp/refs/heads/master/app/apitest/node'),
    // Send authorization headers to the backend.
    headers: {
      HttpHeaders.authorizationHeader: 'Basic your_api_token_here',
    },
  );
  if(response.statusCode == 200){
    final List<dynamic> data = jsonDecode(response.body);
    return data.map((json) => Node.fromJson(json)).toList();
  } else {
    throw Exception('Failed to load users');
  }
}

class Node {
  final int id;
  final String hostname;
  final int node;
  final int created;

  const Node({
    required this.id,
    required this.hostname,
    required this.node,
    required this.created
  });

  factory Node.fromJson(Map<String, dynamic> json) {
    return switch (json) {
      {
        'id': int id,
        'hostname': String name,
        'node': int node,
        'created': int created,
      } =>
        Node(
          id: id,
          hostname: hostname,
          node: node,
          created: created,
        ),
      _ => throw const FormatException('Failed to load album.'),
    };
  }
}