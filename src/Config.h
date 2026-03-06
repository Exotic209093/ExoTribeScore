#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <API/ARK/Ark.h>

namespace ExoTribeScore
{
struct StructureScoreRule
{
	std::string blueprint;
	int score_value = 0;
};

struct FloatingTextSettings
{
	FColor positive_color = FColor(90, 220, 90, 255);
	FColor negative_color = FColor(220, 90, 90, 255);
	float scale_x = 1.0f;
	float scale_y = 1.0f;
	float lifespan = 3.0f;
	float min_scale = 0.6f;
	float fade_in_time = 0.1f;
	float fade_out_time = 0.5f;
	FVector velocity = FVector(0.0f, 0.0f, 40.0f);
};

struct MessageTemplates
{
	std::string attacker = "+{amount} Tribe Score";
	std::string victim = "-{amount} Tribe Score";
};

struct ScoreConfig
{
	int version = 1;
	int score_floor = 0;
	int leaderboard_limit = 10;
	FloatingTextSettings floating_text;
	MessageTemplates messages;
	std::unordered_map<std::string, StructureScoreRule> structures;
};

struct ConfigLoadResult
{
	bool success = false;
	std::shared_ptr<const ScoreConfig> config;
	std::vector<std::string> errors;
};

class ConfigService
{
public:
	explicit ConfigService(std::filesystem::path config_path);

	ConfigLoadResult Load() const;
	const std::filesystem::path& GetPath() const;

	static std::string NormalizeBlueprintPath(const std::string& value, std::string* error_message = nullptr);

private:
	std::filesystem::path config_path_;
};
} // namespace ExoTribeScore
