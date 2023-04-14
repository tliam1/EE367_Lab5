enum ValidEntry {
	NotValid,
	Valid
};

struct switch_port_forwarding {
	enum ValidEntry valid;
	char dst;
	int port;
};

void switch_main(int switch_id);
