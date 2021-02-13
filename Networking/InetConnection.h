#pragma once

#include <atomic>
#include <bitset>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_map>

#include "BitStream.h"
#include "Connection.h"
#include "Random.h"

struct sockaddr_storage;	// Forward declaration

namespace TechDemo
{
	namespace IO
	{
		class InetConnection : public Connection
		{
			friend class PacketHandshake;
			friend class PacketNACK;
			friend class ServerConnection;

			public:
				InetConnection();

				virtual ~InetConnection();

				virtual bool connect(std::string const& address);

				virtual bool connect(std::string const& ipAddress, unsigned short port);

				virtual bool disconnect();

				virtual bool send(PacketBase* packet) const;

				virtual bool send(std::shared_ptr<PacketBase> const& packet) const;

				virtual std::string const& getRemoteAddress() const;
				
				virtual unsigned short getPort() const;

				virtual unsigned short getLocalPort() const;

				virtual bool isLocal() const;

				virtual long getTimeOffset() const;

				virtual void setDropChance(float dropChance);

				virtual float getDropChance() const;

				static std::shared_ptr<InetConnection> getConnection(sockaddr_storage* address);

				static std::pair<std::shared_ptr<InetConnection>, bool> getConnection(sockaddr_storage* address, unsigned socket);

			protected:
				static std::pair<int, std::string> getLastError();
				static std::pair<int, std::string> getLastError(unsigned socket);
				static void* getInetAddr(struct sockaddr* addr);

				InetConnection(std::string const& ipAddress, unsigned short port, unsigned socket);

				virtual bool send(const char* data, unsigned short length) const;
				virtual void connectLoop();

				std::atomic_uint socket = 0;
				std::string ipAddress;
				unsigned short port = 0;
				unsigned short localPort = 0;
				bool local = true;
				mutable std::atomic_bool connecting = false;
				std::atomic_bool initialized = false;
				std::thread listeningThread;
				std::atomic_long timeOffset = 0L;

				long dataClock = 0;			// Clock for bandwidth and other data analysis
				mutable std::atomic_ulong bytesSent = 0;
				mutable std::atomic_ulong bytesRcvd = 0;
				mutable std::atomic_ulong packetsSent = 0;
				mutable std::atomic_ulong packetsResent = 0;
				mutable std::atomic_ulong packetsLost = 0;
				mutable std::atomic_ulong packetsDuplicated = 0;

				// Reliability Variables
				mutable std::atomic_uint32_t nextSequenceNumber = Util::Random::xorshift();	// Outbound sequence number
				uint32_t expectedSequenceNumber = 0;										// Inbound sequence number
				uint32_t lastSequenceNumber = 0;											// Largest inbound sequence number
				mutable std::unordered_map<uint32_t, std::string> sentPackets;
				mutable std::mutex sentPacketLock;
				std::unordered_map<uint32_t, BitStream> receivedPackets;
				std::set<uint32_t> missingPackets;
				std::mutex missingPacketsLock;
				BitStream dataStream;

				float packetDropChance = 0.0f;
		};
	}
}