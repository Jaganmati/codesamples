#pragma once

#include "InetConnection.h"

namespace TechDemo
{
	namespace IO
	{
		class NetworkManager;	// Forward declaration
		class PacketPing;		// Forward declaration

		class PlayerConnection : public InetConnection
		{
			friend class InetConnection;
			friend class NetworkManager;
			friend class PacketPing;
			friend class ServerConnection;

			public:
				PlayerConnection();

				virtual ~PlayerConnection();

				virtual bool connect(std::string const& address);

				virtual bool connect(std::string const& ipAddress, unsigned short port);

				virtual bool disconnect();

				virtual bool send(PacketBase* packet) const;

				virtual bool send(std::shared_ptr<PacketBase> const& packet) const;

				virtual bool send(const char* data, unsigned short length) const;

				// Round-trip time in nanoseconds. 1,000,000 ns = 1 ms
				int getRTT() const;

				const std::vector<float>& getRTTHistory() const;

			private:
				PlayerConnection(std::string const& ipAddress, unsigned short port, unsigned socket);

				std::atomic_int rtt = 0;
				long rttClock = 0;
				std::vector<float> rttHistory = std::vector<float>(150);
				std::atomic_ullong lastUpdate = 0ULL;
		};
	}
}