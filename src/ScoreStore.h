#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Config.h"

namespace ExoTribeScore
{
struct TribeScoreEntry
{
	int tribe_id = 0;
	std::string last_known_name;
	int score = 0;
	std::string updated_at;
};

struct TribeScoreEventRecord
{
	int attacker_tribe_id = 0;
	int victim_tribe_id = 0;
	std::string blueprint;
	int delta = 0;
	std::string map_name;
	std::string created_at;
	int attacker_score = 0;
	int victim_score = 0;
};

struct TribeMemberRecord
{
	int tribe_id = 0;
	uint64_t player_data_id = 0;
	std::string player_name;
	int rank_group = 0;
	std::string updated_at;
};

struct ScoreLoadResult
{
	bool success = false;
	std::unordered_map<int, TribeScoreEntry> scores;
	std::vector<std::string> errors;
};

struct ScoreChangeRecord
{
	TribeScoreEntry attacker_entry;
	TribeScoreEntry victim_entry;
	TribeScoreEventRecord event_record;
	bool replace_attacker_members = false;
	bool replace_victim_members = false;
	std::vector<TribeMemberRecord> attacker_members;
	std::vector<TribeMemberRecord> victim_members;
};

class ScoreStore
{
public:
	ScoreStore() = default;

	void Configure(const MySqlConnectionSettings& connection_settings);
	bool TestConnection(std::vector<std::string>* errors = nullptr) const;
	ScoreLoadResult Load() const;
	bool UpsertScores(const std::vector<TribeScoreEntry>& scores, std::vector<std::string>* errors = nullptr) const;
	std::vector<TribeScoreEntry> LoadTopScores(int limit, std::vector<std::string>* errors = nullptr) const;
	bool ApplyScoreChange(const ScoreChangeRecord& score_change, std::vector<std::string>* errors = nullptr) const;
	std::string GetConnectionSummary() const;

private:
	MySqlConnectionSettings GetConnectionSettings() const;
	std::string GetQualifiedTableName(const char* base_name) const;

	mutable std::mutex mutex_;
	MySqlConnectionSettings connection_settings_;
};
} // namespace ExoTribeScore
