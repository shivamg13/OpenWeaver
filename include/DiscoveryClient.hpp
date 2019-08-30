#ifndef MARLIN_BEACON_DISCOVERYCLIENT_HPP
#define MARLIN_BEACON_DISCOVERYCLIENT_HPP

#include <marlin/net/udp/UdpTransportFactory.hpp>
#include <map>

namespace marlin {
namespace beacon {

template<typename DiscoveryClientDelegate>
class DiscoveryClient {
private:
	using BaseTransportFactory = net::UdpTransportFactory<
		DiscoveryClient<DiscoveryClientDelegate>,
		DiscoveryClient<DiscoveryClientDelegate>
	>;
	using BaseTransport = net::UdpTransport<
		DiscoveryClient<DiscoveryClientDelegate>
	>;

	BaseTransportFactory f;

	// Discovery protocol
	void send_DISCPROTO(BaseTransport &transport);
	void did_recv_DISCPROTO(BaseTransport &transport);
	void send_LISTPROTO(BaseTransport &transport);
	void did_recv_LISTPROTO(BaseTransport &transport, net::Buffer &&packet);

	void send_DISCPEER(BaseTransport &transport);
	void did_recv_LISTPEER(BaseTransport &transport, net::Buffer &&packet);

	void send_HEARTBEAT(BaseTransport &transport);

	BaseTransport *beacon = nullptr;

	uv_timer_t beacon_timer;
	static void beacon_timer_cb(uv_timer_t *handle);

	uv_timer_t heartbeat_timer;
	static void heartbeat_timer_cb(uv_timer_t *handle);

public:
	// Listen delegate
	bool should_accept(net::SocketAddress const &addr);
	void did_create_transport(BaseTransport &transport);

	// Transport delegate
	void did_dial(BaseTransport &transport);
	void did_recv_packet(BaseTransport &transport, net::Buffer &&packet);
	void did_send_packet(BaseTransport &transport, net::Buffer &&packet);

	DiscoveryClient(net::SocketAddress const &addr);

	DiscoveryClientDelegate *delegate;
	bool is_discoverable = false;

	void start_discovery(net::SocketAddress const &beacon_addr);
};


// Impl

//---------------- Discovery protocol functions begin ----------------//

template<typename DiscoveryClientDelegate>
void DiscoveryClient<DiscoveryClientDelegate>::send_DISCPROTO(
	BaseTransport &transport
) {
	char *message = new char[2] {0, 0};

	net::Buffer p(message, 2);
	transport.send(std::move(p));
}

template<typename DiscoveryClientDelegate>
void DiscoveryClient<DiscoveryClientDelegate>::did_recv_DISCPROTO(
	BaseTransport &transport
) {
	SPDLOG_DEBUG("DISCPROTO <<< {}", transport.dst_addr.to_string());

	send_LISTPROTO(transport);
}

template<typename DiscoveryClientDelegate>
void DiscoveryClient<DiscoveryClientDelegate>::send_LISTPROTO(
	BaseTransport &transport
) {
	auto protocols = delegate->get_protocols();

	net::Buffer p(
		new char[3 + protocols.size()*8] {0, 1, (char)protocols.size()},
		3 + protocols.size()*8
	);

	auto iter = protocols.begin();
	for(
		auto i = 3;
		iter != protocols.end();
		iter++, i += 8
	) {
		auto [protocol, version, port] = *iter;

		p.write_uint32_be(i, protocol);
		p.write_uint16_be(i+4, version);
		p.write_uint16_be(i+6, port);
	}

	transport.send(std::move(p));
}

template<typename DiscoveryClientDelegate>
void DiscoveryClient<DiscoveryClientDelegate>::did_recv_LISTPROTO(
	BaseTransport &transport,
	net::Buffer &&packet
) {
	SPDLOG_DEBUG("LISTPROTO <<< {}", transport.dst_addr.to_string());

	uint8_t num_proto = packet.read_uint8(2);
	packet.cover(3);
	for(uint8_t i = 0; i < num_proto; i++) {
		uint32_t protocol = packet.read_uint32_be(8*i);
		uint16_t version = packet.read_uint16_be(4 + 8*i);

		uint16_t port = packet.read_uint16_be(6 + 8*i);
		net::SocketAddress peer_addr(transport.dst_addr);
		// TODO: Move into SocketAddress
		reinterpret_cast<sockaddr_in *>(&peer_addr)->sin_port = (port << 8) + (port >> 8);

		delegate->new_peer(peer_addr, protocol, version);
	}
}

template<typename DiscoveryClientDelegate>
void DiscoveryClient<DiscoveryClientDelegate>::send_DISCPEER(
	BaseTransport &transport
) {
	char *message = new char[2] {0, 2};

	net::Buffer p(message, 2);
	transport.send(std::move(p));
}

template<typename DiscoveryClientDelegate>
void DiscoveryClient<DiscoveryClientDelegate>::did_recv_LISTPEER(
	BaseTransport &transport __attribute__((unused)),
	net::Buffer &&packet
) {
	SPDLOG_DEBUG("LISTPEER <<< {}", transport.dst_addr.to_string());

	for(
		uint16_t i = 2;
		i + 7 < packet.size();
		i += 8
	) {
		std::vector<unsigned char> peer_addr_bytes(
			packet.data()+i,
			packet.data()+i+8
		);
		auto peer_addr = net::SocketAddress::deserialize(peer_addr_bytes.begin());
		f.dial(peer_addr, *this);
	}
}

template<typename DiscoveryClientDelegate>
void DiscoveryClient<DiscoveryClientDelegate>::send_HEARTBEAT(
	BaseTransport &transport
) {
	char *message = new char[2] {0, 4};

	net::Buffer p(message, 2);
	transport.send(std::move(p));
}

template<typename DiscoveryClientDelegate>
void DiscoveryClient<DiscoveryClientDelegate>::beacon_timer_cb(uv_timer_t *handle) {
	auto &client = *(DiscoveryClient<DiscoveryClientDelegate> *)handle->data;

	// Discover new peers
	client.send_DISCPEER(*client.beacon);
}

template<typename DiscoveryClientDelegate>
void DiscoveryClient<DiscoveryClientDelegate>::heartbeat_timer_cb(uv_timer_t *handle) {
	auto &client = *(DiscoveryClient<DiscoveryClientDelegate> *)handle->data;

	if(client.is_discoverable) {
		client.send_HEARTBEAT(*client.beacon);
	}
}

//---------------- Discovery protocol functions begin ----------------//


//---------------- Listen delegate functions begin ----------------//

template<typename DiscoveryClientDelegate>
bool DiscoveryClient<DiscoveryClientDelegate>::should_accept(
	net::SocketAddress const &
) {
	return is_discoverable;
}

template<typename DiscoveryClientDelegate>
void DiscoveryClient<DiscoveryClientDelegate>::did_create_transport(
	BaseTransport &transport
) {
	transport.setup(this);
}

//---------------- Listen delegate functions end ----------------//


//---------------- Transport delegate functions begin ----------------//

template<typename DiscoveryClientDelegate>
void DiscoveryClient<DiscoveryClientDelegate>::did_dial(
	BaseTransport &transport
) {
	if(beacon == nullptr) {
		beacon = &transport;
		send_DISCPEER(*beacon);
		uv_timer_start(
			&beacon_timer,
			&beacon_timer_cb,
			60000,
			60000
		);
		if(is_discoverable) {
			uv_timer_start(
				&heartbeat_timer,
				&heartbeat_timer_cb,
				10000,
				10000
			);
		}
	} else {
		send_DISCPROTO(transport);
	}
}

template<typename DiscoveryClientDelegate>
void DiscoveryClient<DiscoveryClientDelegate>::did_recv_packet(
	BaseTransport &transport,
	net::Buffer &&packet
) {
	switch(packet.read_uint8(1)) {
		// DISCPROTO
		case 0: did_recv_DISCPROTO(transport);
		break;
		// LISTPROTO
		case 1: did_recv_LISTPROTO(transport, std::move(packet));
		break;
		// DISCOVER
		case 2: SPDLOG_ERROR("Unexpected DISCPEER from {}", transport.dst_addr.to_string());
		break;
		// PEERLIST
		case 3: did_recv_LISTPEER(transport, std::move(packet));
		break;
		// HEARTBEAT
		case 4: SPDLOG_ERROR("Unexpected HEARTBEAT from {}", transport.dst_addr.to_string());
		break;
		// UNKNOWN
		default: SPDLOG_TRACE("UNKNOWN <<< {}", transport.dst_addr.to_string());
		break;
	}
}

template<typename DiscoveryClientDelegate>
void DiscoveryClient<DiscoveryClientDelegate>::did_send_packet(
	BaseTransport &transport __attribute__((unused)),
	net::Buffer &&packet
) {
	switch(packet.read_uint8(1)) {
		// DISCPROTO
		case 0: SPDLOG_TRACE("DISCPROTO >>> {}", transport.dst_addr.to_string());
		break;
		// LISTPROTO
		case 1: SPDLOG_TRACE("LISTPROTO >>> {}", transport.dst_addr.to_string());
		break;
		// DISCPEER
		case 2: SPDLOG_TRACE("DISCPEER >>> {}", transport.dst_addr.to_string());
		break;
		// LISTPEER
		case 3: SPDLOG_TRACE("LISTPEER >>> {}", transport.dst_addr.to_string());
		break;
		// HEARTBEAT
		case 4: SPDLOG_TRACE("HEARTBEAT >>> {}", transport.dst_addr.to_string());
		break;
		// UNKNOWN
		default: SPDLOG_TRACE("UNKNOWN >>> {}", transport.dst_addr.to_string());
		break;
	}
}

//---------------- Transport delegate functions end ----------------//

template<typename DiscoveryClientDelegate>
DiscoveryClient<DiscoveryClientDelegate>::DiscoveryClient(
	net::SocketAddress const &addr
) {
	f.bind(addr);
	f.listen(*this);

	uv_timer_init(uv_default_loop(), &beacon_timer);
	beacon_timer.data = this;

	uv_timer_init(uv_default_loop(), &heartbeat_timer);
	heartbeat_timer.data = this;
}

template<typename DiscoveryClientDelegate>
void DiscoveryClient<DiscoveryClientDelegate>::start_discovery(
	const net::SocketAddress &beacon_addr
) {
	f.dial(beacon_addr, *this);
}

} // namespace beacon
} // namespace marlin

#endif // MARLIN_BEACON_DISCOVERYCLIENT_HPP
