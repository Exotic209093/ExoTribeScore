#include "Config.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <unordered_set>

#include <json.hpp>

namespace
{
using json = nlohmann::json;

std::string Trim(const std::string& value)
{
	const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char character)
	{
		return std::isspace(character) != 0;
	});

	if (first == value.end())
	{
		return {};
	}

	const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character)
	{
		return std::isspace(character) != 0;
	}).base();

	return std::string(first, last);
}

std::string ToLowerAscii(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
	{
		return static_cast<char>(std::tolower(character));
	});

	return value;
}

bool StartsWithInsensitive(const std::string& value, const std::string& prefix)
{
	if (value.size() < prefix.size())
	{
		return false;
	}

	return ToLowerAscii(value.substr(0, prefix.size())) == ToLowerAscii(prefix);
}

bool EndsWithInsensitive(const std::string& value, const std::string& suffix)
{
	if (value.size() < suffix.size())
	{
		return false;
	}

	return ToLowerAscii(value.substr(value.size() - suffix.size())) == ToLowerAscii(suffix);
}

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

bool TryReadPositiveFloat(
	const json& object,
	const char* field_name,
	float* out_value,
	std::vector<std::string>& errors,
	const std::string& context)
{
	if (!object.contains(field_name))
	{
		AddError(errors, context + " is missing required field '" + field_name + "'");
		return false;
	}

	const auto& field = object[field_name];
	if (!field.is_number())
	{
		AddError(errors, context + " field '" + field_name + "' must be a number");
		return false;
	}

	const double numeric_value = field.get<double>();
	if (!std::isfinite(numeric_value) || numeric_value <= 0.0
		|| numeric_value > static_cast<double>((std::numeric_limits<float>::max)()))
	{
		AddError(errors, context + " field '" + field_name + "' must be a finite number greater than 0");
		return false;
	}

	*out_value = static_cast<float>(numeric_value);
	return true;
}

bool TryReadNonNegativeFloat(
	const json& object,
	const char* field_name,
	float* out_value,
	std::vector<std::string>& errors,
	const std::string& context)
{
	if (!object.contains(field_name))
	{
		AddError(errors, context + " is missing required field '" + field_name + "'");
		return false;
	}

	const auto& field = object[field_name];
	if (!field.is_number())
	{
		AddError(errors, context + " field '" + field_name + "' must be a number");
		return false;
	}

	const double numeric_value = field.get<double>();
	if (!std::isfinite(numeric_value) || numeric_value < 0.0
		|| numeric_value > static_cast<double>((std::numeric_limits<float>::max)()))
	{
		AddError(errors, context + " field '" + field_name + "' must be a finite number greater than or equal to 0");
		return false;
	}

	*out_value = static_cast<float>(numeric_value);
	return true;
}

bool TryReadColor(
	const json& value,
	FColor* out_color,
	std::vector<std::string>& errors,
	const std::string& context)
{
	if (!value.is_object())
	{
		AddError(errors, context + " must be a JSON object");
		return false;
	}

	ValidateObjectKeys(value, { "r", "g", "b", "a" }, errors, context);

	int channels[4] = { 0, 0, 0, 255 };
	const char* field_names[4] = { "r", "g", "b", "a" };

	for (int index = 0; index < 4; ++index)
	{
		const char* field_name = field_names[index];
		if (!value.contains(field_name))
		{
			AddError(errors, context + " is missing required field '" + field_name + "'");
			return false;
		}

		const auto& field = value[field_name];
		if (!field.is_number_integer())
		{
			AddError(errors, context + " field '" + field_name + "' must be an integer");
			return false;
		}

		const int channel_value = field.get<int>();
		if (channel_value < 0 || channel_value > 255)
		{
			AddError(errors, context + " field '" + field_name + "' must be between 0 and 255");
			return false;
		}

		channels[index] = channel_value;
	}

	*out_color = FColor(
		static_cast<unsigned char>(channels[0]),
		static_cast<unsigned char>(channels[1]),
		static_cast<unsigned char>(channels[2]),
		static_cast<unsigned char>(channels[3]));
	return true;
}

bool TryReadVelocity(
	const json& value,
	FVector* out_velocity,
	std::vector<std::string>& errors,
	const std::string& context)
{
	if (!value.is_object())
	{
		AddError(errors, context + " must be a JSON object");
		return false;
	}

	ValidateObjectKeys(value, { "x", "y", "z" }, errors, context);

	float coordinates[3] = { 0.0f, 0.0f, 0.0f };
	const char* field_names[3] = { "x", "y", "z" };

	for (int index = 0; index < 3; ++index)
	{
		const char* field_name = field_names[index];
		if (!value.contains(field_name))
		{
			AddError(errors, context + " is missing required field '" + field_name + "'");
			return false;
		}

		const auto& field = value[field_name];
		if (!field.is_number())
		{
			AddError(errors, context + " field '" + field_name + "' must be a number");
			return false;
		}

		const double coordinate = field.get<double>();
		if (!std::isfinite(coordinate) || std::abs(coordinate) > static_cast<double>((std::numeric_limits<float>::max)()))
		{
			AddError(errors, context + " field '" + field_name + "' must be a finite number");
			return false;
		}

		coordinates[index] = static_cast<float>(coordinate);
	}

	*out_velocity = FVector(coordinates[0], coordinates[1], coordinates[2]);
	return true;
}
} // namespace

namespace ExoTribeScore
{
ConfigService::ConfigService(std::filesystem::path config_path)
	: config_path_(std::move(config_path))
{
}

ConfigLoadResult ConfigService::Load() const
{
	ConfigLoadResult result;

	std::ifstream file(config_path_);
	if (!file.is_open())
	{
		AddError(result.errors, "Config file not found: " + config_path_.string());
		return result;
	}

	json root;
	try
	{
		file >> root;
	}
	catch (const std::exception& exception)
	{
		AddError(result.errors, "Failed to parse JSON: " + std::string(exception.what()));
		return result;
	}

	if (!root.is_object())
	{
		AddError(result.errors, "Root config value must be a JSON object");
		return result;
	}

	ValidateObjectKeys(
		root,
		{ "version", "score_floor", "leaderboard_limit", "mysql", "floating_text", "messages", "structures" },
		result.errors,
		"Root config");

	auto config = std::make_shared<ScoreConfig>();

	if (!root.contains("version"))
	{
		AddError(result.errors, "Root config is missing required field 'version'");
	}
	else if (!root["version"].is_number_integer())
	{
		AddError(result.errors, "Field 'version' must be an integer");
	}
	else if (root["version"].get<int>() != 1)
	{
		AddError(result.errors, "Field 'version' must be set to 1 for this plugin version");
	}
	else
	{
		config->version = 1;
	}

	if (!root.contains("score_floor"))
	{
		AddError(result.errors, "Root config is missing required field 'score_floor'");
	}
	else if (!root["score_floor"].is_number_integer())
	{
		AddError(result.errors, "Field 'score_floor' must be an integer");
	}
	else
	{
		config->score_floor = root["score_floor"].get<int>();
	}

	if (!root.contains("leaderboard_limit"))
	{
		AddError(result.errors, "Root config is missing required field 'leaderboard_limit'");
	}
	else if (!root["leaderboard_limit"].is_number_integer())
	{
		AddError(result.errors, "Field 'leaderboard_limit' must be an integer");
	}
	else if (root["leaderboard_limit"].get<int>() <= 0)
	{
		AddError(result.errors, "Field 'leaderboard_limit' must be greater than 0");
	}
	else
	{
		config->leaderboard_limit = root["leaderboard_limit"].get<int>();
	}

	if (!root.contains("mysql"))
	{
		AddError(result.errors, "Root config is missing required field 'mysql'");
	}
	else if (!root["mysql"].is_object())
	{
		AddError(result.errors, "Field 'mysql' must be a JSON object");
	}
	else
	{
		const auto& mysql = root["mysql"];
		ValidateObjectKeys(
			mysql,
			{
				"driver",
				"host",
				"port",
				"database",
				"username",
				"password",
				"table_prefix",
				"connection_timeout_seconds"
			},
			result.errors,
			"Field 'mysql'");

		auto read_non_empty_string = [&result, &mysql](const char* field_name, std::string* target)
		{
			if (!mysql.contains(field_name))
			{
				AddError(result.errors, "Field 'mysql' is missing required field '" + std::string(field_name) + "'");
				return;
			}

			if (!mysql[field_name].is_string() || Trim(mysql[field_name].get<std::string>()).empty())
			{
				AddError(result.errors, "Field 'mysql." + std::string(field_name) + "' must be a non-empty string");
				return;
			}

			*target = Trim(mysql[field_name].get<std::string>());
		};

		read_non_empty_string("driver", &config->mysql.driver);
		read_non_empty_string("host", &config->mysql.host);
		read_non_empty_string("database", &config->mysql.database);
		read_non_empty_string("username", &config->mysql.username);

		if (!mysql.contains("password"))
		{
			AddError(result.errors, "Field 'mysql' is missing required field 'password'");
		}
		else if (!mysql["password"].is_string())
		{
			AddError(result.errors, "Field 'mysql.password' must be a string");
		}
		else
		{
			config->mysql.password = mysql["password"].get<std::string>();
		}

		if (!mysql.contains("port"))
		{
			AddError(result.errors, "Field 'mysql' is missing required field 'port'");
		}
		else if (!mysql["port"].is_number_integer())
		{
			AddError(result.errors, "Field 'mysql.port' must be an integer");
		}
		else if (mysql["port"].get<int>() <= 0 || mysql["port"].get<int>() > 65535)
		{
			AddError(result.errors, "Field 'mysql.port' must be between 1 and 65535");
		}
		else
		{
			config->mysql.port = mysql["port"].get<int>();
		}

		if (!mysql.contains("connection_timeout_seconds"))
		{
			AddError(result.errors, "Field 'mysql' is missing required field 'connection_timeout_seconds'");
		}
		else if (!mysql["connection_timeout_seconds"].is_number_integer())
		{
			AddError(result.errors, "Field 'mysql.connection_timeout_seconds' must be an integer");
		}
		else if (mysql["connection_timeout_seconds"].get<int>() <= 0)
		{
			AddError(result.errors, "Field 'mysql.connection_timeout_seconds' must be greater than 0");
		}
		else
		{
			config->mysql.connection_timeout_seconds = mysql["connection_timeout_seconds"].get<int>();
		}

		if (!mysql.contains("table_prefix"))
		{
			AddError(result.errors, "Field 'mysql' is missing required field 'table_prefix'");
		}
		else if (!mysql["table_prefix"].is_string())
		{
			AddError(result.errors, "Field 'mysql.table_prefix' must be a string");
		}
		else
		{
			config->mysql.table_prefix = Trim(mysql["table_prefix"].get<std::string>());
			if (config->mysql.table_prefix.empty())
			{
				AddError(result.errors, "Field 'mysql.table_prefix' must not be empty");
			}
			else if (!std::all_of(
						 config->mysql.table_prefix.begin(),
						 config->mysql.table_prefix.end(),
						 [](unsigned char character)
						 {
							 return std::isalnum(character) != 0 || character == '_';
						 }))
			{
				AddError(result.errors, "Field 'mysql.table_prefix' may contain only letters, numbers, and underscores");
			}
		}
	}

	if (!root.contains("floating_text"))
	{
		AddError(result.errors, "Root config is missing required field 'floating_text'");
	}
	else if (!root["floating_text"].is_object())
	{
		AddError(result.errors, "Field 'floating_text' must be a JSON object");
	}
	else
	{
		const auto& floating_text = root["floating_text"];
		ValidateObjectKeys(
			floating_text,
			{
				"positive_color",
				"negative_color",
				"scale_x",
				"scale_y",
				"lifespan",
				"min_scale",
				"fade_in_time",
				"fade_out_time",
				"velocity"
			},
			result.errors,
			"Field 'floating_text'");

		TryReadColor(
			floating_text.value("positive_color", json::object()),
			&config->floating_text.positive_color,
			result.errors,
			"Field 'floating_text.positive_color'");
		TryReadColor(
			floating_text.value("negative_color", json::object()),
			&config->floating_text.negative_color,
			result.errors,
			"Field 'floating_text.negative_color'");
		TryReadPositiveFloat(floating_text, "scale_x", &config->floating_text.scale_x, result.errors, "Field 'floating_text'");
		TryReadPositiveFloat(floating_text, "scale_y", &config->floating_text.scale_y, result.errors, "Field 'floating_text'");
		TryReadPositiveFloat(floating_text, "lifespan", &config->floating_text.lifespan, result.errors, "Field 'floating_text'");
		TryReadPositiveFloat(floating_text, "min_scale", &config->floating_text.min_scale, result.errors, "Field 'floating_text'");
		TryReadNonNegativeFloat(
			floating_text,
			"fade_in_time",
			&config->floating_text.fade_in_time,
			result.errors,
			"Field 'floating_text'");
		TryReadNonNegativeFloat(
			floating_text,
			"fade_out_time",
			&config->floating_text.fade_out_time,
			result.errors,
			"Field 'floating_text'");
		TryReadVelocity(
			floating_text.value("velocity", json::object()),
			&config->floating_text.velocity,
			result.errors,
			"Field 'floating_text.velocity'");
	}

	if (!root.contains("messages"))
	{
		AddError(result.errors, "Root config is missing required field 'messages'");
	}
	else if (!root["messages"].is_object())
	{
		AddError(result.errors, "Field 'messages' must be a JSON object");
	}
	else
	{
		const auto& messages = root["messages"];
		ValidateObjectKeys(messages, { "attacker", "victim" }, result.errors, "Field 'messages'");

		if (!messages.contains("attacker"))
		{
			AddError(result.errors, "Field 'messages' is missing required field 'attacker'");
		}
		else if (!messages["attacker"].is_string() || Trim(messages["attacker"].get<std::string>()).empty())
		{
			AddError(result.errors, "Field 'messages.attacker' must be a non-empty string");
		}
		else
		{
			config->messages.attacker = messages["attacker"].get<std::string>();
		}

		if (!messages.contains("victim"))
		{
			AddError(result.errors, "Field 'messages' is missing required field 'victim'");
		}
		else if (!messages["victim"].is_string() || Trim(messages["victim"].get<std::string>()).empty())
		{
			AddError(result.errors, "Field 'messages.victim' must be a non-empty string");
		}
		else
		{
			config->messages.victim = messages["victim"].get<std::string>();
		}
	}

	if (!root.contains("structures"))
	{
		AddError(result.errors, "Root config is missing required field 'structures'");
	}
	else if (!root["structures"].is_array())
	{
		AddError(result.errors, "Field 'structures' must be an array");
	}
	else
	{
		std::unordered_set<std::string> seen_blueprints;

		for (size_t index = 0; index < root["structures"].size(); ++index)
		{
			const auto& entry = root["structures"][index];
			const std::string context = "Structure entry #" + std::to_string(index);

			if (!entry.is_object())
			{
				AddError(result.errors, context + " must be a JSON object");
				continue;
			}

			ValidateObjectKeys(entry, { "blueprint", "score_value" }, result.errors, context);

			if (!entry.contains("blueprint"))
			{
				AddError(result.errors, context + " is missing required field 'blueprint'");
				continue;
			}

			if (!entry["blueprint"].is_string())
			{
				AddError(result.errors, context + " field 'blueprint' must be a string");
				continue;
			}

			std::string normalization_error;
			std::string normalized_blueprint =
				NormalizeBlueprintPath(entry["blueprint"].get<std::string>(), &normalization_error);

			if (!normalization_error.empty())
			{
				AddError(result.errors, context + " field 'blueprint' " + normalization_error);
				continue;
			}

			if (!seen_blueprints.insert(normalized_blueprint).second)
			{
				AddError(result.errors, context + " duplicates blueprint path '" + normalized_blueprint + "'");
				continue;
			}

			if (!entry.contains("score_value"))
			{
				AddError(result.errors, context + " is missing required field 'score_value'");
				continue;
			}

			if (!entry["score_value"].is_number_integer())
			{
				AddError(result.errors, context + " field 'score_value' must be an integer");
				continue;
			}

			const int score_value = entry["score_value"].get<int>();
			if (score_value <= 0)
			{
				AddError(result.errors, context + " field 'score_value' must be greater than 0");
				continue;
			}

			StructureScoreRule rule;
			rule.blueprint = normalized_blueprint;
			rule.score_value = score_value;
			config->structures.emplace(rule.blueprint, rule);
		}
	}

	if (!result.errors.empty())
	{
		return result;
	}

	result.success = true;
	result.config = config;
	return result;
}

const std::filesystem::path& ConfigService::GetPath() const
{
	return config_path_;
}

std::string ConfigService::NormalizeBlueprintPath(const std::string& value, std::string* error_message)
{
	auto fail = [error_message](const std::string& message) -> std::string
	{
		if (error_message != nullptr)
		{
			*error_message = message;
		}

		return {};
	};

	std::string normalized = Trim(value);
	if (normalized.empty())
	{
		return fail("cannot be empty");
	}

	std::replace(normalized.begin(), normalized.end(), '\\', '/');

	for (const std::string prefix : { "BlueprintGeneratedClass'", "Blueprint'", "Class'" })
	{
		if (StartsWithInsensitive(normalized, prefix))
		{
			normalized = normalized.substr(prefix.size());
			break;
		}
	}

	if (!normalized.empty() && normalized.front() == '\'')
	{
		normalized.erase(normalized.begin());
	}

	if (!normalized.empty() && normalized.back() == '\'')
	{
		normalized.pop_back();
	}

	const auto dot_position = normalized.rfind('.');
	if (dot_position == std::string::npos || dot_position == 0 || dot_position == normalized.size() - 1)
	{
		return fail("must use a full asset or class path like /Game/Path/Object.Object_C");
	}

	std::string package_name = normalized.substr(0, dot_position);
	std::string object_name = normalized.substr(dot_position + 1);

	if (package_name.empty() || object_name.empty())
	{
		return fail("must use a full asset or class path like /Game/Path/Object.Object_C");
	}

	if (!EndsWithInsensitive(object_name, "_c"))
	{
		object_name += "_C";
	}

	normalized = package_name + "." + object_name;
	normalized = ToLowerAscii(normalized);

	if (error_message != nullptr)
	{
		error_message->clear();
	}

	return normalized;
}
} // namespace ExoTribeScore
