#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <API/ARK/Ark.h>

#include "Config.h"
#include "ScoreStore.h"

namespace ExoTribeScore
{
class TribeScoreService
{
public:
	struct StructureDestructionContext
	{
		std::shared_ptr<const ScoreConfig> config;
		TWeakObjectPtr<AShooterPlayerController> attacker_player_controller;
		std::string blueprint;
		FVector location = FVector(0.0f, 0.0f, 0.0f);
		int attacker_tribe_id = 0;
		int victim_tribe_id = 0;
		int score_value = 0;
		bool valid = false;
	};

	explicit TribeScoreService(std::filesystem::path config_path);

	void LoadInitialState();
	FString ReloadConfig();
	StructureDestructionContext CaptureDestructionContext(
		APrimalStructure* structure,
		AController* killer,
		AActor* damage_causer) const;
	void ProcessDestroyedStructure(const StructureDestructionContext& context, bool destruction_succeeded);
	void ShowLeaderboard(AShooterPlayerController* player_controller) const;
	void Shutdown();

private:
	struct AttackContext
	{
		AShooterPlayerController* player_controller = nullptr;
		int tribe_id = 0;
	};

	struct ScoreEvent
	{
		int attacker_tribe_id = 0;
		int victim_tribe_id = 0;
		int amount = 0;
		int attacker_score = 0;
		int victim_score = 0;
		std::string attacker_tribe_name;
		std::string victim_tribe_name;
	};

	FString ReloadConfigInternal(bool startup);
	void ClampScoresToFloor(int score_floor);
	std::shared_ptr<const ScoreConfig> GetConfigSnapshot() const;

	std::string GetStructureBlueprint(APrimalStructure* structure) const;
	AttackContext ResolveAttackContext(APrimalStructure* victim, AController* killer, AActor* damage_causer) const;
	std::string ResolveTribeName(int tribe_id, const std::string& fallback = {}) const;
	bool TryBuildTribeMemberRecords(int tribe_id, const std::string& updated_at, std::vector<TribeMemberRecord>* members) const;
	std::string GetCurrentMapName() const;
	float GetVictimNotificationRange() const;
	std::vector<AShooterPlayerController*> GetNearbyOnlineTribeMembers(int tribe_id, const FVector& location) const;
	void ShowFloatingText(
		AShooterPlayerController* player_controller,
		const FVector& location,
		const std::string& message,
		const FColor& color,
		const FloatingTextSettings& settings) const;
	void ShowPopup(AShooterPlayerController* player_controller, const FString& message, const FLinearColor& color) const;
	FString BuildLeaderboardNotificationText() const;
	std::string FormatMessage(const std::string& format, const ScoreEvent& event, bool is_attacker) const;

	ConfigService config_service_;
	ScoreStore score_store_;

	mutable std::mutex mutex_;
	std::shared_ptr<const ScoreConfig> active_config_;
	mutable std::unordered_map<int, TribeScoreEntry> scores_;
};
} // namespace ExoTribeScore
