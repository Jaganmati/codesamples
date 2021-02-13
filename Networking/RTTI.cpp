#include <deque>

#include "RTTI.h"

namespace TechDemo
{
	namespace Util
	{
		uint32_t RTTI::Type::getHash() const
		{
			return hash;
		}

		size_t RTTI::Type::getSize() const
		{
			return size;
		}

		char* RTTI::Type::create() const
		{
			return creator ? creator() : nullptr;
		}

		std::list<const RTTI::Type*> RTTI::Type::getBases() const
		{
			std::list<const Type*> result;

			for (const auto& base : bases)
			{
				const RTTI::Type* baseType = getType(base.first);
				result.push_back(baseType);

				if (baseType)
					result.merge(baseType->getBases());
			}

			return result;
		}

		bool RTTI::Type::hasBase(uint32_t baseHash) const
		{
			for (const auto& base : bases)
			{
				if (base.first == baseHash)
					return true;

				const RTTI::Type* baseType = getType(base.first);

				if (baseType && baseType->hasBase(baseHash))
					return true;
			}

			return false;
		}

		std::pair<bool, int32_t> RTTI::Type::getBaseOffset(uint32_t baseHash) const
		{
			for (const auto& base : bases)
			{
				if (base.first == baseHash)
					return std::pair<bool, int32_t>(true, base.second);

				if (const RTTI::Type* baseType = getType(base.first))
				{
					std::pair<bool, int32_t> result = baseType->getBaseOffset(baseHash);

					if (result.first)
						return std::pair<bool, int32_t>(true, result.second + base.second);
				}
			}

			return std::pair<bool, int32_t>(false, 0);
		}

		bool RTTI::Type::hasVariable(const std::string& variable) const
		{
			if (variables.find(variable) != variables.end())
				return true;

			for (const auto& base : bases)
			{
				const RTTI::Type* baseType = getType(base.first);

				if (baseType && baseType->hasVariable(variable))
					return true;
			}

			return false;
		}

		std::tuple<bool, int32_t, const RTTI::Type*, bool, bool(*)(IO::BitStream const&, IO::BitStream const&), void (*)(const char*, const char*, float, char*), void (*)(const char*, const char*, IO::BitStream&), void (*)(IO::BitStream const&, IO::BitStream const&, char*), void(*)(char*), void(*)(char*)> RTTI::Type::getVariable(const std::string& variable) const
		{
			auto variableIter = variables.find(variable);
			if (variableIter != variables.end())
				return std::make_tuple(true, std::get<1>(variableIter->second), getType(std::get<0>(variableIter->second)), std::get<2>(variableIter->second), std::get<3>(variableIter->second), std::get<4>(variableIter->second), std::get<5>(variableIter->second), std::get<6>(variableIter->second), std::get<7>(variableIter->second), std::get<8>(variableIter->second));

			std::deque<std::pair<uint32_t, int32_t>> baseQueue;

			for (const auto& base : bases)
				baseQueue.push_back(base);

			while (!baseQueue.empty())
			{
				auto& base = baseQueue.front();

				if (const RTTI::Type* baseType = getType(base.first))
				{
					variableIter = baseType->variables.find(variable);

					if (variableIter != baseType->variables.end())
						return std::make_tuple(true, std::get<1>(variableIter->second) + base.second, getType(std::get<0>(variableIter->second)), std::get<2>(variableIter->second), std::get<3>(variableIter->second), std::get<4>(variableIter->second), std::get<5>(variableIter->second), std::get<6>(variableIter->second), std::get<7>(variableIter->second), std::get<8>(variableIter->second));

					for (const auto& basePair : baseType->bases)
						baseQueue.push_back(basePair);
				}

				baseQueue.pop_front();
			}

			return std::make_tuple(false, 0, nullptr, false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		}

		std::tuple<bool, char*, const RTTI::Type*, bool, bool(*)(IO::BitStream const&, IO::BitStream const&), void (*)(const char*, const char*, float, char*), void (*)(const char*, const char*, IO::BitStream&), void (*)(IO::BitStream const&, IO::BitStream const&, char*), void(*)(char*), void(*)(char*)> RTTI::Type::getVariable(char* data, const std::string& variable) const
		{
			size_t splitIndex = variable.find('.');
			std::string var = splitIndex != variable.npos ? variable.substr(0, splitIndex) : variable;
			std::string remainder = splitIndex != variable.npos && splitIndex + 1 < variable.size() ? variable.substr(splitIndex + 1) : "";

			std::tuple<bool, int32_t, const Type*, bool, bool(*)(IO::BitStream const&, IO::BitStream const&), void (*)(const char*, const char*, float, char*), void (*)(const char*, const char*, IO::BitStream&), void (*)(IO::BitStream const&, IO::BitStream const&, char*), void(*)(char*), void(*)(char*)> v = getVariable(var);

			if (std::get<0>(v))
			{
				int32_t offset = std::get<1>(v);
				const Type* type = std::get<2>(v);
				bool isPointer = std::get<3>(v);
				auto comparator = std::get<4>(v);
				auto interpolator = std::get<5>(v);
				auto difference = std::get<6>(v);
				auto accumulator = std::get<7>(v);
				auto premodifycallback = std::get<8>(v);
				auto postmodifycallback = std::get<9>(v);

				if (splitIndex == variable.npos)
					return std::make_tuple(true, data + offset, type, isPointer, comparator, interpolator, difference, accumulator, premodifycallback, postmodifycallback);

				if (type)
				{
					if (char* offsetData = !isPointer ? data + offset : *reinterpret_cast<char**>(data + offset))
						return type->getVariable(offsetData, remainder);
				}
			}

			return std::make_tuple(false, nullptr, nullptr, false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		}

		std::unordered_map<std::string, std::tuple<char*, const RTTI::Type*, bool, bool(*)(IO::BitStream const&, IO::BitStream const&), void (*)(const char*, const char*, float, char*), void (*)(const char*, const char*, IO::BitStream&), void (*)(IO::BitStream const&, IO::BitStream const&, char*), void(*)(char*), void(*)(char*)>> RTTI::Type::getVariables(char* data, bool onlyLeaves) const
		{
			std::unordered_map<std::string, std::tuple<char*, const RTTI::Type*, bool, bool(*)(IO::BitStream const&, IO::BitStream const&), void (*)(const char*, const char*, float, char*), void (*)(const char*, const char*, IO::BitStream&), void (*)(IO::BitStream const&, IO::BitStream const&, char*), void(*)(char*), void(*)(char*)>> result;

			if (!variables.empty())
			{
				std::deque<std::tuple<const Type*, char*, std::string>> types;
				types.emplace_back(std::make_tuple(this, data, ""));

				while (!types.empty())
				{
					auto& node = types.front();
					auto& ptr = std::get<1>(node);
					auto& prefix = std::get<2>(node);

					for (const auto& var : std::get<0>(node)->variables)
					{
						const std::string& varName = var.first;
						std::string absoluteName = prefix.empty() ? varName : prefix + "." + varName;

						uint32_t typeHash = std::get<0>(var.second);
						int32_t offset = std::get<1>(var.second);
						bool isPointer = std::get<2>(var.second);
						auto comparator = std::get<3>(var.second);
						auto interpolator = std::get<4>(var.second);
						auto difference = std::get<5>(var.second);
						auto accumulator = std::get<6>(var.second);
						auto premodifycallback = std::get<7>(var.second);
						auto postmodifycallback = std::get<8>(var.second);

						const Type* type = getType(typeHash);
						char* memory = !isPointer ? ptr + offset : *reinterpret_cast<char**>(ptr + offset);

						if (type && memory && !type->variables.empty())
						{
							if ((onlyLeaves && result.find(absoluteName) == result.end()) || result.emplace(absoluteName, std::make_tuple(memory, type, isPointer, comparator, interpolator, difference, accumulator, premodifycallback, postmodifycallback)).second)
								types.emplace_back(std::make_tuple(type, memory, absoluteName));
						}
						else result.emplace(absoluteName, std::make_tuple(!memory ? ptr + offset : memory, type, isPointer, comparator, interpolator, difference, accumulator, premodifycallback, postmodifycallback));
					}

					types.pop_front();
				}
			}

			for (auto& base : bases)
			{
				if (auto baseType = getType(base.first))
					result.merge(baseType->getVariables(data + base.second, onlyLeaves));
			}

			return result;
		}

		std::unordered_set<std::string> RTTI::Type::getVariableNames(bool onlyLeaves) const
		{
			std::unordered_set<std::string> result;

			if (!variables.empty())
			{
				std::deque<std::pair<const Type*, std::string>> types;
				types.emplace_back(std::make_pair(this, ""));

				while (!types.empty())
				{
					auto& node = types.front();
					auto& prefix = node.second;

					for (const auto& var : node.first->variables)
					{
						const std::string& varName = var.first;
						std::string absoluteName = prefix.empty() ? varName : prefix + "." + varName;

						uint32_t typeHash = std::get<0>(var.second);
						const Type* type = getType(typeHash);

						if (type && !type->variables.empty())
						{
							if ((onlyLeaves && result.find(absoluteName) == result.end()) || result.emplace(absoluteName).second)
							{
								types.emplace_back(std::make_pair(type, absoluteName));

								for (auto baseType : type->getBases())
									types.emplace_back(std::make_pair(baseType, absoluteName));
							}
						}
						else result.emplace(absoluteName);
					}

					types.pop_front();
				}
			}

			for (auto& base : bases)
			{
				if (auto baseType = getType(base.first))
					result.merge(baseType->getVariableNames(onlyLeaves));
			}

			return result;
		}

		std::unordered_set<std::string> RTTI::Type::getVariableNames(char* data, bool onlyLeaves) const
		{
			std::unordered_set<std::string> result;

			if (!variables.empty())
			{
				std::deque<std::tuple<const Type*, char*, std::string>> types;
				types.emplace_back(std::make_tuple(this, data, ""));

				while (!types.empty())
				{
					auto& node = types.front();
					auto& ptr = std::get<1>(node);
					auto& prefix = std::get<2>(node);

					for (const auto& var : std::get<0>(node)->variables)
					{
						const std::string& varName = var.first;
						std::string absoluteName = prefix.empty() ? varName : prefix + "." + varName;

						uint32_t typeHash = std::get<0>(var.second);
						int32_t offset = std::get<1>(var.second);
						bool isPointer = std::get<2>(var.second);

						const Type* type = getType(typeHash);
						char* memory = !isPointer ? ptr + offset : *reinterpret_cast<char**>(ptr + offset);

						if (type && memory && !type->variables.empty())
						{
							if ((onlyLeaves && result.find(absoluteName) == result.end()) || result.emplace(absoluteName).second)
							{
								types.emplace_back(std::make_tuple(type, memory, absoluteName));

								for (auto baseType : type->getBases())
									types.emplace_back(std::make_tuple(baseType, memory + type->getBaseOffset(baseType->getHash()).second, absoluteName));
							}
						}
						else result.emplace(absoluteName);
					}

					types.pop_front();
				}
			}

			for (auto& base : bases)
			{
				if (auto baseType = getType(base.first))
					result.merge(baseType->getVariableNames(data + base.second, onlyLeaves));
			}

			return result;
		}

		const RTTI::Type* RTTI::getType(uint32_t typeHash)
		{
			auto& types = getTypes();
			auto iter = types.find(typeHash);
			return iter != types.end() ? &iter->second : nullptr;
		}

		std::unordered_map<uint32_t, RTTI::Type>& RTTI::getTypes()
		{
			static std::unordered_map<uint32_t, Type> types;
			return types;
		}
	}
}