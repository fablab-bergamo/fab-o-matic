#include "AuthProvider.hpp"

#include <cstdint>
#include <string>

#include "FabBackend.hpp"
#include "Logging.hpp"

namespace fablabbg
{
  namespace Board
  {
    // Only main.cpp instanciates the variables through Board.h file
    extern FabBackend server;
  } // namespace Board

  AuthProvider::AuthProvider(WhiteList list) : whitelist{list}, cache{}
  {
    loadCache();
  }

  /// @brief Cache the user request
  /// @param uid card id of the user
  /// @param name name of the user to be cached
  /// @param level priviledge level of the user
  void AuthProvider::updateCache(card::uid_t uid, FabUser::UserLevel level) const
  {
    // Search for the card in the cache
    for (auto &cached : cache)
    {
      if (cached.uid == uid)
      {
        cached.level = level;
        return;
      }
    }

    // Add into the list
    cache.at(cache_idx).uid = uid;
    cache.at(cache_idx).level = level;

    cache_idx = (cache_idx + 1) % conf::rfid_tags::CACHE_LEN;
  }

  /// @brief Verifies the card ID against the server (if available) or the whitelist
  /// @param uid card ID
  /// @param out a FabUser with an authenticated flag==true if server or whitelist confirmed the ID
  /// @return false if the user was not found on server or whitelist
  auto AuthProvider::tryLogin(card::uid_t uid, FabBackend &server) const -> std::optional<FabUser>
  {
    FabUser user;
    using UserResult = ServerMQTT::UserResult;
    auto uid_str = card::uid_str(uid);

    ESP_LOGD(TAG, "tryLogin called for %s", uid_str.c_str());

    if (!server.isOnline())
      server.connect();

    if (server.isOnline())
    {
      auto response = server.checkCard(uid);
      if (response->request_ok)
      {
        if (response->getResult() == UserResult::Authorized)
        {
          user.authenticated = true;
          user.card_uid = uid;
          user.holder_name = response->holder_name;
          user.user_level = response->user_level;
          // Cache the positive result
          updateCache(uid, response->user_level);

          ESP_LOGD(TAG, " -> online check OK (%s)", user.toString().c_str());

          return user;
        }
        else
        {
          // Invalidate the cache entries
          updateCache(uid, FabUser::UserLevel::Unknown);
        }
      } // if (response->request_ok)

      ESP_LOGD(TAG, " -> online check NOT OK");

      user.authenticated = false;

      if (response->request_ok)
        return std::nullopt;
    }
    else if (auto result = uidInCache(uid); result.has_value()) // Try the cache
    {
      auto [card, level] = result.value();
      user.card_uid = card;
      user.authenticated = true;
      user.user_level = level;
      user.holder_name = "???";
      ESP_LOGD(TAG, " -> cache check OK (%s)", user.toString().c_str());
      return user;
    }

    // Last check, whitelist
    if (auto result = uidInWhitelist(uid); result.has_value())
    {
      auto [card, level, name] = result.value();
      user.card_uid = card;
      user.authenticated = true;
      user.user_level = level;
      user.holder_name = name;
      ESP_LOGD(TAG, " -> whilelist check OK (%s)", user.toString().c_str());

      return user;
    }

    ESP_LOGD(TAG, " -> whilelist check NOK");
    return std::nullopt;
  }

  /// @brief Checks if the card ID is whitelisted
  /// @param uid card ID
  /// @return a whitelistentry object if the card is found in whitelist
  auto AuthProvider::uidInWhitelist(card::uid_t candidate_uid) const -> std::optional<WhiteListEntry>
  {
    auto elem = std::find_if(whitelist.begin(), whitelist.end(),
                             [candidate_uid](const auto &input)
                             {
                               auto [w_uid, w_level, w_name] = input;
                               return w_uid == candidate_uid;
                             });

    if (elem == end(whitelist))
    {
      ESP_LOGD(TAG, "%s not found in whitelist", card::uid_str(candidate_uid).c_str());
      return std::nullopt;
    }

    return {*elem};
  }

  /// @brief Checks if the card ID is whitelisted
  /// @param uid card ID
  /// @return a whitelistentry object if the card is found in whitelist
  auto AuthProvider::uidInCache(card::uid_t candidate_uid) const -> std::optional<CachedFabUser>
  {
    auto elem = std::find_if(cache.begin(), cache.end(),
                             [candidate_uid](const auto &input)
                             {
                               return input.uid == candidate_uid;
                             });

    if (elem == end(cache))
    {
      ESP_LOGD(TAG, "%s not found in cache", card::uid_str(candidate_uid).c_str());
      return std::nullopt;
    }

    return {*elem};
  }

  /// @brief Loads the cache from EEPROM
  auto AuthProvider::loadCache() -> void
  {
    auto config = SavedConfig::LoadFromEEPROM();
    if (!config.has_value())
      return;

    cache_idx = 0;
    for (auto &user : config.value().cachedRfid)
    {
      cache.at(cache_idx).uid = user.uid;
      cache.at(cache_idx).level = user.level;
      ESP_LOGD(TAG, "Cached RFID tag %s", card::uid_str(user.uid).c_str());
      cache_idx++;
      if (cache_idx >= cache.size())
        break;
    }
  }

  /// @brief Sets the whitelist
  /// @param list the whitelist to set
  auto AuthProvider::setWhitelist(WhiteList list) -> void
  {
    whitelist = list;
  }

  /// @brief Saves the cache of RFID to EEPROM
  auto AuthProvider::saveCache() const -> bool
  {
    SavedConfig config = SavedConfig::LoadFromEEPROM().value_or(SavedConfig::DefaultConfig());
    SavedConfig original{config};

    for (auto idx = 0; idx < cache.size(); idx++)
    {
      config.cachedRfid.at(idx).uid = cache.at(idx).uid;
      config.cachedRfid.at(idx).level = cache.at(idx).level;
    }

    // Check if current config is different from updated cachedRfid ignoring order of elements
    if (ScrambledEquals<CachedFabUser, conf::rfid_tags::CACHE_LEN>(original.cachedRfid, config.cachedRfid))
    {
      ESP_LOGD(TAG, "Cache is the same, not saving");
      return true;
    }

    return config.SaveToEEPROM();
  }
} // namespace fablabbg