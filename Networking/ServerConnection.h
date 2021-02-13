#pragma once

#include <memory>
#include <unordered_set>

#include "InetConnection.h"

namespace TechDemo
{
	namespace IO
	{
		class PlayerConnection;

		class ServerConnection : public InetConnection
		{
			friend class PlayerConnection;

			public:
				ServerConnection();

				bool listen(unsigned short port);

				virtual bool connect(std::string const& ipAddress, unsigned short port);

				virtual bool disconnect();

				virtual bool send(PacketBase* packet) const;

				virtual bool send(std::shared_ptr<PacketBase> const& packet) const;

				std::unordered_set<std::shared_ptr<PlayerConnection>> getClients() const;

			private:
				void listenLoop();
		};
	}
}