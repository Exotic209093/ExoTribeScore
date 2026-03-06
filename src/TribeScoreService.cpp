#include "TribeScoreService.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

#include <Ark/ArkApiUtils.h>
#include <Logger/Logger.h>

namespace
{
std::string ToLowerAscii(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
	{
		return static_cast<char>(std::tolower(character));
	});

	return value;
}

void ReplaceAll(std::string& value, const std::string& from, const std::string& to)
{
	if (from.empty())
	{
		return;
	}

	size_t position = 0;
	while ((position = value.find(from, position)) != std::string::npos)
	{
		value.replace(position, from.length(), to);
		position += to.length();
	}
}

std::string MakeTimestampUtc()
{
	const std::time_t now = std::time(nullptr);
	std::tm utc_time{};
	gmtime_s(&utc_time, &now);

	std::ostringstream stream;
	stream << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%SZ");
	return stream.str();
}

FString ToFString(const std::string& value)
{
	return FString(value.c_str());
}

FLinearColor ToLinearColor(const FColor& color)
{
	return FLinearColor(
		static_cast<float>(color.R) / 255.0f,
		static_cast<float>(color.G) / 255.0f,
		static_cast<float>(color.B) / 255.0f,
		static_cast<float>(color.A) / 255.0f);
}
} // namespace

namespace ExoTribeScore
{
TribeScoreService::TribeScoreService(std::filesystem::path config_path, std::filesystem::path scores_path)
	: config_service_(std::move(config_path)),
	  score_store_(std::move(scores_path)),
	  active_config_(std::make_shared<ScoreConfig>())
{
}

void TribeScoreService::LoadInitialState()
{
	LoadScores();
	ReloadConfigInternal(true);
}

FString TribeScoreService::ReloadConfig()
{
	return ReloadConfigInternal(false);
}

TribeScoreService::StructureDestructionContext TribeScoreService::CaptureDestructionContext(
	APrimalStructure* structure,
	AController* killer,
	AActor* damage_causer) const
{
	StructureDestructionContext context;

	if (structure == nullptr)
	{
		return context;
	}

	const auto config = GetConfigSnapshot();
	if (!config || config->structures.empty())
	{
		return context;
	}

	const int victim_tribe_id = structure->TargetingTeamField();
	if (victim_tribe_id <= 0)
	{
		return context;
	}

	const std::string blueprint = GetStructureBlueprint(structure);
	if (blueprint.empty())
	{
		return context;
	}

	const auto rule_iterator = config->structures.find(blueprint);
	if (rule_iterator == config->structures.end())
	{
		return context;
	}

	const AttackContext attack_context = ResolveAttackContext(structure, killer, damage_causer);
	if (attack_context.tribe_id <= 0 || attack_context.tribe_id == victim_tribe_id)
	{
		return context;
	}

	context.config = config;
	context.location = structure->DefaultActorLocationField();
	context.attacker_tribe_id = attack_context.tribe_id;
	context.victim_tribe_id = victim_tribe_id;
	context.score_value = rule_iterator->second.score_value;
	context.valid = true;

	if (attack_context.player_controller != nullptr)
	{
		context.attacker_player_controller = GetWeakReference(attack_context.player_controller);
	}

	return context;
}

void TribeScoreService::ProcessDestroyedStructure(const StructureDestructionContext& context, const bool destruction_succeeded)
{
	if (!destruction_succeeded || !context.valid || !context.config)
	{
		return;
	}

	const std::string attacker_tribe_name = ResolveTribeName(context.attacker_tribe_id);
	const std::string victim_tribe_name = ResolveTribeName(context.victim_tribe_id);
	const std::string updated_at = MakeTimestampUtc();

	ScoreEvent event;
	event.attacker_tribe_id = context.attacker_tribe_id;
	event.victim_tribe_id = context.victim_tribe_id;
	event.amount = context.score_value;
	event.attacker_tribe_name = attacker_tribe_name.empty()
		? "Tribe " + std::to_string(context.attacker_tribe_id)
		: attacker_tribe_name;
	event.victim_tribe_name = victim_tribe_name.empty()
		? "Tribe " + std::to_string(context.victim_tribe_id)
		: victim_tribe_name;

	std::unordered_map<int, TribeScoreEntry> snapshot_to_save;
	{
		std::lock_guard<std::mutex> lock(mutex_);

		TribeScoreEntry& attacker_entry = scores_[context.attacker_tribe_id];
		attacker_entry.tribe_id = context.attacker_tribe_id;
		if (!attacker_tribe_name.empty())
		{
			attacker_entry.last_known_name = attacker_tribe_name;
		}

		attacker_entry.score += context.score_value;
		attacker_entry.updated_at = updated_at;
		event.attacker_score = attacker_entry.score;

		TribeScoreEntry& victim_entry = scores_[context.victim_tribe_id];
		victim_entry.tribe_id = context.victim_tribe_id;
		if (!victim_tribe_name.empty())
		{
			victim_entry.last_known_name = victim_tribe_name;
		}

		const int clamped_victim_score = (std::max)(
			context.config->score_floor,
			victim_entry.score - context.score_value);
		victim_entry.score = clamped_victim_score;
		victim_entry.updated_at = updated_at;
		event.victim_score = victim_entry.score;

		snapshot_to_save = scores_;
	}

	PersistScoresSnapshot(snapshot_to_save);

	auto attacker_player_reference = context.attacker_player_controller;
	AShooterPlayerController* attacker_player_controller = attacker_player_reference.Get();
	if (attacker_player_controller != nullptr)
	{
		const std::string attacker_message =
			FormatMessage(context.config->messages.attacker, event, true);
		ShowFloatingText(
			attacker_player_controller,
			context.location,
			attacker_message,
			context.config->floating_text.positive_color,
			context.config->floating_text);
	}

	const std::vector<AShooterPlayerController*> victim_members =
		GetNearbyOnlineTribeMembers(context.victim_tribe_id, context.location);
	const std::string victim_message =
		FormatMessage(context.config->messages.victim, event, false);

	for (AShooterPlayerController* player_controller : victim_members)
	{
		ShowFloatingText(
			player_controller,
			context.location,
			victim_message,
			context.config->floating_text.negative_color,
			context.config->floating_text);
	}
}

void TribeScoreService::ShowLeaderboard(AShooterPlayerController* player_controller) const
{
	if (player_controller == nullptr)
	{
		return;
	}

	ShowPopup(player_controller, BuildLeaderboardNotificationText(), FLinearColor(1.0f, 0.85f, 0.2f, 1.0f));
}

void TribeScoreService::Shutdown()
{
	std::lock_guard<std::mutex> lock(mutex_);
	active_config_ = std::make_shared<ScoreConfig>();
	scores_.clear();
}

FString TribeScoreService::ReloadConfigInternal(const bool startup)
{
	const ConfigLoadResult load_result = config_service_.Load();
	if (!load_result.success)
	{
		Log::GetLog()->error("Failed to load ExoTribeScore config from {}", config_service_.GetPath().string());
		for (const auto& error : load_result.errors)
		{
			Log::GetLog()->error("Config validation error: {}", error);
		}

		if (startup)
		{
			Log::GetLog()->warn("ExoTribeScore started with no active structure score rules");
			return "ExoTribeScore config failed to load. Scoring rules are inactive.";
		}

		return FString::Format(
			"ExoTribeScore reload failed with {} validation error(s). Previous config kept.",
			static_cast<int>(load_result.errors.size()));
	}

	{
		std::lock_guard<std::mutex> lock(mutex_);
		active_config_ = load_result.config;
	}

	ClampScoresToFloor(load_result.config->score_floor);

	const int structure_count = static_cast<int>(load_result.config->structures.size());
	if (startup)
	{
		Log::GetLog()->info(
			"Loaded ExoTribeScore config from {} with {} structure rule(s)",
			config_service_.GetPath().string(),
			structure_count);
		return FString::Format("ExoTribeScore loaded {} scoring rule(s).", structure_count);
	}

	Log::GetLog()->info(
		"Reloaded ExoTribeScore config from {} with {} structure rule(s)",
		config_service_.GetPath().string(),
		structure_count);
	return FString::Format("ExoTribeScore reloaded {} scoring rule(s).", structure_count);
}

void TribeScoreService::LoadScores()
{
	const ScoreLoadResult load_result = score_store_.Load();
	if (!load_result.success)
	{
		Log::GetLog()->error("Failed to load ExoTribeScore scores from {}", score_store_.GetPath().string());
		for (const auto& error : load_result.errors)
		{
			Log::GetLog()->error("Score validation error: {}", error);
		}

		std::lock_guard<std::mutex> lock(mutex_);
		scores_.clear();
		return;
	}

	{
		std::lock_guard<std::mutex> lock(mutex_);
		scores_ = load_result.scores;
	}

	Log::GetLog()->info(
		"Loaded ExoTribeScore scores from {} with {} persisted tribe entries",
		score_store_.GetPath().string(),
		static_cast<int>(load_result.scores.size()));
}

void TribeScoreService::ClampScoresToFloor(const int score_floor)
{
	bool changed = false;
	std::unordered_map<int, TribeScoreEntry> snapshot_to_save;

	{
		std::lock_guard<std::mutex> lock(mutex_);
		for (auto& [tribe_id, entry] : scores_)
		{
			static_cast<void>(tribe_id);

			if (entry.score < score_floor)
			{
				entry.score = score_floor;
				entry.updated_at = MakeTimestampUtc();
				changed = true;
			}
		}

		if (changed)
		{
			snapshot_to_save = scores_;
		}
	}

	if (changed)
	{
		PersistScoresSnapshot(snapshot_to_save);
	}
}

std::shared_ptr<const ScoreConfig> TribeScoreService::GetConfigSnapshot() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return active_config_;
}

std::string TribeScoreService::GetStructureBlueprint(APrimalStructure* structure) const
{
	if (structure == nullptr || structure->ClassField() == nullptr)
	{
		return {};
	}

	FString class_path;
	structure->ClassField()->GetPathName(&class_path, nullptr);

	std::string normalization_error;
	std::string blueprint = ConfigService::NormalizeBlueprintPath(class_path.ToString(), &normalization_error);
	if (!normalization_error.empty())
	{
		Log::GetLog()->warn(
			"Unable to normalize runtime structure class path '{}': {}",
			class_path.ToString(),
			normalization_error);
		return {};
	}

	return blueprint;
}

TribeScoreService::AttackContext TribeScoreService::ResolveAttackContext(
	APrimalStructure* victim,
	AController* killer,
	AActor* damage_causer) const
{
	AttackContext context;
	if (victim == nullptr)
	{
		return context;
	}

	if (killer != nullptr
		&& killer->IsA(AShooterPlayerController::GetPrivateStaticClass())
		&& !killer->IsLocalController())
	{
		auto* shooter_player_controller = static_cast<AShooterPlayerController*>(killer);
		context.player_controller = shooter_player_controller;
		context.tribe_id = shooter_player_controller->TargetingTeamField();
	}
	else if (damage_causer != nullptr && damage_causer->IsA(APrimalStructureExplosive::StaticClass()))
	{
		auto* explosive = static_cast<APrimalStructureExplosive*>(damage_causer);
		context.tribe_id = explosive->ConstructorTargetingTeamField();

		const uint64 attacker_steam_id =
			ArkApi::GetApiUtils().GetAttackerSteamID(victim, killer, damage_causer, true);
		if (attacker_steam_id != 0)
		{
			context.player_controller = ArkApi::GetApiUtils().FindPlayerFromSteamId(attacker_steam_id);
		}
	}
	else
	{
		if (killer != nullptr)
		{
			context.tribe_id = killer->TargetingTeamField();
		}

		if (context.tribe_id <= 0 && damage_causer != nullptr)
		{
			context.tribe_id = damage_causer->TargetingTeamField();
		}
	}

	if (context.tribe_id <= 0 || context.tribe_id == victim->TargetingTeamField())
	{
		return {};
	}

	return context;
}

std::string TribeScoreService::ResolveTribeName(const int tribe_id, const std::string& fallback) const
{
	if (tribe_id <= 0)
	{
		return fallback;
	}

	AShooterGameMode* game_mode = ArkApi::GetApiUtils().GetShooterGameMode();
	if (game_mode == nullptr)
	{
		return fallback;
	}

	FTribeData tribe_data{};
	if (!game_mode->GetOrLoadTribeData(tribe_id, &tribe_data))
	{
		return fallback;
	}

	const std::string tribe_name = tribe_data.TribeNameField().ToString();
	if (tribe_name.empty())
	{
		return fallback;
	}

	return tribe_name;
}

void TribeScoreService::PersistScoresSnapshot(const std::unordered_map<int, TribeScoreEntry>& snapshot) const
{
	std::vector<std::string> errors;
	if (score_store_.Save(snapshot, &errors))
	{
		return;
	}

	Log::GetLog()->error("Failed to save ExoTribeScore scores to {}", score_store_.GetPath().string());
	for (const auto& error : errors)
	{
		Log::GetLog()->error("Score persistence error: {}", error);
	}
}

void TribeScoreService::RefreshStoredTribeName(const int tribe_id, const std::string& live_name) const
{
	if (tribe_id <= 0 || live_name.empty())
	{
		return;
	}

	bool changed = false;
	std::unordered_map<int, TribeScoreEntry> snapshot_to_save;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto iterator = scores_.find(tribe_id);
		if (iterator == scores_.end() || iterator->second.last_known_name == live_name)
		{
			return;
		}

		iterator->second.last_known_name = live_name;
		snapshot_to_save = scores_;
		changed = true;
	}

	if (changed)
	{
		PersistScoresSnapshot(snapshot_to_save);
	}
}

float TribeScoreService::GetVictimNotificationRange() const
{
	AShooterGameState* game_state = ArkApi::GetApiUtils().GetGameState();
	if (game_state == nullptr || game_state->FloatingHUDRangeField() <= 0.0f)
	{
		return 10000.0f;
	}

	return game_state->FloatingHUDRangeField();
}

std::vector<AShooterPlayerController*> TribeScoreService::GetNearbyOnlineTribeMembers(
	const int tribe_id,
	const FVector& location) const
{
	std::vector<AShooterPlayerController*> players;
	if (tribe_id <= 0)
	{
		return players;
	}

	const float range = GetVictimNotificationRange();
	const auto& player_controllers = ArkApi::GetApiUtils().GetWorld()->PlayerControllerListField();
	for (TWeakObjectPtr<APlayerController> player_controller_ref : player_controllers)
	{
		auto* player_controller = static_cast<AShooterPlayerController*>(player_controller_ref.Get());
		if (player_controller == nullptr)
		{
			continue;
		}

		if (player_controller->TargetingTeamField() != tribe_id)
		{
			continue;
		}

		AShooterCharacter* player_character = player_controller->GetPlayerCharacter();
		if (player_character == nullptr || player_character->IsDead())
		{
			continue;
		}

		if (FVector::Distance(player_character->DefaultActorLocationField(), location) > range)
		{
			continue;
		}

		players.push_back(player_controller);
	}

	return players;
}

void TribeScoreService::ShowFloatingText(
	AShooterPlayerController* player_controller,
	const FVector& location,
	const std::string& message,
	const FColor& color,
	const FloatingTextSettings& settings) const
{
	if (player_controller == nullptr || message.empty())
	{
		return;
	}

	FString floating_text = ToFString(message);
	player_controller->ClientAddFloatingText(
		FVector_NetQuantize(location),
		&floating_text,
		color,
		settings.scale_x,
		settings.scale_y,
		settings.lifespan,
		settings.velocity,
		settings.min_scale,
		settings.fade_in_time,
		settings.fade_out_time);
}

void TribeScoreService::ShowPopup(
	AShooterPlayerController* player_controller,
	const FString& message,
	const FLinearColor& color) const
{
	if (player_controller == nullptr)
	{
		return;
	}

	FString popup_text(message);
	player_controller->ClientServerSOTFNotificationCustom(&popup_text, color, 1.0f, 10.0f, nullptr, nullptr);
}

FString TribeScoreService::BuildLeaderboardNotificationText() const
{
	struct LeaderboardEntry
	{
		int tribe_id = 0;
		std::string tribe_name;
		int score = 0;
	};

	const auto config = GetConfigSnapshot();
	const int leaderboard_limit = config ? config->leaderboard_limit : 10;

	std::vector<TribeScoreEntry> score_entries;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		score_entries.reserve(scores_.size());
		for (const auto& [tribe_id, entry] : scores_)
		{
			static_cast<void>(tribe_id);
			score_entries.push_back(entry);
		}
	}

	if (score_entries.empty())
	{
		return "No tribe scores recorded yet.";
	}

	std::vector<LeaderboardEntry> leaderboard_entries;
	leaderboard_entries.reserve(score_entries.size());

	for (const TribeScoreEntry& entry : score_entries)
	{
		const std::string live_name = ResolveTribeName(entry.tribe_id, entry.last_known_name);
		if (!live_name.empty() && live_name != entry.last_known_name)
		{
			RefreshStoredTribeName(entry.tribe_id, live_name);
		}

		LeaderboardEntry leaderboard_entry;
		leaderboard_entry.tribe_id = entry.tribe_id;
		leaderboard_entry.tribe_name = live_name.empty()
			? "Tribe " + std::to_string(entry.tribe_id)
			: live_name;
		leaderboard_entry.score = entry.score;
		leaderboard_entries.push_back(std::move(leaderboard_entry));
	}

	std::sort(leaderboard_entries.begin(), leaderboard_entries.end(), [](const LeaderboardEntry& left, const LeaderboardEntry& right)
	{
		if (left.score != right.score)
		{
			return left.score > right.score;
		}

		const std::string left_name = ToLowerAscii(left.tribe_name);
		const std::string right_name = ToLowerAscii(right.tribe_name);
		if (left_name != right_name)
		{
			return left_name < right_name;
		}

		return left.tribe_id < right.tribe_id;
	});

	std::ostringstream stream;
	stream << "Tribe Leaderboard";

	const int result_count = (std::min)(leaderboard_limit, static_cast<int>(leaderboard_entries.size()));
	for (int index = 0; index < result_count; ++index)
	{
		const LeaderboardEntry& entry = leaderboard_entries[index];
		stream << '\n'
			<< (index + 1)
			<< ". "
			<< entry.tribe_name
			<< " - "
			<< entry.score;
	}

	return ToFString(stream.str());
}

std::string TribeScoreService::FormatMessage(
	const std::string& format,
	const ScoreEvent& event,
	const bool is_attacker) const
{
	std::string message = format;
	const int tribe_score = is_attacker ? event.attacker_score : event.victim_score;
	const std::string signed_amount = is_attacker
		? "+" + std::to_string(event.amount)
		: "-" + std::to_string(event.amount);

	ReplaceAll(message, "{amount}", std::to_string(event.amount));
	ReplaceAll(message, "{signed_amount}", signed_amount);
	ReplaceAll(message, "{score}", std::to_string(tribe_score));
	ReplaceAll(message, "{attacker_tribe}", event.attacker_tribe_name);
	ReplaceAll(message, "{victim_tribe}", event.victim_tribe_name);
	return message;
}
} // namespace ExoTribeScore
