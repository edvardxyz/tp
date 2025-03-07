CREATE TABLE node (
	node INTEGER NOT NULL,
	name TEXT NOT NULL,
	created INTEGER NOT NULL DEFAULT (strftime('%s','now'))
);

CREATE TABLE subnet (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	name TEXT NOT NULL,
	node INTEGER NOT NULL,
	mask INTEGER NOT NULL,
	created INTEGER NOT NULL DEFAULT (strftime('%s','now'))
);

CREATE TABLE param (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	param_id INTEGER NOT NULL,
	name TEXT NOT NULL,
	size INTEGER NOT NULL,
	mask INTEGER,
	vmem INTEGER,
	type INTEGER NOT NULL,
	created INTEGER NOT NULL DEFAULT (strftime('%s','now')),
	UNIQUE (param_id, name)
);

CREATE TABLE param_node (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	param_id INTEGER NOT NULL,
	node_id INTEGER NOT NULL,
	created INTEGER NOT NULL DEFAULT (strftime('%s','now')),
	FOREIGN KEY(param_id) REFERENCES param(id),
	FOREIGN KEY(node_id) REFERENCES node(node),
	UNIQUE (param_id, node_id)
);

CREATE TABLE param_int (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	value INTEGER NOT NULL,
	idx INTEGER NOT NULL,
	param_node_id INTEGER NOT NULL,
	time_sec INTEGER NOT NULL,
	created INTEGER NOT NULL DEFAULT (strftime('%s','now')),
	FOREIGN KEY(param_node_id) REFERENCES param_node(id)
);

CREATE TABLE param_real (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	value REAL NOT NULL,
	idx INTEGER NOT NULL,
	param_node_id INTEGER NOT NULL,
	time_sec INTEGER NOT NULL,
	created INTEGER NOT NULL DEFAULT (strftime('%s','now')),
	FOREIGN KEY(param_node_id) REFERENCES param_node(id)
);

CREATE TABLE pushed (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	node_id INTEGER NOT NULL,
	val_id INTEGER NOT NULL,
	created INTEGER NOT NULL DEFAULT (strftime('%s','now')),
	FOREIGN KEY(node_id) REFERENCES node(node)
);

CREATE TABLE contact (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	node_to_id INTEGER NOT NULL,
	node_from_id INTEGER NOT NULL,
	time_sec INTEGER NOT NULL,
	FOREIGN KEY(node_to_id) REFERENCES node(node),
	FOREIGN KEY(node_from_id) REFERENCES node(node)
);

CREATE TABLE login (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	user TEXT NOT NULL,
	hash TEXT NOT NULL,
	created INTEGER NOT NULL DEFAULT (strftime('%s','now')),
	UNIQUE (user)
);



CREATE INDEX idx_param_node_param ON param_node(id);
CREATE INDEX idx_param_int_param_node ON param_int(param_node_id);
CREATE INDEX idx_param_real_param_node ON param_real(param_node_id);

INSERT INTO subnet (name, node, mask) VALUES ('sub1', 0, 8);
INSERT INTO subnet (name, node, mask) VALUES ('sub2', 64, 8);
INSERT INTO subnet (name, node, mask) VALUES ('sub3', 128, 8);
