import 'package:http/http.dart' as http;
import 'dart:async';
import 'dart:convert';
import 'dart:io';

Future<List<Node>> fetchNodes() async {
  final response = await http.get(
    Uri.parse('http://localhost:8888/node'),
    // Send authorization headers to the backend.
    // headers: {
    //   HttpHeaders.authorizationHeader: 'Basic your_api_token_here',
    // },
  );
  if(response.statusCode == 200){
    final List<dynamic> data = jsonDecode(response.body);
    return data.map((json) => Node.fromJson(json)).toList();
  } else {
    throw Exception('Failed to load node');
  }
}

class Node {
  final String name;
  final int node;

  const Node({
    required this.name,
    required this.node,
  });

  factory Node.fromJson(Map<String, dynamic> json) {
    return switch (json) {
      {
        'name': String name,
        'node': int node,
      } =>
        Node(
          name: name,
          node: node,
        ),
      _ => throw const FormatException('Failed to load node.'),
    };
  }
}