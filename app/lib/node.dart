import 'package:http/http.dart' as http;
import 'dart:async';
import 'dart:convert';

import 'package:app/subnet.dart';

Future<List<Node>> fetchNodes(Subnet subnet) async {
  // calculate the number of nodes in the subnet.
  final int numberOfNodes = 1 << (14 - subnet.mask);
  
  List<Future<Node?>> futures = [];
  for (int i = 0; i < numberOfNodes; i++) {
    int nodeAddress = subnet.node + i;
    futures.add(_fetchNode(nodeAddress));
  }
  final List<Node?> results = await Future.wait(futures);
  return results.whereType<Node>().toList();
}

Future<Node?> _fetchNode(int nodeAddress) async {
  final response = await http.get(
    Uri.parse('http://localhost:8888/node/$nodeAddress'),
  );
  if (response.statusCode == 200) {
    final decoded = jsonDecode(response.body);
    // Handle if the response is a list instead of a map.
    if (decoded is List) {
      if (decoded.isNotEmpty) {
        return Node.fromJson(decoded.first);
      } else {
        return null;
      }
    } else if (decoded is Map<String, dynamic>) {
      return Node.fromJson(decoded);
    } else {
      return null;
    }
  } else {
    return null;
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