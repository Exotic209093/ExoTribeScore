#pragma once

#include "TribeScoreService.h"

namespace ExoTribeScore
{
class ExoTribeScorePlugin
{
public:
	static ExoTribeScorePlugin& Get();

	void Load();
	void Unload();
	TribeScoreService::StructureDestructionContext CaptureStructureDestruction(
		APrimalStructure* structure,
		AController* killer,
		AActor* damage_causer) const;
	void OnStructureDestroyed(const TribeScoreService::StructureDestructionContext& context, bool destruction_succeeded);
	void HandleLeaderboardChatCommand(AShooterPlayerController* player_controller, FString* message, EChatSendMode::Type mode);
	void HandleReloadConsoleCommand(APlayerController* player_controller, FString* command);
	void HandleReloadRconCommand(RCONClientConnection* rcon_connection, RCONPacket* packet);

private:
	ExoTribeScorePlugin();

	void RegisterCommands();
	void UnregisterCommands();
	void RegisterHooks();
	void UnregisterHooks();
	void ReplyToConsole(APlayerController* player_controller, const FString& reply) const;
	void ReplyToRcon(RCONClientConnection* rcon_connection, RCONPacket* packet, const FString& reply) const;

	TribeScoreService tribe_score_service_;
	bool loaded_ = false;
};
} // namespace ExoTribeScore
