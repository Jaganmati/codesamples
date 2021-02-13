#include "Engine.h"
#include "GameObject.h"
#include "Model.h"
#include "PacketSpawnObject.h"

namespace TechDemo
{
	namespace IO
	{
		PacketSpawnObject::PacketSpawnObject(const Util::UUID& uuid, const std::string& model, const World::Transform& modelOffset, const std::unordered_set<std::shared_ptr<World::ComponentBase>>& components) : Packet<PacketSpawnObject>(), uuid(uuid), model(model), modelOffset(modelOffset), components(components)
		{
		}

		void PacketSpawnObject::serialize(BitStream& stream)
		{
			stream.write(uuid);
			stream.write(model);
			stream.write(modelOffset);

			uint16_t size = 0U;
			BitStream compStr;

			for (auto& comp : components)
			{
				if (comp->synchronized())
				{
					compStr.write(comp->getTypeHash());
					compStr.write(comp->getUUID());
					compStr.write(*comp);

					++size;
				}
			}

			stream.write(size);
			stream << compStr;
		}

		void PacketSpawnObject::deserialize(BitStream& stream)
		{
			stream.read(uuid);
			stream.read(model);
			stream.read(modelOffset);

			uint16_t size = 0;
			stream.read(size);

			for (uint16_t i = 0; i < size; ++i)
			{
				uint32_t type = 0;
				stream.read(type);

				auto comp = World::ComponentBase::create(type);

				Util::UUID uuid;
				stream.read(uuid);

				comp->uuid = uuid;

				stream.read(*comp);

				components.emplace(comp);
			}
		}

		void PacketSpawnObject::handle(InetConnection const& conn, Direction direction)
		{
			World::GameObject* object = new World::GameObject(uuid);

			if (!model.empty())
				object->setModel(Graphics::Model::getModel(model));

			object->getModelOffset() = modelOffset;

			object->componentsLock.lock();
			for (auto const& comp : components)
				object->addComponent(comp);

			for (auto const& comp : object->getComponents())
				comp->init();
			object->componentsLock.unlock();

			Engine::getScene().addObject(object);
		}

		void PacketSpawnObject::setUUID(const Util::UUID& uuid)
		{
			this->uuid = uuid;
		}
	}
}