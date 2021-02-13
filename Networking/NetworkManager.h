#pragma once

#include <unordered_map>

#include "UUID.h"

struct WSAData;

namespace TechDemo
{
	namespace IO
	{
		class PlayerConnection;	//!< Forward declaration

		struct ServerStartListening {};

		struct ServerStop {};

		struct ServerIncomingConnect
		{
			std::shared_ptr<PlayerConnection> connection = nullptr;
		};

		struct ServerClientDisconnect
		{
			std::shared_ptr<PlayerConnection> connection = nullptr;
		};

		struct ClientHandshakeComplete
		{
			PlayerConnection const* connection = nullptr;
		};

		class NetworkManager
		{
			public:
				NetworkManager() = delete;

				static void init();

				static void shutdown();

				static void serverStart(const ServerStartListening*);

				static void serverStop(const ServerStop*);

				static void serverNewClient(const ServerIncomingConnect* msg);

				static void serverClientDisconnect(const ServerClientDisconnect* msg);

			private:
				static WSAData winSockData;
				static std::unordered_map<std::shared_ptr<PlayerConnection>, Util::UUID> players;
		};
	}
}