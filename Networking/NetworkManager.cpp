#include <iostream>

#include <WinSock2.h>

#include "Animator.h"
#include "CircularPath.h"
#include "Connection.h"
#include "Engine.h"
#include "GameObject.h"
#include "Messenger.h"
#include "Model.h"
#include "NetworkManager.h"
#include "PacketDestroyObject.h"
#include "PacketSpawnObject.h"
#include "PlayerConnection.h"
#include "PlayerController.h"
#include "ServerConnection.h"

namespace TechDemo
{
	namespace IO
	{
		WSAData NetworkManager::winSockData;
		std::unordered_map<std::shared_ptr<PlayerConnection>, Util::UUID> NetworkManager::players;

		void NetworkManager::init()
		{
			if (int result = WSAStartup(MAKEWORD(2, 2), &winSockData))
			{
				std::cerr << "An error occurred while starting Windows Sockets (" << result << ")" << std::endl;
				std::exit(-1);
			}

			Util::Messenger::addListener(NetworkManager::serverStart);
			Util::Messenger::addListener(NetworkManager::serverStop);
			Util::Messenger::addListener(NetworkManager::serverNewClient);
			Util::Messenger::addListener(NetworkManager::serverClientDisconnect);
		}

		void NetworkManager::shutdown()
		{
			while (Connection::hasConnections());

			if (int result = WSACleanup())
			{
				std::cerr << "An error occurred while shutting down Windows Sockets (" << result << ")" << std::endl;
			}

			Util::Messenger::removeListener(NetworkManager::serverStart);
			Util::Messenger::removeListener(NetworkManager::serverStop);
			Util::Messenger::removeListener(NetworkManager::serverNewClient);
			Util::Messenger::removeListener(NetworkManager::serverClientDisconnect);
		}

		void NetworkManager::serverStart(const ServerStartListening*)
		{
			World::GameObject* object = new World::GameObject({ 0.0f, -1.0f, 0.0f }, { 30.0f, 0.5f, 30.0f });
			object->setModel(Graphics::Model::getModel("./models/cube2.obj"));
			object->addComponent<Physics::RigidBody>(object->getTransform());
			Engine::getScene().addObject(object);

			for (int y = 1; y <= 5; ++y)
			{
				for (int x = -2; x <= 2; ++x)
				{
					for (int z = -2; z <= 2; ++z)
					{
						World::GameObject* object = new World::GameObject({ x + y * 0.85f, y, z + y * 0.85f }, 0.5f);
						object->setModel(Graphics::Model::getModel("./models/cube2.obj"));
						object->addComponent<Physics::RigidBody>(object->getTransform());
						Engine::getScene().addObject(object);
					}
				}
			}

			World::GameObject* movingObject = new World::GameObject({-6, -0.5f, -6}, 0.005f);
			movingObject->setModel(Graphics::Model::getModel("./models/ybot.fbx"));
			//movingObject->addComponent<Physics::RigidBody>(movingObject->getTransform());
			//movingObject->addComponent<World::CircularPath>(glm::vec3(-4, 1, -4));
			movingObject->addComponent<World::Animator>();
			Engine::getScene().addObject(movingObject);

			World::GameObject* player = new World::GameObject({ -3, 3, 0 }, 0.005f);
			player->addComponent<Graphics::Camera>();
			player->addComponent<IO::PlayerController>();
			player->setModel(Graphics::Model::getModel("./models/ybot.fbx"));
			player->getModelOffset().addRotation(glm::radians(180.0f), glm::vec3(0, 1, 0));
			player->getModelOffset().setPosition({ 0, -0.7f, 0 });
			player->setVisible(false);
			Engine::getScene().addObject(player);
		}

		void NetworkManager::serverStop(const ServerStop*)
		{
			std::recursive_mutex& mutex = Connection::getConnectionsLock();
			std::lock_guard lock(mutex);
			for (auto& pair : Connection::getConnections())
			{
				auto conn = pair.second;
				conn->disconnect();
			}

			Connection::getConnections().clear();
		}

		void NetworkManager::serverNewClient(const ServerIncomingConnect* msg)
		{
			if (msg->connection)
			{
				if (!Engine::client || msg->connection->getPort() != Engine::client->getLocalPort())
				{
					World::GameObject* player = new World::GameObject({ -3, 3, 0 }, 0.005f);
					player->addComponent<Graphics::Camera>()->setSynchronized(true);
					player->addComponent<IO::PlayerController>();
					player->getModelOffset().addRotation(glm::radians(180.0f), glm::vec3(0, 1, 0));
					player->getModelOffset().setPosition({ 0, -0.7f, 0 });
					Engine::getScene().addObject(player);

					std::recursive_mutex& mutex = Engine::getScene().getObjectsLock();
					std::lock_guard lock(mutex);

					for (auto const& pair : Engine::getScene().getObjects())
					{
						auto model = pair.second->getModel();
						msg->connection->send(new PacketSpawnObject(pair.second->getUUID(), model ? model->getFileName() : "", pair.second->getModelOffset(), pair.second->getComponents()));
					}

					player->removeComponent<Graphics::Camera>();
					player->setModel(Graphics::Model::getModel("./models/ybot.fbx"));

					for (auto const& conn : Engine::server->getClients())
					{
						if (conn && conn->getPort() != msg->connection->getPort() && (!Engine::client || conn->getPort() != Engine::client->getLocalPort()))
						{
							auto model = player->getModel();
							msg->connection->send(new PacketSpawnObject(player->getUUID(), model ? model->getFileName() : "", player->getModelOffset(), player->getComponents()));
						}
					}

					players.emplace(msg->connection, player->getUUID());
				}

				msg->connection->lastUpdate = Engine::getTimestamp();
			}
		}

		void NetworkManager::serverClientDisconnect(const ServerClientDisconnect* msg)
		{
			auto iter = players.find(msg->connection);

			if (iter != players.end())
			{
				if (std::shared_ptr<World::GameObject> object = World::GameObject::getObject(iter->second))
				{
					Engine::server->send(new PacketDestroyObject(iter->second));

					object->remove();
				}

				players.erase(iter);
			}
		}
	}
}