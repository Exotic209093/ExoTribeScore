#include "ScoreStore.h"

#include <array>
#include <cstdint>
#include <sstream>
#include <utility>

#include <Windows.h>
#include <sql.h>
#include <sqlext.h>

namespace
{
void AddError(std::vector<std::string>& errors, const std::string& message)
{
	errors.push_back(message);
}

std::string EscapeSqlString(const std::string& value)
{
	std::string escaped;
	escaped.reserve(value.size() + 8);

	for (const char character : value)
	{
		if (character == '\'')
		{
			escaped += "''";
		}
		else
		{
			escaped += character;
		}
	}

	return escaped;
}

std::string EscapeConnectionStringValue(const std::string& value)
{
	std::string escaped;
	escaped.reserve(value.size() + 2);
	escaped += '{';

	for (const char character : value)
	{
		escaped += character;
		if (character == '}')
		{
			escaped += '}';
		}
	}

	escaped += '}';
	return escaped;
}

void AppendOdbcDiagnostics(
	const SQLSMALLINT handle_type,
	const SQLHANDLE handle,
	const std::string& context,
	std::vector<std::string>* errors)
{
	if (errors == nullptr)
	{
		return;
	}

	bool any = false;
	for (SQLSMALLINT record_index = 1;; ++record_index)
	{
		std::array<char, SQL_MAX_MESSAGE_LENGTH> message_buffer{};
		std::array<char, 6> sql_state{};
		SQLINTEGER native_error = 0;
		SQLSMALLINT message_length = 0;
		const SQLRETURN rc = SQLGetDiagRecA(
			handle_type,
			handle,
			record_index,
			reinterpret_cast<SQLCHAR*>(sql_state.data()),
			&native_error,
			reinterpret_cast<SQLCHAR*>(message_buffer.data()),
			static_cast<SQLSMALLINT>(message_buffer.size()),
			&message_length);

		if (rc == SQL_NO_DATA)
		{
			break;
		}

		if (!SQL_SUCCEEDED(rc))
		{
			break;
		}

		any = true;
		std::ostringstream stream;
		stream << context << " [" << sql_state.data() << "] " << message_buffer.data();
		AddError(*errors, stream.str());
	}

	if (!any)
	{
		AddError(*errors, context);
	}
}

class OdbcConnection
{
public:
	~OdbcConnection()
	{
		Close();
	}

	bool Open(const ExoTribeScore::MySqlConnectionSettings& settings, std::vector<std::string>* errors)
	{
		Close();

		if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &environment_)))
		{
			AddErrorIfPresent(errors, "Failed to allocate ODBC environment handle");
			return false;
		}

		if (!SQL_SUCCEEDED(SQLSetEnvAttr(environment_, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0)))
		{
			AppendOdbcDiagnostics(SQL_HANDLE_ENV, environment_, "Failed to configure ODBC environment", errors);
			return false;
		}

		if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_DBC, environment_, &connection_)))
		{
			AppendOdbcDiagnostics(SQL_HANDLE_ENV, environment_, "Failed to allocate ODBC connection handle", errors);
			return false;
		}

		SQLSetConnectAttr(
			connection_,
			SQL_LOGIN_TIMEOUT,
			reinterpret_cast<SQLPOINTER>(static_cast<intptr_t>(settings.connection_timeout_seconds)),
			0);
		SQLSetConnectAttr(
			connection_,
			SQL_ATTR_CONNECTION_TIMEOUT,
			reinterpret_cast<SQLPOINTER>(static_cast<intptr_t>(settings.connection_timeout_seconds)),
			0);

		std::ostringstream connection_stream;
		connection_stream
			<< "DRIVER=" << EscapeConnectionStringValue(settings.driver) << ';'
			<< "SERVER=" << EscapeConnectionStringValue(settings.host) << ';'
			<< "PORT=" << settings.port << ';'
			<< "DATABASE=" << EscapeConnectionStringValue(settings.database) << ';'
			<< "UID=" << EscapeConnectionStringValue(settings.username) << ';'
			<< "PWD=" << EscapeConnectionStringValue(settings.password) << ';'
			<< "CHARSET=utf8mb4;";

		const std::string connection_string = connection_stream.str();
		std::array<char, 1024> output_buffer{};
		SQLSMALLINT output_length = 0;
		const SQLRETURN rc = SQLDriverConnectA(
			connection_,
			nullptr,
			reinterpret_cast<SQLCHAR*>(const_cast<char*>(connection_string.c_str())),
			SQL_NTS,
			reinterpret_cast<SQLCHAR*>(output_buffer.data()),
			static_cast<SQLSMALLINT>(output_buffer.size()),
			&output_length,
			SQL_DRIVER_NOPROMPT);

		if (!SQL_SUCCEEDED(rc))
		{
			AppendOdbcDiagnostics(SQL_HANDLE_DBC, connection_, "Failed to connect to MySQL through ODBC", errors);
			return false;
		}

		return true;
	}

	bool Execute(const std::string& sql, std::vector<std::string>* errors, const std::string& context) const
	{
		SQLHSTMT statement = SQL_NULL_HSTMT;
		if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, connection_, &statement)))
		{
			AppendOdbcDiagnostics(SQL_HANDLE_DBC, connection_, context + ": statement allocation failed", errors);
			return false;
		}

		const SQLRETURN rc = SQLExecDirectA(statement, reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql.c_str())), SQL_NTS);
		if (!SQL_SUCCEEDED(rc))
		{
			AppendOdbcDiagnostics(SQL_HANDLE_STMT, statement, context, errors);
			SQLFreeHandle(SQL_HANDLE_STMT, statement);
			return false;
		}

		SQLFreeHandle(SQL_HANDLE_STMT, statement);
		return true;
	}

	bool Query(
		const std::string& sql,
		std::vector<std::array<std::string, 4>>* rows,
		std::vector<std::string>* errors,
		const std::string& context) const
	{
		SQLHSTMT statement = SQL_NULL_HSTMT;
		if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, connection_, &statement)))
		{
			AppendOdbcDiagnostics(SQL_HANDLE_DBC, connection_, context + ": statement allocation failed", errors);
			return false;
		}

		const SQLRETURN exec_rc = SQLExecDirectA(statement, reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql.c_str())), SQL_NTS);
		if (!SQL_SUCCEEDED(exec_rc))
		{
			AppendOdbcDiagnostics(SQL_HANDLE_STMT, statement, context, errors);
			SQLFreeHandle(SQL_HANDLE_STMT, statement);
			return false;
		}

		while (true)
		{
			const SQLRETURN fetch_rc = SQLFetch(statement);
			if (fetch_rc == SQL_NO_DATA)
			{
				break;
			}

			if (!SQL_SUCCEEDED(fetch_rc))
			{
				AppendOdbcDiagnostics(SQL_HANDLE_STMT, statement, context + ": row fetch failed", errors);
				SQLFreeHandle(SQL_HANDLE_STMT, statement);
				return false;
			}

			std::array<std::string, 4> row;
			for (SQLUSMALLINT column = 0; column < 4; ++column)
			{
				row[column] = ReadTextColumn(statement, static_cast<SQLUSMALLINT>(column + 1));
			}

			rows->push_back(std::move(row));
		}

		SQLFreeHandle(SQL_HANDLE_STMT, statement);
		return true;
	}

	void Close()
	{
		if (connection_ != SQL_NULL_HDBC)
		{
			SQLDisconnect(connection_);
			SQLFreeHandle(SQL_HANDLE_DBC, connection_);
			connection_ = SQL_NULL_HDBC;
		}

		if (environment_ != SQL_NULL_HENV)
		{
			SQLFreeHandle(SQL_HANDLE_ENV, environment_);
			environment_ = SQL_NULL_HENV;
		}
	}

private:
	static void AddErrorIfPresent(std::vector<std::string>* errors, const std::string& message)
	{
		if (errors != nullptr)
		{
			AddError(*errors, message);
		}
	}

	static std::string ReadTextColumn(SQLHSTMT statement, const SQLUSMALLINT column_index)
	{
		std::string result;
		std::array<char, 1024> buffer{};

		while (true)
		{
			SQLLEN indicator = 0;
			const SQLRETURN rc = SQLGetData(
				statement,
				column_index,
				SQL_C_CHAR,
				buffer.data(),
				static_cast<SQLLEN>(buffer.size()),
				&indicator);

			if (rc == SQL_NO_DATA)
			{
				break;
			}

			if (!SQL_SUCCEEDED(rc) && rc != SQL_SUCCESS_WITH_INFO)
			{
				break;
			}

			if (indicator == SQL_NULL_DATA)
			{
				return {};
			}

			result += buffer.data();
			if (rc == SQL_SUCCESS)
			{
				break;
			}
		}

		return result;
	}

	SQLHENV environment_ = SQL_NULL_HENV;
	SQLHDBC connection_ = SQL_NULL_HDBC;
};

bool EnsureSchema(
	const OdbcConnection& connection,
	const std::string& tribe_scores_table,
	const std::string& score_events_table,
	const std::string& tribe_members_table,
	std::vector<std::string>* errors)
{
	return connection.Execute(
			"CREATE TABLE IF NOT EXISTS " + tribe_scores_table
				+ " (tribe_id INT NOT NULL PRIMARY KEY, last_known_name VARCHAR(255) NOT NULL DEFAULT '', score INT NOT NULL, updated_at VARCHAR(32) NOT NULL DEFAULT '') ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
			errors,
			"Failed to initialize tribe_scores table")
		&& connection.Execute(
			"CREATE TABLE IF NOT EXISTS " + score_events_table
				+ " (id BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY, attacker_tribe_id INT NOT NULL, victim_tribe_id INT NOT NULL, blueprint TEXT NOT NULL, delta INT NOT NULL, map_name VARCHAR(191) NOT NULL DEFAULT '', created_at VARCHAR(32) NOT NULL, attacker_score INT NOT NULL, victim_score INT NOT NULL, INDEX idx_created_at (created_at)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
			errors,
			"Failed to initialize score_events table")
		&& connection.Execute(
			"CREATE TABLE IF NOT EXISTS " + tribe_members_table
				+ " (tribe_id INT NOT NULL, player_data_id BIGINT UNSIGNED NOT NULL, player_name VARCHAR(255) NOT NULL DEFAULT '', rank_group INT NOT NULL DEFAULT 0, updated_at VARCHAR(32) NOT NULL, PRIMARY KEY (tribe_id, player_data_id), INDEX idx_tribe_id (tribe_id)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
			errors,
			"Failed to initialize tribe_members table");
}

bool UpsertScoresInternal(
	const OdbcConnection& connection,
	const std::string& tribe_scores_table,
	const std::vector<ExoTribeScore::TribeScoreEntry>& scores,
	std::vector<std::string>* errors)
{
	if (scores.empty())
	{
		return true;
	}

	std::ostringstream sql;
	sql << "INSERT INTO " << tribe_scores_table
		<< " (tribe_id, last_known_name, score, updated_at) VALUES ";

	for (size_t index = 0; index < scores.size(); ++index)
	{
		if (index > 0)
		{
			sql << ',';
		}

		const auto& entry = scores[index];
		sql << '('
			<< entry.tribe_id << ", '"
			<< EscapeSqlString(entry.last_known_name) << "', "
			<< entry.score << ", '"
			<< EscapeSqlString(entry.updated_at) << "')";
	}

	sql << " ON DUPLICATE KEY UPDATE "
		<< "last_known_name = VALUES(last_known_name), "
		<< "score = VALUES(score), "
		<< "updated_at = VALUES(updated_at);";

	return connection.Execute(sql.str(), errors, "Failed to upsert tribe scores");
}

bool ReplaceMembersInternal(
	const OdbcConnection& connection,
	const std::string& tribe_members_table,
	const int tribe_id,
	const std::vector<ExoTribeScore::TribeMemberRecord>& members,
	std::vector<std::string>* errors)
{
	std::ostringstream delete_sql;
	delete_sql << "DELETE FROM " << tribe_members_table << " WHERE tribe_id = " << tribe_id << ';';
	if (!connection.Execute(delete_sql.str(), errors, "Failed to clear tribe members"))
	{
		return false;
	}

	if (members.empty())
	{
		return true;
	}

	std::ostringstream insert_sql;
	insert_sql << "INSERT INTO " << tribe_members_table
		<< " (tribe_id, player_data_id, player_name, rank_group, updated_at) VALUES ";

	for (size_t index = 0; index < members.size(); ++index)
	{
		if (index > 0)
		{
			insert_sql << ',';
		}

		const auto& member = members[index];
		insert_sql << '('
			<< member.tribe_id << ", "
			<< member.player_data_id << ", '"
			<< EscapeSqlString(member.player_name) << "', "
			<< member.rank_group << ", '"
			<< EscapeSqlString(member.updated_at) << "')";
	}

	insert_sql << ';';
	return connection.Execute(insert_sql.str(), errors, "Failed to insert tribe members");
}
} // namespace

namespace ExoTribeScore
{
void ScoreStore::Configure(const MySqlConnectionSettings& connection_settings)
{
	std::lock_guard<std::mutex> lock(mutex_);
	connection_settings_ = connection_settings;
}

bool ScoreStore::TestConnection(std::vector<std::string>* errors) const
{
	const MySqlConnectionSettings settings = GetConnectionSettings();
	OdbcConnection connection;
	if (!connection.Open(settings, errors))
	{
		return false;
	}

	return EnsureSchema(
		connection,
		GetQualifiedTableName("tribe_scores"),
		GetQualifiedTableName("score_events"),
		GetQualifiedTableName("tribe_members"),
		errors);
}

ScoreLoadResult ScoreStore::Load() const
{
	ScoreLoadResult result;
	result.success = true;

	const MySqlConnectionSettings settings = GetConnectionSettings();
	OdbcConnection connection;
	if (!connection.Open(settings, &result.errors))
	{
		result.success = false;
		return result;
	}

	if (!EnsureSchema(
			connection,
			GetQualifiedTableName("tribe_scores"),
			GetQualifiedTableName("score_events"),
			GetQualifiedTableName("tribe_members"),
			&result.errors))
	{
		result.success = false;
		return result;
	}

	std::vector<std::array<std::string, 4>> rows;
	if (!connection.Query(
			"SELECT tribe_id, last_known_name, score, updated_at FROM " + GetQualifiedTableName("tribe_scores") + " ORDER BY tribe_id ASC;",
			&rows,
			&result.errors,
			"Failed to load tribe scores from MySQL"))
	{
		result.success = false;
		return result;
	}

	try
	{
		for (const auto& row : rows)
		{
			TribeScoreEntry entry;
			entry.tribe_id = std::stoi(row[0]);
			entry.last_known_name = row[1];
			entry.score = std::stoi(row[2]);
			entry.updated_at = row[3];
			result.scores.emplace(entry.tribe_id, std::move(entry));
		}
	}
	catch (const std::exception& exception)
	{
		result.success = false;
		result.scores.clear();
		AddError(result.errors, "Failed to parse tribe score rows from MySQL: " + std::string(exception.what()));
	}

	return result;
}

bool ScoreStore::UpsertScores(const std::vector<TribeScoreEntry>& scores, std::vector<std::string>* errors) const
{
	const MySqlConnectionSettings settings = GetConnectionSettings();
	OdbcConnection connection;
	if (!connection.Open(settings, errors))
	{
		return false;
	}

	if (!EnsureSchema(
			connection,
			GetQualifiedTableName("tribe_scores"),
			GetQualifiedTableName("score_events"),
			GetQualifiedTableName("tribe_members"),
			errors))
	{
		return false;
	}

	return UpsertScoresInternal(connection, GetQualifiedTableName("tribe_scores"), scores, errors);
}

std::vector<TribeScoreEntry> ScoreStore::LoadTopScores(const int limit, std::vector<std::string>* errors) const
{
	std::vector<TribeScoreEntry> entries;
	if (limit <= 0)
	{
		return entries;
	}

	const MySqlConnectionSettings settings = GetConnectionSettings();
	OdbcConnection connection;
	if (!connection.Open(settings, errors))
	{
		return entries;
	}

	if (!EnsureSchema(
			connection,
			GetQualifiedTableName("tribe_scores"),
			GetQualifiedTableName("score_events"),
			GetQualifiedTableName("tribe_members"),
			errors))
	{
		return entries;
	}

	std::vector<std::array<std::string, 4>> rows;
	std::ostringstream query;
	query
		<< "SELECT tribe_id, last_known_name, score, updated_at FROM " << GetQualifiedTableName("tribe_scores")
		<< " ORDER BY score DESC, last_known_name ASC, tribe_id ASC LIMIT " << limit << ';';

	if (!connection.Query(query.str(), &rows, errors, "Failed to query tribe leaderboard from MySQL"))
	{
		return entries;
	}

	try
	{
		for (const auto& row : rows)
		{
			TribeScoreEntry entry;
			entry.tribe_id = std::stoi(row[0]);
			entry.last_known_name = row[1];
			entry.score = std::stoi(row[2]);
			entry.updated_at = row[3];
			entries.push_back(std::move(entry));
		}
	}
	catch (const std::exception& exception)
	{
		entries.clear();
		if (errors != nullptr)
		{
			AddError(*errors, "Failed to parse leaderboard rows from MySQL: " + std::string(exception.what()));
		}
	}

	return entries;
}

bool ScoreStore::ApplyScoreChange(const ScoreChangeRecord& score_change, std::vector<std::string>* errors) const
{
	const MySqlConnectionSettings settings = GetConnectionSettings();
	OdbcConnection connection;
	if (!connection.Open(settings, errors))
	{
		return false;
	}

	const std::string tribe_scores_table = GetQualifiedTableName("tribe_scores");
	const std::string score_events_table = GetQualifiedTableName("score_events");
	const std::string tribe_members_table = GetQualifiedTableName("tribe_members");

	if (!EnsureSchema(connection, tribe_scores_table, score_events_table, tribe_members_table, errors))
	{
		return false;
	}

	if (!connection.Execute("START TRANSACTION;", errors, "Failed to start MySQL transaction"))
	{
		return false;
	}

	const std::vector<TribeScoreEntry> scores_to_upsert = {
		score_change.attacker_entry,
		score_change.victim_entry
	};

	if (!UpsertScoresInternal(connection, tribe_scores_table, scores_to_upsert, errors))
	{
		connection.Execute("ROLLBACK;", nullptr, "Rollback");
		return false;
	}

	std::ostringstream event_sql;
	event_sql
		<< "INSERT INTO " << score_events_table
		<< " (attacker_tribe_id, victim_tribe_id, blueprint, delta, map_name, created_at, attacker_score, victim_score) VALUES ("
		<< score_change.event_record.attacker_tribe_id << ", "
		<< score_change.event_record.victim_tribe_id << ", '"
		<< EscapeSqlString(score_change.event_record.blueprint) << "', "
		<< score_change.event_record.delta << ", '"
		<< EscapeSqlString(score_change.event_record.map_name) << "', '"
		<< EscapeSqlString(score_change.event_record.created_at) << "', "
		<< score_change.event_record.attacker_score << ", "
		<< score_change.event_record.victim_score << ");";

	if (!connection.Execute(event_sql.str(), errors, "Failed to insert score event into MySQL"))
	{
		connection.Execute("ROLLBACK;", nullptr, "Rollback");
		return false;
	}

	if (score_change.replace_attacker_members
		&& !ReplaceMembersInternal(
			connection,
			tribe_members_table,
			score_change.attacker_entry.tribe_id,
			score_change.attacker_members,
			errors))
	{
		connection.Execute("ROLLBACK;", nullptr, "Rollback");
		return false;
	}

	if (score_change.replace_victim_members
		&& !ReplaceMembersInternal(
			connection,
			tribe_members_table,
			score_change.victim_entry.tribe_id,
			score_change.victim_members,
			errors))
	{
		connection.Execute("ROLLBACK;", nullptr, "Rollback");
		return false;
	}

	if (!connection.Execute("COMMIT;", errors, "Failed to commit MySQL transaction"))
	{
		connection.Execute("ROLLBACK;", nullptr, "Rollback");
		return false;
	}

	return true;
}

std::string ScoreStore::GetConnectionSummary() const
{
	const MySqlConnectionSettings settings = GetConnectionSettings();
	std::ostringstream stream;
	stream << settings.host << ':' << settings.port << '/' << settings.database;
	return stream.str();
}

MySqlConnectionSettings ScoreStore::GetConnectionSettings() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return connection_settings_;
}

std::string ScoreStore::GetQualifiedTableName(const char* base_name) const
{
	const MySqlConnectionSettings settings = GetConnectionSettings();
	return settings.table_prefix + base_name;
}
} // namespace ExoTribeScore
