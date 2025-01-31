import 'package:flutter/material.dart';

import 'dart:async';
import 'package:app/subnet.dart';
import 'package:app/node.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  // This widget is the root of your application.
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Flutter Demo',
      theme: ThemeData(
        // This is the theme of your application.
        //
        // TRY THIS: Try running your application with "flutter run". You'll see
        // the application has a purple toolbar. Then, without quitting the app,
        // try changing the seedColor in the colorScheme below to Colors.green
        // and then invoke "hot reload" (save your changes or press the "hot
        // reload" button in a Flutter-supported IDE, or press "r" if you used
        // the command line to start the app).
        //
        // Notice that the counter didn't reset back to zero; the application
        // state is not lost during the reload. To reset the state, use hot
        // restart instead.
        //
        // This works for code too, not just values: Most code changes can be
        // tested with just a hot reload.
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.deepPurpleAccent),
        useMaterial3: true,
      ),
      home: const MyHomePage(title: 'Flutter Demo Home Page'),
    );
  }
}

class MyHomePage extends StatefulWidget {
  const MyHomePage({super.key, required this.title});

  // This widget is the home page of your application. It is stateful, meaning
  // that it has a State object (defined below) that contains fields that affect
  // how it looks.

  // This class is the configuration for the state. It holds the values (in this
  // case the title) provided by the parent (in this case the App widget) and
  // used by the build method of the State. Fields in a Widget subclass are
  // always marked "final".

  final String title;

  @override
  State<MyHomePage> createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> {
  late Future<List<Subnet>> _futureSubnets;

  @override
  void initState() {
    super.initState();
    _futureSubnets = fetchSubnets();
  }

@override
Widget build(BuildContext context) {
  return Scaffold(
    appBar: AppBar(
      title: const Text('Subnets'),
    ),
    body: FutureBuilder<List<Subnet>>(
      future: _futureSubnets,
      builder: (context, snapshot) {
        if (snapshot.connectionState == ConnectionState.waiting) {
          return const Center(child: CircularProgressIndicator());
        }
        if (snapshot.hasError) {
          return Center(child: Text('Error: ${snapshot.error}'));
        }
        if (snapshot.hasData && snapshot.data!.isNotEmpty) {
          final subnets = snapshot.data!;

          return SingleChildScrollView(
            scrollDirection: Axis.horizontal, // Allows horizontal scroll if needed
            child: SizedBox(
              width: MediaQuery.of(context).size.width,

            child: DataTable(
              showCheckboxColumn: false,
              columns: const [
                DataColumn(label: Text('Name')),
                DataColumn(label: Text('Node')),
                DataColumn(label: Text('Mask')),
              ],
              rows: subnets.map((subnet) {
                return DataRow(
                onSelectChanged: (selected) {
                if (selected == true) {
                  Navigator.push(
                    context,
                    MaterialPageRoute(
                      builder: (context) => SubnetDetailScreen(subnet: subnet),
                    ),
                  );
                }
              },
                  cells: [
                    DataCell(Text(subnet.name)),
                    DataCell(Text(subnet.node.toString())),
                    DataCell(Text(subnet.mask.toString())),
                  ],
                );
              }).toList(),
              ),
            )
          );
        }

        return const Center(child: Text('No subnets found.'));
      },
    ),
  );
}
}

class SubnetDetailScreen extends StatefulWidget {
  final Subnet subnet;

  const SubnetDetailScreen({
    Key? key,
    required this.subnet,
  }) : super(key: key);

  @override
  State<SubnetDetailScreen> createState() => _SubnetDetailScreenState();
}

class _SubnetDetailScreenState extends State<SubnetDetailScreen> {
  late Future<List<Node>> _futureNodes;

  @override
  void initState() {
    super.initState();
    _futureNodes = fetchNodes();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
        appBar: AppBar(
          title: Text('Nodes for ${widget.subnet.name}'),
        ),
        body: FutureBuilder<List<Node>>(
          future: _futureNodes,
          builder: (context, snapshot) {
            // 1. Loading state
            if (snapshot.connectionState == ConnectionState.waiting) {
              return const Center(child: CircularProgressIndicator());
            }

            // 2. Error state
            if (snapshot.hasError) {
              return Center(
                child: Text('Error: ${snapshot.error}'),
              );
            }

            // 3. Data loaded
            if (snapshot.hasData && snapshot.data!.isNotEmpty) {
              final nodes = snapshot.data!;
              return SingleChildScrollView(
                scrollDirection: Axis.horizontal, // Allows horizontal scroll if needed
                child: SizedBox(
                  width: MediaQuery.of(context).size.width,
                  child: SingleChildScrollView(
                    scrollDirection: Axis.vertical,
                    child: DataTable(
                  showCheckboxColumn: false,
                  columns: const [
                    DataColumn(label: Text('Hostname')),
                    DataColumn(label: Text('Node')),
                  ],
                  rows: nodes.map((node) {
                    return DataRow(
                    onSelectChanged: (selected) {
                    if (selected == true) {
                      //Navigator.push(
                        //context,
                        //MaterialPageRoute(
                          //builder: (context) => SubnetDetailScreen(node: node),
                        //),
                      //);
                    }
                  },
                      cells: [
                        DataCell(Text(node.hostname)),
                        DataCell(Text(node.node.toString())),
                      ],
                    );
                  }).toList(),
                  ),
                  ),
                )
              );
            }

            // 4. If the list is empty
            return const Center(child: Text('No nodes found.'));
          },
        ));
  }
}

class NodeDetailScreen extends StatefulWidget {
  final Node node;

  const NodeDetailScreen({
    Key? key,
    required this.node,
  }) : super(key: key);

  @override
  State<NodeDetailScreen> createState() => _NodeDetailScreenState();
}

class _NodeDetailScreenState extends State<NodeDetailScreen> {
  late Future<List<Param>> _futureParams;

  @override
  void initState() {
    super.initState();
    _futureParams = fetchParams();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
        appBar: AppBar(
          title: Text('Parameters for ${widget.node.hostname}'),
        ),
        body: FutureBuilder<List<Param>>(
          future: _futureParams,
          builder: (context, snapshot) {
            // 1. Loading state
            if (snapshot.connectionState == ConnectionState.waiting) {
              return const Center(child: CircularProgressIndicator());
            }

            // 2. Error state
            if (snapshot.hasError) {
              return Center(
                child: Text('Error: ${snapshot.error}'),
              );
            }

            // 3. Data loaded
            if (snapshot.hasData && snapshot.data!.isNotEmpty) {
              final params = snapshot.data!;
              return SingleChildScrollView(
                scrollDirection: Axis.horizontal, // Allows horizontal scroll if needed
                child: SizedBox(
                  width: MediaQuery.of(context).size.width,
                  child: SingleChildScrollView(
                    scrollDirection: Axis.vertical,
                    child: DataTable(
                  showCheckboxColumn: false,
                  columns: const [
                    DataColumn(label: Text('xxxx')),
                    DataColumn(label: Text('xxxx')),
                  ],
                  rows: params.map((param) {
                    return DataRow(
                    onSelectChanged: (selected) {
                    if (selected == true) {
                      //Navigator.push(
                        //context,
                        //MaterialPageRoute(
                          //builder: (context) => SubnetDetailScreen(node: node),
                        //),
                      //);
                    }
                  },
                      cells: [
                        DataCell(Text(param.)),
                        DataCell(Text(param.node.toString())),
                      ],
                    );
                  }).toList(),
                  ),
                  ),
                )
              );
            }

            // 4. If the list is empty
            return const Center(child: Text('No parameters found.'));
          },
        ));
  }
}
