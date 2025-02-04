project('c-api', 'c',
  version: '0.1.0',
  default_options: ['warning_level=3', 'c_std=gnu2x'],
  subproject_dir: 'lib',
  default_options: [
	'csp:packet_padding_bytes=42', 
	'csp:buffer_count=1000',
	'csp:buffer_size=2048',
	'csp:conn_max=20',
	'csp:conn_rxqueue_len=1000',
	'csp:qfifo_len=1000',
	'csp:rdp_max_window=1000',
	'csp:port_max_bind=16',
	'csp:use_rtable=true',
	'param:commands=false',
	'param:scheduler=false',
        'param:list_dynamic=true',
  ]
)

zmq_dep = dependency('libzmq', required: true)
jansson_dep = dependency('jansson', required: true)
ulfius_dep = dependency('libulfius', required: true)
sqlite_dep = dependency('sqlite3', required: true)
jwt_dep = dependency('libjwt', required: true)
csp_dep = dependency('csp', fallback: ['csp', 'csp_dep'], required: true)
param_dep = dependency('param', fallback: ['param', 'param_dep'], required: true)

executable('c-api',
  [
    'src/main.c',
    'src/db.c',
    'src/subnet.c',
    'src/node.c',
    'src/sniffer.c',
    'src/param.c',
    #'src/token.c',
  ],
  dependencies: [ulfius_dep, sqlite_dep, jwt_dep, jansson_dep, csp_dep, param_dep, zmq_dep]
)

