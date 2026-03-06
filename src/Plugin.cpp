#include "Plugin.h"

#include <stdexcept>
#include <string>

#include <Logger/Logger.h>

namespace
{
constexpr auto kPluginName = "ExoTribeScore";
constexpr auto kLeaderboardCommand = "/leaderboard";
constexpr auto kReloadCommandName = "ExoTribeScore.Reload";

DECLARE_HOOK(APrimalStructure_Die, bool, APrimalStructure*, float, FDamageEvent*, AController*, AActor*);

template <typename T>
void EnsureHook(const std::string& hook_name, LPVOID detour, T** original)
{
	if (!ArkApi::GetHooks().SetHook(hook_name, detour, original))
	{
		throw std::runtime_error("Failed to register hook '" + hook_name + "'");
	}
}

bool __fastcall Hook_APrimalStructure_Die(
	APrimalStructure* structure,
	float killing_damage,
	FDamageEvent* damage_event,
	AController* killer,
	AActor* damage_causer)
{
	const auto destruction_context =
		ExoTribeScore::ExoTribeScorePlugin::Get().CaptureStructureDestruction(structure, killer, damage_causer);
	const bool destroyed =
		APrimalStructure_Die_original(structure, killing_damage, damage_event, killer, damage_causer);
	ExoTribeScore::ExoTribeScorePlugin::Get().OnStructureDestroyed(destruction_context, destroyed);
	return destroyed;
}
} // namespace

namespace ExoTribeScore
{
ExoTribeScorePlugin& ExoTribeScorePlugin::Get()
{
	static ExoTribeScorePlugin instance;
	return instance;
}

ExoTribeScorePlugin::ExoTribeScorePlugin()
	: tribe_score_service_(
		  std::filesystem::path("ArkApi") / "Plugins" / kPluginName / "config.json",
		  std::filesystem::path("ArkApi") / "Plugins" / kPluginName / "scores.json")
{
}

void ExoTribeScorePlugin::Load()
{
	if (loaded_)
	{
		return;
	}

	RegisterCommands();

	try
	{
		RegisterHooks();
		tribe_score_service_.LoadInitialState();
		loaded_ = true;

		Log::GetLog()->info("{} plugin loaded", kPluginName);
	}
	catch (const std::exception&)
	{
		UnregisterHooks();
		UnregisterCommands();
		throw;
	}
}

void ExoTribeScorePlugin::Unload()
{
	if (!loaded_)
	{
		return;
	}

	tribe_score_service_.Shutdown();
	UnregisterHooks();
	UnregisterCommands();
	loaded_ = false;

	Log::GetLog()->info("{} plugin unloaded", kPluginName);
}

TribeScoreService::StructureDestructionContext ExoTribeScorePlugin::CaptureStructureDestruction(
	APrimalStructure* structure,
	AController* killer,
	AActor* damage_causer) const
{
	return tribe_score_service_.CaptureDestructionContext(structure, killer, damage_causer);
}

void ExoTribeScorePlugin::OnStructureDestroyed(
	const TribeScoreService::StructureDestructionContext& context,
	const bool destruction_succeeded)
{
	tribe_score_service_.ProcessDestroyedStructure(context, destruction_succeeded);
}

void ExoTribeScorePlugin::HandleLeaderboardChatCommand(
	AShooterPlayerController* player_controller,
	FString* message,
	const EChatSendMode::Type mode)
{
	static_cast<void>(message);
	static_cast<void>(mode);

	tribe_score_service_.ShowLeaderboard(player_controller);
}

void ExoTribeScorePlugin::HandleReloadConsoleCommand(APlayerController* player_controller, FString* command)
{
	static_cast<void>(command);

	const FString reply = tribe_score_service_.ReloadConfig();
	ReplyToConsole(player_controller, reply);
}

void ExoTribeScorePlugin::HandleReloadRconCommand(RCONClientConnection* rcon_connection, RCONPacket* packet)
{
	const FString reply = tribe_score_service_.ReloadConfig();
	ReplyToRcon(rcon_connection, packet, reply);
}

void ExoTribeScorePlugin::RegisterCommands()
{
	ArkApi::GetCommands().AddChatCommand(kLeaderboardCommand, [](AShooterPlayerController* player_controller, FString* message, EChatSendMode::Type mode)
	{
		ExoTribeScorePlugin::Get().HandleLeaderboardChatCommand(player_controller, message, mode);
	});

	ArkApi::GetCommands().AddConsoleCommand(kReloadCommandName, [](APlayerController* player_controller, FString* command, bool)
	{
		ExoTribeScorePlugin::Get().HandleReloadConsoleCommand(player_controller, command);
	});

	ArkApi::GetCommands().AddRconCommand(kReloadCommandName, [](RCONClientConnection* rcon_connection, RCONPacket* packet, UWorld*)
	{
		ExoTribeScorePlugin::Get().HandleReloadRconCommand(rcon_connection, packet);
	});
}

void ExoTribeScorePlugin::UnregisterCommands()
{
	ArkApi::GetCommands().RemoveChatCommand(kLeaderboardCommand);
	ArkApi::GetCommands().RemoveConsoleCommand(kReloadCommandName);
	ArkApi::GetCommands().RemoveRconCommand(kReloadCommandName);
}

void ExoTribeScorePlugin::RegisterHooks()
{
	EnsureHook("APrimalStructure.Die", &Hook_APrimalStructure_Die, &APrimalStructure_Die_original);
}

void ExoTribeScorePlugin::UnregisterHooks()
{
	ArkApi::GetHooks().DisableHook("APrimalStructure.Die", &Hook_APrimalStructure_Die);
}

void ExoTribeScorePlugin::ReplyToConsole(APlayerController* player_controller, const FString& reply) const
{
	auto* shooter_controller = static_cast<AShooterPlayerController*>(player_controller);
	if (shooter_controller != nullptr)
	{
		FString message(reply);
		shooter_controller->ClientServerChatDirectMessage(&message, FColorList::Green, false);
	}

	Log::GetLog()->info("{}", reply.ToString());
}

void ExoTribeScorePlugin::ReplyToRcon(
	RCONClientConnection* rcon_connection,
	RCONPacket* packet,
	const FString& reply) const
{
	if (rcon_connection != nullptr && packet != nullptr)
	{
		FString message(reply);
		rcon_connection->SendMessageW(packet->Id, 0, &message);
	}

	Log::GetLog()->info("{}", reply.ToString());
}
} // namespace ExoTribeScore

extern "C" __declspec(dllexport) void Plugin_Init()
{
	Log::Get().Init(kPluginName);

	try
	{
		ExoTribeScore::ExoTribeScorePlugin::Get().Load();
	}
	catch (const std::exception& exception)
	{
		Log::GetLog()->error("Failed to initialize {}: {}", kPluginName, exception.what());
	}
}

extern "C" __declspec(dllexport) void Plugin_Unload()
{
	try
	{
		ExoTribeScore::ExoTribeScorePlugin::Get().Unload();
	}
	catch (const std::exception& exception)
	{
		Log::GetLog()->error("Failed to unload {} cleanly: {}", kPluginName, exception.what());
	}
}
