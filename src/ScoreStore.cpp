#include "ScoreStore.h"

#include <algorithm>
#include <fstream>
#include <unordered_set>

#include <json.hpp>

namespace
{
using json = nlohmann::json;

void AddError(std::vector<std::string>& errors, const std::string& message)
{
	errors.push_back(message);
}

bool ValidateObjectKeys(
	const json& object,
	const std::unordered_set<std::string>& allowed_keys,
	std::vector<std::string>& errors,
	const std::string& context)
{
	bool valid = true;

	for (const auto& [key, unused_value] : object.items())
	{
		static_cast<void>(unused_value);

		if (allowed_keys.find(key) == allowed_keys.end())
		{
			AddError(errors, context + " contains unknown field '" + key + "'");
			valid = false;
		}
	}

	return valid;
}
} // namespace

namespace ExoTribeScore
{
ScoreStore::ScoreStore(std::filesystem::path path)
	: path_(std::move(path))
{
}

ScoreLoadResult ScoreStore::Load() const
{
	ScoreLoadResult result;
	result.success = true;

	std::ifstream file(path_);
	if (!file.is_open())
	{
		return result;
	}

	json root;
	try
	{
		file >> root;
	}
	catch (const std::exception& exception)
	{
		result.success = false;
		AddError(result.errors, "Failed to parse score JSON: " + std::string(exception.what()));
		return result;
	}

	if (!root.is_object())
	{
		result.success = false;
		AddError(result.errors, "Score root value must be a JSON object");
		return result;
	}

	ValidateObjectKeys(root, { "version", "tribes" }, result.errors, "Score root");

	if (!root.contains("version"))
	{
		AddError(result.errors, "Score root is missing required field 'version'");
	}
	else if (!root["version"].is_number_integer())
	{
		AddError(result.errors, "Score field 'version' must be an integer");
	}
	else if (root["version"].get<int>() != 1)
	{
		AddError(result.errors, "Score field 'version' must be set to 1");
	}

	if (!root.contains("tribes"))
	{
		AddError(result.errors, "Score root is missing required field 'tribes'");
	}
	else if (!root["tribes"].is_array())
	{
		AddError(result.errors, "Score field 'tribes' must be an array");
	}
	else
	{
		std::unordered_set<int> seen_tribes;

		for (size_t index = 0; index < root["tribes"].size(); ++index)
		{
			const auto& entry = root["tribes"][index];
			const std::string context = "Score entry #" + std::to_string(index);

			if (!entry.is_object())
			{
				AddError(result.errors, context + " must be a JSON object");
				continue;
			}

			ValidateObjectKeys(entry, { "tribe_id", "last_known_name", "score", "updated_at" }, result.errors, context);

			if (!entry.contains("tribe_id") || !entry["tribe_id"].is_number_integer())
			{
				AddError(result.errors, context + " field 'tribe_id' must be an integer");
				continue;
			}

			const int tribe_id = entry["tribe_id"].get<int>();
			if (tribe_id <= 0)
			{
				AddError(result.errors, context + " field 'tribe_id' must be greater than 0");
				continue;
			}

			if (!seen_tribes.insert(tribe_id).second)
			{
				AddError(result.errors, context + " duplicates tribe_id '" + std::to_string(tribe_id) + "'");
				continue;
			}

			if (!entry.contains("score") || !entry["score"].is_number_integer())
			{
				AddError(result.errors, context + " field 'score' must be an integer");
				continue;
			}

			TribeScoreEntry score_entry;
			score_entry.tribe_id = tribe_id;
			score_entry.score = entry["score"].get<int>();

			if (entry.contains("last_known_name"))
			{
				if (!entry["last_known_name"].is_string())
				{
					AddError(result.errors, context + " field 'last_known_name' must be a string");
					continue;
				}

				score_entry.last_known_name = entry["last_known_name"].get<std::string>();
			}

			if (entry.contains("updated_at"))
			{
				if (!entry["updated_at"].is_string())
				{
					AddError(result.errors, context + " field 'updated_at' must be a string");
					continue;
				}

				score_entry.updated_at = entry["updated_at"].get<std::string>();
			}

			result.scores.emplace(score_entry.tribe_id, std::move(score_entry));
		}
	}

	if (!result.errors.empty())
	{
		result.success = false;
		result.scores.clear();
	}

	return result;
}

bool ScoreStore::Save(
	const std::unordered_map<int, TribeScoreEntry>& scores,
	std::vector<std::string>* errors) const
{
	std::filesystem::create_directories(path_.parent_path());

	std::vector<TribeScoreEntry> entries;
	entries.reserve(scores.size());

	for (const auto& [tribe_id, entry] : scores)
	{
		static_cast<void>(tribe_id);
		entries.push_back(entry);
	}

	std::sort(entries.begin(), entries.end(), [](const TribeScoreEntry& left, const TribeScoreEntry& right)
	{
		return left.tribe_id < right.tribe_id;
	});

	json root;
	root["version"] = 1;
	root["tribes"] = json::array();

	for (const TribeScoreEntry& entry : entries)
	{
		root["tribes"].push_back(
			{
				{ "tribe_id", entry.tribe_id },
				{ "last_known_name", entry.last_known_name },
				{ "score", entry.score },
				{ "updated_at", entry.updated_at }
			});
	}

	std::ofstream file(path_, std::ios::trunc);
	if (!file.is_open())
	{
		if (errors != nullptr)
		{
			AddError(*errors, "Failed to open score file for writing: " + path_.string());
		}

		return false;
	}

	try
	{
		file << root.dump(2);
		file << '\n';
	}
	catch (const std::exception& exception)
	{
		if (errors != nullptr)
		{
			AddError(*errors, "Failed to write score file: " + std::string(exception.what()));
		}

		return false;
	}

	return true;
}

const std::filesystem::path& ScoreStore::GetPath() const
{
	return path_;
}
} // namespace ExoTribeScore
