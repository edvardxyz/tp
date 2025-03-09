import 'package:flutter/material.dart';

import 'dart:async';
import 'package:app/subnet.dart';
import 'package:app/node.dart';
import 'package:app/param.dart';

import 'package:http/http.dart' as http;
import 'dart:convert';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';

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
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.deepPurpleAccent),
        useMaterial3: true,
      ),
      home: DefaultTabController(
        length: 2, 
        child: Scaffold(
          appBar: AppBar(
            title: const Text('Login & Register'),
            bottom: const TabBar(
              tabs: [
                Tab(text: 'Login'),
                Tab(text: 'Register'),
              ],
            ),
          ),
          body: TabBarView(
            children: [
              LoginScreen(),
              RegisterScreen(),
            ],
          ),
        ),
        ),
    );
  }
}

class LoginScreen extends StatelessWidget {
  final TextEditingController usernameController = TextEditingController();
  final TextEditingController passwordController = TextEditingController();
  final FlutterSecureStorage secureStorage = const FlutterSecureStorage();

  Future<void> loginUser(BuildContext context) async {
    final String username = usernameController.text;
    final String password = passwordController.text;

    final Uri url = Uri.parse('http://localhost:8888/login');

    try {
      final response = await http.post(
        url,
        headers: {
          'Content-Type': 'application/json',
        },
        body: jsonEncode(<String, String>{
          'username': username,
          'password': password,
        }),
      );

      if (response.statusCode == 200 || response.statusCode == 201) {
        print('User login successfully!');

        final Map<String, dynamic> responseData = jsonDecode(response.body);
        final String token = responseData['token'];
        final String refreshToken = responseData['refreshToken'];

        await secureStorage.write(key: 'token', value: token);
        await secureStorage.write(key: 'refreshToken', value: refreshToken);

        if(context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(
              content: Text('User login successfully!'),
            ),
          );

        // Navigate to the subnet window (MyHomePage)
        Navigator.pushReplacement(
          context,
          MaterialPageRoute(builder: (context) => MyHomePage(title: 'Subnets')),
        );

        }

      } else {
        print('Login failed with status: ${response.statusCode}');
        if(context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text('Login failed! ${response.body}'),
            ),
          );
        }
      }
    } catch (e) {
      print('An error occurred: $e');
      if(context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('An error occurred: $e'),
          ),
        );
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Card(
        margin: EdgeInsets.all(20.0),
        child: Padding(
          padding: EdgeInsets.all(16.0),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: <Widget>[
              TextField(
                controller: usernameController,
                decoration: InputDecoration(labelText: 'Username'),
              ),
              SizedBox(height: 10),
              TextField(
                controller: passwordController,
                decoration: InputDecoration(labelText: 'Password'),
                obscureText: true,
              ),
              SizedBox(height: 20),
              ElevatedButton(
                onPressed: () {
                  loginUser(context);
                },
                child: Text('Login'),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class RegisterScreen extends StatelessWidget {
  final TextEditingController usernameController = TextEditingController();
  final TextEditingController passwordController = TextEditingController();
  Future<void> registerUser(BuildContext context) async {
    final String username = usernameController.text;
    final String password = passwordController.text;

    final Uri url = Uri.parse('http://localhost:8888/register');

    try {
      final response = await http.post(
        url,
        headers: {
          'Content-Type': 'application/json',
        },
        body: jsonEncode(<String, String>{
          'username': username,
          'password': password,
        }),
      );

      if (response.statusCode == 200 || response.statusCode == 201) {
        print('User registered successfully!');
        if(context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(
              content: Text('User registered successfully!'),
            ),
          );
        }
      } else {
        print('Registration failed with status: ${response.statusCode}');
        if(context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text('Registration failed! ${response.body}'),
            ),
          );
        }
      }
    } catch (e) {
      print('An error occurred: $e');
      if(context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('An error occurred: $e'),
          ),
        );
      }
    }
  }


  @override
  Widget build(BuildContext context) {
    return Center(
      child: Card(
        margin: EdgeInsets.all(20.0),
        child: Padding(
          padding: EdgeInsets.all(16.0),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: <Widget>[
              TextField(
                controller: usernameController,
                decoration: InputDecoration(labelText: 'Username'),
              ),
              SizedBox(height: 10),
              TextField(
                controller: passwordController,
                decoration: InputDecoration(labelText: 'Password'),
                obscureText: true,
              ),
              SizedBox(height: 20),
              ElevatedButton(
                onPressed: () {
                  registerUser(context);
                },
                child: Text('Signup'),
              ),
            ],
          ),
        ),
      ),
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
                scrollDirection:
                    Axis.horizontal, // Allows horizontal scroll if needed
                child: SizedBox(
                  width: MediaQuery.of(context).size.width,
                  child: DataTable(
                    showCheckboxColumn: false,
                    columns: const [
                      DataColumn(label: Text('Name')),
                      DataColumn(label: Text('Subnet')),
                    ],
                    rows: subnets.map((subnet) {
                      return DataRow(
                        onSelectChanged: (selected) {
                          if (selected == true) {
                            Navigator.push(
                              context,
                              MaterialPageRoute(
                                builder: (context) =>
                                    SubnetDetailScreen(subnet: subnet),
                              ),
                            );
                          }
                        },
                        cells: [
                          DataCell(Text(subnet.name)),
                          DataCell(Text(
                              '${subnet.node.toString()} / ${subnet.mask.toString()}')),
                        ],
                      );
                    }).toList(),
                  ),
                ));
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
    super.key,
    required this.subnet,
  });

  @override
  State<SubnetDetailScreen> createState() => _SubnetDetailScreenState();
}

class _SubnetDetailScreenState extends State<SubnetDetailScreen> {
  late Future<List<Node>> _futureNodes;

  @override
  void initState() {
    super.initState();
    _futureNodes = fetchNodes(widget.subnet);
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
                  scrollDirection:
                      Axis.horizontal, // Allows horizontal scroll if needed
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
                                Navigator.push(
                                  context,
                                  MaterialPageRoute(
                                    builder: (context) =>
                                        NodeDetailScreen(node: node),
                                  ),
                                );
                              }
                            },
                            cells: [
                              DataCell(Text(node.name)),
                              DataCell(Text(node.node.toString())),
                            ],
                          );
                        }).toList(),
                      ),
                    ),
                  ));
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
    super.key,
    required this.node,
  });

  @override
  State<NodeDetailScreen> createState() => _NodeDetailScreenState();
}

class _NodeDetailScreenState extends State<NodeDetailScreen> {
  late Future<List<Param>> _futureParams;

  @override
  void initState() {
    super.initState();
    _futureParams = loadAndCombineParams(widget.node);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
        appBar: AppBar(
          title: Text('Parameters for ${widget.node.name}'),
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
                  scrollDirection:
                      Axis.horizontal, // Allows horizontal scroll if needed
                  child: SizedBox(
                    width: MediaQuery.of(context).size.width,
                    child: SingleChildScrollView(
                      scrollDirection: Axis.vertical,
                      child: DataTable(
                        showCheckboxColumn: false,
                        columns: const [
                          DataColumn(label: Text('Name')),
                          DataColumn(label: Text('id')),
                          DataColumn(label: Text('value')),
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
                              DataCell(Text(param.name)),
                              DataCell(Text(param.paramId.toString())),
                              DataCell(Text(param.value.toString())),
                            ],
                          );
                        }).toList(),
                      ),
                    ),
                  ));
            }

            // 4. If the list is empty
            return const Center(child: Text('No parameters found.'));
          },
        ));
  }
}
