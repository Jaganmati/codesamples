#pragma once

#include <unordered_set>

#include "Component.h"
#include "Packet.h"
#include "UUID.h"

namespace TechDemo
{
	namespace IO
	{
		class PacketSpawnObject : public Packet<PacketSpawnObject>
		{
			public:
				PacketSpawnObject() = default;

				PacketSpawnObject(const Util::UUID& uuid, const std::string& model, const World::Transform& modelOffset, const std::unordered_set<std::shared_ptr<World::ComponentBase>>& components);

				virtual void serialize(BitStream& stream);

				virtual void deserialize(BitStream& stream);

				virtual void handle(InetConnection const& conn, Direction direction);

				void setUUID(const Util::UUID& uuid);

			private:
				Util::UUID uuid;
				std::string model;
				World::Transform modelOffset;
				std::unordered_set<std::shared_ptr<World::ComponentBase>> components;
		};
	}
}