#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace ExoTribeScore
{
struct TribeScoreEntry
{
	int tribe_id = 0;
	std::string last_known_name;
	int score = 0;
	std::string updated_at;
};

struct ScoreLoadResult
{
	bool success = false;
	std::unordered_map<int, TribeScoreEntry> scores;
	std::vector<std::string> errors;
};

class ScoreStore
{
public:
	explicit ScoreStore(std::filesystem::path path);

	ScoreLoadResult Load() const;
	bool Save(const std::unordered_map<int, TribeScoreEntry>& scores, std::vector<std::string>* errors = nullptr) const;
	const std::filesystem::path& GetPath() const;

private:
	std::filesystem::path path_;
};
} // namespace ExoTribeScore
