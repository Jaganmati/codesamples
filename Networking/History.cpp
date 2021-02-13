#include "Component.h"
#include "Connection.h"
#include "Engine.h"
#include "GameObject.h"
#include "History.h"
#include "PlayerConnection.h"
#include "RTTI.h"
#include "Scene.h"

#include <iostream>

namespace TechDemo
{
	namespace World
	{
		std::atomic_bool History::logEverything = false;
		std::unordered_map<Util::UUID,														// For a given scene (by scene's UUID)
			std::pair<std::recursive_mutex, std::map<unsigned long long,					// For a given frame (by closest server timestamp [sorted in ascending order])
			std::pair<std::shared_ptr<std::recursive_mutex>, std::unordered_map<Util::UUID,	// For a given component (by component's UUID)
			std::tuple<std::shared_ptr<std::recursive_mutex>, uint32_t, std::unordered_map<std::string,		// For a given parameter (by name) [uint32_t represents component type hash]
			std::pair<std::shared_ptr<std::recursive_mutex>, std::tuple<unsigned long long, IO::BitStream>>	// Record of delta since last change and new value
		>>>>>>> History::data;
		std::recursive_mutex History::dataLock;

		
		std::list<std::shared_ptr<ComponentBase>> History::getState(const std::shared_ptr<IO::Connection>& conn, const Util::UUID& sceneId, unsigned long long timestamp)
		{
			std::list<std::shared_ptr<ComponentBase>> result;

			dataLock.lock();

			auto sceneIter = data.find(sceneId);
			if (sceneIter != data.end())
			{
				sceneIter->second.first.lock();

				auto& frames = sceneIter->second.second;
				auto frameIter = frames.lower_bound(timestamp);
				bool isBetween = frameIter != frames.end() && frameIter->first > timestamp;
				frameIter = isBetween && frameIter != frames.begin() ? std::prev(frameIter) : frameIter;

				if (frameIter != frames.end())
				{
					frameIter->second.first->lock();

					auto& components = frameIter->second.second;
					for (auto& componentPair : components)
					{
						auto& uniqueId = componentPair.first;
						auto& typeHash = std::get<1>(componentPair.second);

						auto type = Util::RTTI::getType(typeHash);

						if (type != nullptr)
						{
							auto component = ComponentBase::create(typeHash);

							if (component != nullptr)
							{
								component->uuid = uniqueId;
								component->init();

								for (const auto& variable : type->getVariableNames())
									getState(conn, frames, timestamp, component, variable);

								result.emplace_back(component);
							}
						}
					}

					frameIter->second.first->unlock();
				}

				sceneIter->second.first.unlock();
			}

			dataLock.unlock();

			return result;
		}

		IO::BitStream History::getState(const Util::UUID& sceneId, unsigned long long timestamp, const Util::UUID& componentId, const std::string& variable)
		{
			std::lock_guard dLock(dataLock);

			auto sceneIter = data.find(sceneId);
			if (sceneIter != data.end())
			{
				std::lock_guard sceneLock(sceneIter->second.first);

				auto& frames = sceneIter->second.second;
				auto frameIter = frames.lower_bound(timestamp);

				if (frameIter != frames.end())
				{
					frameIter = frameIter->first > timestamp&& frameIter != frames.begin() ? std::prev(frameIter) : frameIter;

					if (frameIter->first > timestamp)
						return IO::BitStream();

					for (std::reverse_iterator rFrameIter(std::next(frameIter)); rFrameIter != frames.rend(); ++rFrameIter)
					{
						auto compsLock = rFrameIter->second.first;
						auto& components = rFrameIter->second.second;

						std::lock_guard csLock(*compsLock);

						auto compIter = components.find(componentId);
						if (compIter != components.end())
						{
							auto& compLock = std::get<0>(compIter->second);
							auto compType = Util::RTTI::getType(std::get<1>(compIter->second));
							auto& variables = std::get<2>(compIter->second);

							std::lock_guard cLock(*compLock);

							auto var = variables.find(variable);

							if (var != variables.end())
							{
								std::lock_guard varLock(*std::get<0>(var->second));

								auto bs = std::get<1>(std::get<1>(var->second));
								auto varData = compType->getVariable(variable);

								auto interpolator = std::get<5>(varData);

								if (interpolator && rFrameIter->first != timestamp)
								{
									for (auto nextFrameIter = std::next(frameIter); nextFrameIter != frames.end(); ++nextFrameIter)
									{
										auto& nextComponents = nextFrameIter->second.second;
										auto nextCompsLock = nextFrameIter->second.first;

										std::lock_guard lock(*nextCompsLock);

										auto nextComponent = nextComponents.find(componentId);
										if (nextComponent != nextComponents.end())
										{
											auto& nextVarLock = std::get<0>(nextComponent->second);
											auto& nextVariables = std::get<2>(nextComponent->second);

											std::lock_guard innerLock(*nextVarLock);

											auto nextVarIter = nextVariables.find(variable);
											if (nextVarIter != nextVariables.end())
											{
												nextVarIter->second.first->lock();

												// Interpolate between frame values
												float distance = static_cast<float>(timestamp - rFrameIter->first) / static_cast<float>(nextFrameIter->first - rFrameIter->first);

												interpolator(bs.data().data(), std::get<1>(std::get<1>(nextVarIter->second)).data().data(), distance, const_cast<char*>(bs.data().data()));

												nextVarIter->second.first->unlock();
												break;
											}
										}
									}
								}

								return bs;
							}
						}
					}
				}
			}

			return IO::BitStream();
		}

		void History::applyState(const std::shared_ptr<IO::Connection>& conn, const Util::UUID& sceneId, unsigned long long timestamp)
		{
			dataLock.lock();

			auto sceneIter = data.find(sceneId);
			if (sceneIter != data.end())
			{
				sceneIter->second.first.lock();

				auto& frames = sceneIter->second.second;
				auto frameIter = frames.lower_bound(timestamp);
				bool isBetween = frameIter != frames.end() && frameIter->first > timestamp;
				frameIter = isBetween && frameIter != frames.begin() ? std::prev(frameIter) : frameIter;

				if (frameIter != frames.end())
				{
					frameIter->second.first->lock();

					auto& components = frameIter->second.second;
					for (auto& componentPair : components)
					{
						auto& uniqueId = componentPair.first;
						auto& typeHash = std::get<1>(componentPair.second);

						auto type = Util::RTTI::getType(typeHash);

						if (type != nullptr)
						{
							auto component = World::ComponentBase::getComponent(uniqueId);	// TODO: Handle deletion of component (dynamic construction / deletion)

							if (component != nullptr)
							{
								for (const auto& variable : type->getVariableNames())
									getState(conn, frames, timestamp, component, variable);
							}
						}
					}

					frameIter->second.first->unlock();
				}

				sceneIter->second.first.unlock();
			}

			dataLock.unlock();
		}

		void History::log(const Util::UUID& sceneId, unsigned long long timestamp)
		{
			Scene* scene = Scene::getScene(sceneId);

			if (scene)
			{
				static const uint32_t baseHash = Util::CRC32::checksum(Util::getQualifiedName<ComponentBase>());
				bool logAll = logEverything;

				decltype(data)::value_type::second_type::second_type::value_type::second_type::second_type components;

				scene->getObjectsLock().lock();
				for (const auto& object : scene->getObjects())
				{
					for (const auto& component : object.second->getComponents())
					{
						uint32_t typeHash = component->getTypeHash();
						const Util::RTTI::Type* type = Util::RTTI::getType(typeHash);

						if (type)
						{
							Util::extract_template_type<decltype(components)::value_type::second_type::_Mybase::_Mybase>::type variables;

							int32_t baseOffset = type->getBaseOffset(baseHash).second;

							for (const auto& variable : type->getVariables(reinterpret_cast<char*>(&*component) - baseOffset, true))
							{
								const auto& varName = variable.first;
								char* memory = std::get<0>(variable.second);
								const Util::RTTI::Type* varType = std::get<1>(variable.second);
								bool isPointer = std::get<2>(variable.second);

								if (!isPointer)
								{
									IO::BitStream bs;
									bs.write(memory, varType->size);

									unsigned long long deltaTime = 0ULL;

									dataLock.lock();

									auto scene = data.find(sceneId);
									if (scene != data.end())
									{
										auto& sceneMutex = scene->second.first;
										sceneMutex.lock();

										auto& frames = scene->second.second;
										auto next = frames.lower_bound(timestamp);
										auto prev = next != frames.end() ? next != frames.begin() ? ++std::reverse_iterator(next) : frames.rend() : !frames.empty() ? frames.rbegin() : frames.rend();

										bool skip = false;

										for (; next != frames.end(); ++next)
										{
											//auto& mutex = next->second.first;
											//std::lock_guard lock(mutex);

											auto compIter = next->second.second.find(component->getUUID());
											if (compIter != next->second.second.end())
											{
												auto& tuple = compIter->second;
												//std::lock_guard innerLock(std::get<0>(tuple));

												auto& variables = std::get<2>(tuple);
												auto varIter = variables.find(variable.first);
												if (varIter != variables.end())
												{
													if (prev != frames.rend())
													{
														varIter->second.first->lock();

														prev = std::reverse_iterator(frames.find(next->first - std::get<0>(std::get<1>(varIter->second))));

														varIter->second.first->unlock();
													}

													break;
												}
											}
										}

										auto varComparator = std::get<3>(variable.second);

										// Compare bitstream data with old value, if one exists.
										for (; prev != frames.rend(); ++prev)
										{
											//auto& mutex = prev->second.first;
											//std::lock_guard lock(mutex);
											
											auto compIter = prev->second.second.find(component->getUUID());
											if (compIter != prev->second.second.end())
											{
												auto& tuple = compIter->second;
												//std::lock_guard innerLock(std::get<0>(tuple));

												auto& variables = std::get<2>(tuple);
												auto varIter = variables.find(variable.first);
												if (varIter != variables.end())
												{
													varIter->second.first->lock();

													auto& oldBitStream = std::get<1>(std::get<1>(varIter->second));
													skip = !logAll && varComparator(bs, oldBitStream);
													deltaTime = timestamp - prev->first;

													varIter->second.first->unlock();
													break;
												}
											}
										}

										if (skip)
										{
											sceneMutex.unlock();
											dataLock.unlock();
											continue;
										}

										// Update delta timestamp of next log, if one exists
										if (next != frames.end())
										{
											//auto& mutex = next->second.first;
											//std::lock_guard lock(mutex);

											auto compIter = next->second.second.find(component->getUUID());
											if (compIter != next->second.second.end())
											{
												auto& tuple = compIter->second;
												//std::lock_guard innerLock(std::get<0>(tuple));

												auto& variables = std::get<2>(tuple);
												auto varIter = variables.find(variable.first);
												if (varIter != variables.end())
												{
													if (prev != frames.rend())
													{
														varIter->second.first->lock();

														std::get<0>(varIter->second.second) -= deltaTime;

														varIter->second.first->unlock();
													}
												}
											}
										}
										
										sceneMutex.unlock();
									}

									dataLock.unlock();

									auto& varEntry = variables[varName];
									varEntry.first = std::shared_ptr<std::recursive_mutex>(new std::recursive_mutex);
									varEntry.second = std::make_tuple(deltaTime, std::move(bs));
								}
							}

							if (!variables.empty())
							{
								auto& comp = components[component->getUUID()];
								std::get<0>(comp) = std::shared_ptr<std::recursive_mutex>(new std::recursive_mutex);
								std::get<1>(comp) = typeHash;
								std::get<2>(comp) = std::move(variables);
							}
						}
					}
				}
				scene->getObjectsLock().unlock();

				if (!components.empty())
				{
					dataLock.lock();

					auto& updates = pendingUpdates();

					// Log pending update for remote clients
					{
						auto& mutex = IO::Connection::getConnectionsLock();
						std::lock_guard connLock(mutex);
						auto& updatesLock = getUpdatesLock();
						std::lock_guard updateLock(updatesLock);
						for (auto const& conn : IO::Connection::getConnections())
						{
							for (auto const& comp : components)
							{
								updates[comp.first].emplace(timestamp, conn.second);
							}
						}
					}

					// Log pending update for local client
					if (Engine::client && !Engine::server && Engine::client->isConnected())
					{
						auto& updatesLock = getUpdatesLock();
						std::lock_guard updateLock(updatesLock);
						for (auto const& comp : components)
							updates[comp.first].emplace(timestamp, nullptr);
					}

					auto& s = data[sceneId];
					s.first.lock();
					dataLock.unlock();

					auto& frame = s.second[timestamp];
					frame.first = std::shared_ptr<std::recursive_mutex>(new std::recursive_mutex);
					frame.second = std::move(components);

					s.first.unlock();
				}
			}
		}

		void History::remove(const Util::UUID& sceneId, unsigned long long timestamp)
		{
			dataLock.lock();

			auto scene = data.find(sceneId);
			if (scene != data.end())
			{
				scene->second.first.lock();
				dataLock.unlock();

				auto frame = scene->second.second.find(timestamp);

				if (frame != scene->second.second.end())
				{
					auto lock = frame->second.first;
					lock->lock();

					scene->second.second.erase(frame);

					lock->unlock();
				}
				
				scene->second.first.unlock();
			} else dataLock.unlock();
		}

		void History::removeBefore(const Util::UUID& sceneId, unsigned long long timestamp)
		{
			dataLock.lock();

			auto scene = data.find(sceneId);
			if (scene != data.end())
			{
				scene->second.first.lock();
				dataLock.unlock();

				auto& frames = scene->second.second;
				auto frame = frames.lower_bound(timestamp);

				if (frame != frames.begin())
				{
					auto prevFrame = frame != frames.end() ? ++std::reverse_iterator(frame) : frames.rbegin();

					while (prevFrame.base() != frames.end())
					{
						auto f = std::reverse_iterator(frames.erase(prevFrame.base()));
						prevFrame = f.base() != frames.end() ? ++f : f;
					}
				}

				scene->second.first.unlock();
			}
			else dataLock.unlock();
		}

		void History::removeAfter(const Util::UUID& sceneId, unsigned long long timestamp)
		{
			dataLock.lock();

			auto scene = data.find(sceneId);
			if (scene != data.end())
			{
				scene->second.first.lock();
				dataLock.unlock();

				auto& frames = scene->second.second;

				for (auto frame = frames.upper_bound(timestamp); frame != frames.end();)
					frame = frames.erase(frame);

				scene->second.first.unlock();
			}
			else dataLock.unlock();
		}

		std::unordered_map<Util::UUID, std::unordered_map<std::string, IO::BitStream>> History::getChanges(const Util::UUID& sceneId, unsigned long long timestamp)
		{
			std::unordered_map<Util::UUID, std::unordered_map<std::string, IO::BitStream>> result;

			dataLock.lock();

			auto sceneIter = data.find(sceneId);
			if (sceneIter != data.end())
			{
				auto& frameMutex = sceneIter->second.first;
				auto& frames = sceneIter->second.second;
				frameMutex.lock();
				dataLock.unlock();

				auto frameIter = frames.find(timestamp);
				if (frameIter != frames.end())
				{
					auto componentMutex = frameIter->second.first;
					auto& components = frameIter->second.second;
					componentMutex->lock();
					frameMutex.unlock();

					for (auto& component : components)
					{
						auto& varMutex = std::get<0>(component.second);
						auto& variables = std::get<2>(component.second);

						std::unordered_map<std::string, IO::BitStream> varData;

						varMutex->lock();
						for (auto& variable : variables)
						{
							auto& streamMutex = std::get<0>(variable.second);

							streamMutex->lock();
							auto stream = std::get<1>(std::get<1>(variable.second));
							streamMutex->unlock();

							varData.emplace(variable.first, std::move(stream));
						}
						varMutex->unlock();

						result.emplace(component.first, std::move(varData));
					}

					componentMutex->unlock();
				} else frameMutex.unlock();
			}
			else dataLock.unlock();

			return result;
		}

		std::unordered_map<Util::UUID, std::unordered_map<std::string, std::pair<unsigned long long, IO::BitStream>>> History::getChanges(const Util::UUID& sceneId, unsigned long long fromExclusive, unsigned long long toInclusive)
		{
			std::unordered_map<Util::UUID, std::unordered_map<std::string, std::pair<unsigned long long, IO::BitStream>>> result;

			if (toInclusive > fromExclusive)
			{
				dataLock.lock();

				auto sceneIter = data.find(sceneId);
				if (sceneIter != data.end())
				{
					auto& frameMutex = sceneIter->second.first;
					auto& frames = sceneIter->second.second;
					frameMutex.lock();
					dataLock.unlock();

					auto iter = std::reverse_iterator(frames.lower_bound(toInclusive));
					iter = iter == frames.rend() ? std::reverse_iterator(frames.begin()) : iter->first == toInclusive ? iter : iter != frames.rend() ? std::next(iter) : iter;
					auto from = std::reverse_iterator(frames.lower_bound(fromExclusive));
					from = from == frames.rend() || from->first == fromExclusive ? from : from != frames.rbegin() ? std::next(from) : frames.rend();

					if (iter != frames.rend())
					{
						for (; iter != from; ++iter)
						{
							auto componentMutex = iter->second.first;
							auto& components = iter->second.second;
							componentMutex->lock();

							for (auto& component : components)
							{
								auto& varMutex = std::get<0>(component.second);
								auto& variables = std::get<2>(component.second);

								varMutex->lock();
								for (auto& variable : variables)
								{
									auto& streamMutex = std::get<0>(variable.second);

									streamMutex->lock();
									auto stream = std::get<1>(std::get<1>(variable.second));
									streamMutex->unlock();

									result[component.first].try_emplace(variable.first, std::make_pair(iter->first, std::move(stream)));
								}
								varMutex->unlock();
							}

							componentMutex->unlock();
						}
					}

					frameMutex.unlock();
				}
				else dataLock.unlock();
			}

			return result;
		}

		std::unordered_map<Util::UUID, std::multimap<unsigned long long, std::shared_ptr<IO::Connection>>>& History::pendingUpdates()
		{
			static std::unordered_map<Util::UUID,												// For a given component (by component's UUID)
				std::multimap<unsigned long long,												// For a given frame (by closest server timestamp [sorted in ascending order])
				std::shared_ptr<IO::Connection>													// Record of pending connections (awaiting verification of change)
			>> pending;

			return pending;
		}

		std::mutex& History::getUpdatesLock()
		{
			static std::mutex lock;
			return lock;
		}

		void History::getState(const std::shared_ptr<IO::Connection>& conn, decltype(History::data)::iterator::value_type::second_type::second_type& frames, unsigned long long timestamp, std::shared_ptr<ComponentBase>& component, const std::string& variable)
		{
			auto frameIter = frames.lower_bound(timestamp);

			if (frameIter != frames.end())
			{
				const Util::UUID& componentId = component->getUUID();
				const Util::RTTI::Type* type = Util::RTTI::getType(component->getTypeHash());

				if (type)
				{
					frameIter = frameIter->first > timestamp && frameIter != frames.begin() ? std::prev(frameIter) : frameIter;

					if (frameIter->first > timestamp)
						return;

					for (std::reverse_iterator rFrameIter(/*frameIter->first > timestamp ? frameIter : */std::next(frameIter)); rFrameIter != frames.rend(); ++rFrameIter)
					{
						auto compsLock = rFrameIter->second.first;
						auto& components = rFrameIter->second.second;

						std::lock_guard csLock(*compsLock);

						auto compIter = components.find(componentId);

						if (compIter != components.end())
						{
							auto& compLock = std::get<0>(compIter->second);
							auto& variables = std::get<2>(compIter->second);

							std::lock_guard cLock(*compLock);

							auto var = variables.find(variable);

							if (var != variables.end())
							{
								std::get<0>(var->second)->lock();

								auto& bs = std::get<1>(std::get<1>(var->second));
								
								auto varData = type->getVariable(reinterpret_cast<char*>(&*component), variable);

								auto premodifycallback = std::get<8>(varData);
								if (premodifycallback)
									premodifycallback(std::get<1>(varData));
								
								bs.peek(std::get<1>(varData), std::get<2>(varData)->getSize());

								auto postmodifycallback = std::get<9>(varData);
								if (postmodifycallback)
									postmodifycallback(std::get<1>(varData));

								auto interpolator = std::get<5>(varData);

								if (interpolator && rFrameIter->first != timestamp)
								{
									for (auto nextFrameIter = std::next(frameIter); nextFrameIter != frames.end(); ++nextFrameIter)
									{
										auto& nextComponents = nextFrameIter->second.second;
										auto nextCompsLock = nextFrameIter->second.first;

										std::lock_guard lock(*nextCompsLock);

										auto nextComponent = nextComponents.find(componentId);
										if (nextComponent != nextComponents.end())
										{
											auto& nextVarLock = std::get<0>(nextComponent->second);
											auto& nextVariables = std::get<2>(nextComponent->second);

											std::lock_guard innerLock(*nextVarLock);

											auto nextVarIter = nextVariables.find(variable);
											if (nextVarIter != nextVariables.end())
											{
												std::get<0>(nextVarIter->second)->lock();

												// Interpolate between frame values
												float distance = static_cast<float>(timestamp - rFrameIter->first) / static_cast<float>(nextFrameIter->first - rFrameIter->first);
												
												if (premodifycallback)
													premodifycallback(std::get<1>(varData));

												interpolator(std::get<1>(varData), std::get<1>(std::get<1>(nextVarIter->second)).data().data(), distance, std::get<1>(varData));

												if (postmodifycallback)
													postmodifycallback(std::get<1>(varData));

												std::get<0>(nextVarIter->second)->unlock();
												break;
											}
										}
									}
								}

								std::get<0>(var->second)->unlock();
								return;
							}
						}
					}
				}
			}
		}

		void History::applyChanges(Util::UUID const& componentId, std::unordered_map<std::string, std::pair<unsigned long long, IO::BitStream>> const& variables)
		{
			// TODO: Send the ordered data instead of needing to do this.
			std::unordered_multimap<unsigned long long, std::pair<std::string, IO::BitStream>> orderedVars;

			for (auto& var : variables)
				orderedVars.emplace(var.second.first, std::make_pair(var.first, var.second.second));

			dataLock.lock();
			auto sceneIter = data.find(Engine::getScene().getUUID());
			sceneIter->second.first.lock();
			dataLock.unlock();

			for (auto iter = orderedVars.begin(), next = orderedVars.upper_bound(iter->first); iter != orderedVars.end();)
			{
				auto timeIter = sceneIter->second.second.find(iter->first);

				if (timeIter == sceneIter->second.second.end())
				{
					//decltype(sceneIter->second.second)::value_type::second_type value;
					//timeIter = sceneIter->second.second.emplace(iter->first, value).first;
				}

				// TODO: Get last time and component hash
				auto lastTimeIter = timeIter - 1;

				while (true)
				{


					if (lastTimeIter == sceneIter->second.second.begin())
						break;
				}

				timeIter->second.first->lock();

				auto componentIter = timeIter->second.second.find(componentId);

				if (componentIter == timeIter->second.second.end())
				{
					uint32_t componentHash = 0U;	// TODO
					std::tuple<std::recursive_mutex, uint32_t, decltype(timeIter->second.second)::value_type::second_type::_Mybase::_Mybase::_This_type> value;
					std::get<1>(value) = componentHash;
					//componentIter = timeIter->second.second.emplace(componentId, std::move(value)).first;
				}

				timeIter->second.first->unlock();
				std::get<0>(componentIter->second)->lock();

				for (; iter != next; ++iter)
				{
					auto& vars = std::get<2>(componentIter->second);
					auto varIter = vars.find(iter->second.first);

					if (varIter == vars.end())
					{
						unsigned long long deltaTime = 0ULL;	// TODO
						//vars.emplace(iter->second.first, std::tuple<std::shared_ptr<std::recursive_mutex>, std::tuple<unsigned long long, IO::BitStream>>(std::shared_ptr<std::recursive_mutex>(new std::recursive_mutex), std::tuple<unsigned long long, IO::BitStream>(deltaTime, iter->second.second)));
					}
					else
					{
						std::get<0>(varIter->second)->lock();

						std::get<1>(std::get<1>(varIter->second)) = std::move(iter->second.second);

						std::get<0>(varIter->second)->unlock();
					}
				}

				std::get<0>(componentIter->second)->unlock();

				if (next != orderedVars.end())
					next = orderedVars.upper_bound(iter->first);
			}

			sceneIter->second.first.unlock();
		}
	}
}