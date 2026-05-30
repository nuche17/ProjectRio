#include "Core/MSB_HUDStateLoader.h"
#include "Core/MSB_GenerateQuickMatchSetupGeckoCode.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/TagSet.h"
#include "Core/Core.h"
#include <fstream>
#include <string>
#include <picojson.h>


bool LoadStateFromHud(const std::string& path, MSB_QuickMatchState& outState,
                      const std::string& p1Username, const std::string& p2Username)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        ERROR_LOG_FMT(COMMON, "Failed to open HUD file: {}", path);
        return false;
    }

    INFO_LOG_FMT(COMMON, "Found HUD file: {}", path);

    std::string json_str((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());

    picojson::value v;
    std::string err = picojson::parse(v, json_str);

    if (!err.empty())
    {
        ERROR_LOG_FMT(COMMON, "Failed to parse HUD JSON from file: {} ({})", path, err);
        return false;
    }

    INFO_LOG_FMT(COMMON, "Successfully parsed HUD JSON from file: {}", path);

    if (!v.is<picojson::object>())
    {
        ERROR_LOG_FMT(COMMON, "HUD JSON is not an object: {}", path);
        return false;
    }

    INFO_LOG_FMT(COMMON, "HUD is a JSON object.: {}", path);

    const picojson::object& j = v.get<picojson::object>();

    // === P1/P2 to HOME/AWAY MAPPING and VERIFICATION ===
    if (p1Username.empty())
    {
        ERROR_LOG_FMT(COMMON, "Could not find player 1 in netplay data.");
        return false;
    }
    INFO_LOG_FMT(COMMON, "P1 player: {}", p1Username);
    INFO_LOG_FMT(COMMON, "P2 player: {}", p2Username.empty() ? "none (solo)" : p2Username);

    std::string awayPlayer = j.count("Away Player") ?
        std::string(StripWhitespace(j.at("Away Player").get<std::string>())) : "";
    std::string homePlayer = j.count("Home Player") ?
        std::string(StripWhitespace(j.at("Home Player").get<std::string>())) : "";
    INFO_LOG_FMT(COMMON, "Home and away players set: Home {}, Away {}.", homePlayer, awayPlayer);

    // Validate players match the HUD file
    bool p1IsAway = (p1Username == awayPlayer);
    bool p1IsHome = (p1Username == homePlayer);

    if (!p1IsAway && !p1IsHome)
    {
        ERROR_LOG_FMT(COMMON, "P1 player '{}' not found in HUD file. Away='{}', Home='{}'",
                    p1Username, awayPlayer, homePlayer);
        return false;
    }

    // If opponent is known, validate they match the other slot
    if (!p2Username.empty())
    {
        bool opponentIsAway = (p2Username == awayPlayer);
        bool opponentIsHome = (p2Username == homePlayer);

        if (!((p1IsAway && opponentIsHome) || (p1IsHome && opponentIsAway)))
        {
            ERROR_LOG_FMT(COMMON, "Player mismatch. P1='{}', Opponent='{}', HUD Away='{}', HUD Home='{}'",
                        p1Username, p2Username, awayPlayer, homePlayer);
            return false;
        }

        menuInputRestrictionEnabled = true; // if both players present, restrict menu inputs to prevent accidental desync. 
    }
    // Solo game - just verify P1 player is in the file, P2 slot can be "No Player Selected"
    else
    {
        menuInputRestrictionEnabled = false; // if solo, don't restrict menu inputs since P1 needs some control on the captain screen. Used for debugging.
        INFO_LOG_FMT(COMMON, "Solo game detected, skipping opponent validation.");
    }

    INFO_LOG_FMT(COMMON, "Starting parsing HUD to fill out state");


    MSB_QuickMatchState state;
    state.SetP1IsAway(p1IsAway);

    // Captain fielding positions — resolved to batting slots in the batting order section below.
    std::optional<uint8_t> captainFieldingPosP1;
    std::optional<uint8_t> captainFieldingPosP2;

    // === PRE-GAME SETTINGS ===
    // === ROSTERS ===
    // Characters are stored in roster order in the HUD file, but the state
    // needs them in position order (P, C, 1B, 2B, 3B, SS, LF, CF, RF).
    // The "Fielding Position" field tells us what position each roster slot plays.

    for (int i = 0; i < 9; i++)
    {
        std::string p1Key = p1IsAway ? "Away Roster " + std::to_string(i)
                                            : "Home Roster " + std::to_string(i);
        std::string p2Key = p1IsAway ? "Home Roster " + std::to_string(i)
                                            : "Away Roster " + std::to_string(i);

        if (j.count(p1Key))
        {
            const picojson::object& roster = j.at(p1Key).get<picojson::object>();
            uint8_t charID = static_cast<uint8_t>(roster.at("CharID").get<double>());
            uint8_t position = static_cast<uint8_t>(roster.at("Fielding Position").get<double>());
            if (position < 9)
            {
                MSB_Player p;
                p.charID       = charID;
                p.battingHand  = static_cast<uint8_t>(roster.at("Batting Hand").get<double>());
                p.fieldingHand = static_cast<uint8_t>(roster.at("Fielding Hand").get<double>());
                p.superstar    = static_cast<uint8_t>(roster.at("Superstar").get<double>());
                state.GetP1().SetPlayer(position, p);
            }

            if (roster.count("Captain") && roster.at("Captain").get<double>() == 1)
                captainFieldingPosP1 = position;
        }

        if (j.count(p2Key))
        {
            const picojson::object& roster = j.at(p2Key).get<picojson::object>();
            uint8_t charID = static_cast<uint8_t>(roster.at("CharID").get<double>());
            uint8_t position = static_cast<uint8_t>(roster.at("Fielding Position").get<double>());
            if (position < 9)
            {
                MSB_Player p;
                p.charID       = charID;
                p.battingHand  = static_cast<uint8_t>(roster.at("Batting Hand").get<double>());
                p.fieldingHand = static_cast<uint8_t>(roster.at("Fielding Hand").get<double>());
                p.superstar    = static_cast<uint8_t>(roster.at("Superstar").get<double>());
                state.GetP2().SetPlayer(position, p);
            }

            if (roster.count("Captain") && roster.at("Captain").get<double>() == 1)
                captainFieldingPosP2 = position;
        }
    }
    for (int i = 0; i < 9; i++)
    {
        const MSB_Player* p = state.GetP1().GetPlayer(static_cast<uint8_t>(i));
        INFO_LOG_FMT(COMMON,
            "P1 Position {}: CharID {}, Batting Hand {}, Fielding Hand {}, Superstar {}",
            i,
            p && p->charID.has_value()       ? std::to_string(p->charID.value())       : "not set",
            p && p->battingHand.has_value()  ? std::to_string(p->battingHand.value())  : "not set",
            p && p->fieldingHand.has_value() ? std::to_string(p->fieldingHand.value()) : "not set",
            p && p->superstar.has_value()    ? std::to_string(p->superstar.value())    : "not set");
    }
    for (int i = 0; i < 9; i++)
    {
        const MSB_Player* p = state.GetP2().GetPlayer(static_cast<uint8_t>(i));
        INFO_LOG_FMT(COMMON,
            "P2 Position {}: CharID {}, Batting Hand {}, Fielding Hand {}, Superstar {}",
            i,
            p && p->charID.has_value()       ? std::to_string(p->charID.value())       : "not set",
            p && p->battingHand.has_value()  ? std::to_string(p->battingHand.value())  : "not set",
            p && p->fieldingHand.has_value() ? std::to_string(p->fieldingHand.value()) : "not set",
            p && p->superstar.has_value()    ? std::to_string(p->superstar.value())    : "not set");
    }
    INFO_LOG_FMT(COMMON, "Captain P1 fielding pos: {}",
        captainFieldingPosP1.has_value() ? std::to_string(captainFieldingPosP1.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Captain P2 fielding pos: {}",
        captainFieldingPosP2.has_value() ? std::to_string(captainFieldingPosP2.value()) : "not set");


    if (j.count("Away Logo") && j.count("Home Logo"))
    {
        uint32_t awayLogo = static_cast<uint32_t>(j.at("Away Logo").get<double>());
        uint32_t homeLogo = static_cast<uint32_t>(j.at("Home Logo").get<double>());
        (p1IsAway ? state.GetP1() : state.GetP2()).SetLogo(awayLogo);
        (p1IsAway ? state.GetP2() : state.GetP1()).SetLogo(homeLogo);
    }
    INFO_LOG_FMT(COMMON, "Logo Away: {}", (p1IsAway ? state.GetP1() : state.GetP2()).GetLogo().has_value() ? std::to_string((p1IsAway ? state.GetP1() : state.GetP2()).GetLogo().value()) : "not set");
    INFO_LOG_FMT(COMMON, "Logo Home: {}", (p1IsAway ? state.GetP2() : state.GetP1()).GetLogo().has_value() ? std::to_string((p1IsAway ? state.GetP2() : state.GetP1()).GetLogo().value()) : "not set");

    if (j.count("StadiumID"))
    {
        // need to convert stadium ID in HUD to menu index.
        uint8_t stadiumID = static_cast<uint8_t>(j.at("StadiumID").get<double>());
        uint8_t stadiumIndex;
        if (stadiumID == 1) stadiumIndex = 5; // Bowser Stadium
        else if (stadiumID == 4) stadiumIndex = 1; // Peach Garden
        else if (stadiumID == 5) stadiumIndex = 4; // DK Jungle
        else stadiumIndex = stadiumID; // Rest match directly.

        state.SetStadium(stadiumIndex);
    }
    INFO_LOG_FMT(COMMON, "Stadium: {}", state.GetStadium().has_value() ? std::to_string(state.GetStadium().value()) : "not set");

    // handled with half inning value since the game uses this to load offence/defence, not home or away.
    // if (j.count("First Batting Team"))
    // {
    //     uint8_t firstBattingTeam = static_cast<uint8_t>(j.at("First Batting Team").get<double>()); // 0=Original P1, 1=Original P2
    //     if (p1IsAway)
    //         state.SetFirstBatter(firstBattingTeam);
    //     else
    //         state.SetFirstBatter((firstBattingTeam == 0) ? 1 : 0);
    // }
    // INFO_LOG_FMT(COMMON, "First Batter: {}", state.GetFirstBatter().has_value() ? std::to_string(state.GetFirstBatter().value()) : "not set");

    if (j.count("Star Skills On"))
        state.SetStarSkills(static_cast<uint8_t>(j.at("Star Skills On").get<double>()));
    INFO_LOG_FMT(COMMON, "Star Skills: {}", state.GetStarSkills().has_value() ? std::to_string(state.GetStarSkills().value()) : "not set");

    if (j.count("Innings Selected"))
        state.SetInningsSelected(static_cast<uint8_t>(j.at("Innings Selected").get<double>()));
    INFO_LOG_FMT(COMMON, "Innings Selected: {}", state.GetInningsSelected().has_value() ? std::to_string(state.GetInningsSelected().value()) : "not set");

    if (j.count("Mercy On"))
        state.SetMercy(static_cast<uint8_t>(j.at("Mercy On").get<double>()));
    INFO_LOG_FMT(COMMON, "Mercy: {}", state.GetMercy().has_value() ? std::to_string(state.GetMercy().value()) : "not set");

    // === IN-GAME STATE ===

    if (j.count("Inning"))
        state.SetInning(static_cast<uint32_t>(j.at("Inning").get<double>()));
    INFO_LOG_FMT(COMMON, "Inning: {}", state.GetInning().has_value() ? std::to_string(state.GetInning().value()) : "not set");

    if (j.count("Half Inning"))
    {
        uint8_t halfInning = static_cast<uint8_t>(j.at("Half Inning").get<double>());

        state.SetHalfInning(halfInning);
        state.SetBattingTeam(static_cast<uint32_t>(halfInning));
        state.SetFieldingTeam(static_cast<uint32_t>(1 - halfInning));

        if (p1IsAway) state.SetFirstBatter(halfInning);
        else state.SetFirstBatter(1 - halfInning);
    }
    INFO_LOG_FMT(COMMON, "Half Inning: {}", state.GetHalfInning().has_value() ? std::to_string(state.GetHalfInning().value()) : "not set");
    INFO_LOG_FMT(COMMON, "Batting Team: {}", state.GetBattingTeam().has_value() ? std::to_string(state.GetBattingTeam().value()) : "not set");
    INFO_LOG_FMT(COMMON, "Fielding Team: {}", state.GetFieldingTeam().has_value() ? std::to_string(state.GetFieldingTeam().value()) : "not set");
    INFO_LOG_FMT(COMMON, "First Batter: {}", state.GetFirstBatter().has_value() ? std::to_string(state.GetFirstBatter().value()) : "not set");

    if (j.count("Away Score"))
        state.SetAwayScore(static_cast<uint16_t>(j.at("Away Score").get<double>()));
    INFO_LOG_FMT(COMMON, "Away Score: {}", state.GetAwayScore().has_value() ? std::to_string(state.GetAwayScore().value()) : "not set");

    if (j.count("Home Score"))
        state.SetHomeScore(static_cast<uint16_t>(j.at("Home Score").get<double>()));
    INFO_LOG_FMT(COMMON, "Home Score: {}", state.GetHomeScore().has_value() ? std::to_string(state.GetHomeScore().value()) : "not set");

    if (j.count("Away Inning Scores"))
    {
        const picojson::array& scores = j.at("Away Inning Scores").get<picojson::array>();
        for (int i = 0; i < static_cast<int>(scores.size()) && i < 18; i++)
            state.SetAwayInningScore(i, static_cast<uint16_t>(scores[i].get<double>()));
    }

    if (j.count("Home Inning Scores"))
    {
        const picojson::array& scores = j.at("Home Inning Scores").get<picojson::array>();
        for (int i = 0; i < static_cast<int>(scores.size()) && i < 18; i++)
            state.SetHomeInningScore(i, static_cast<uint16_t>(scores[i].get<double>()));
    }
    for (int i = 0; i < 18; i++)
    {
        if (state.GetAwayInningScore(i).has_value())
            INFO_LOG_FMT(COMMON, "Away Inning {} Score: {}", i, state.GetAwayInningScore(i).value());
        if (state.GetHomeInningScore(i).has_value())
            INFO_LOG_FMT(COMMON, "Home Inning {} Score: {}", i, state.GetHomeInningScore(i).value());
    }

    if (j.count("Strikes"))
        state.SetStrikes(static_cast<uint32_t>(j.at("Strikes").get<double>()));
    INFO_LOG_FMT(COMMON, "Strikes: {}", state.GetStrikes().has_value() ? std::to_string(state.GetStrikes().value()) : "not set");

    if (j.count("Balls"))
        state.SetBalls(static_cast<uint32_t>(j.at("Balls").get<double>()));
    INFO_LOG_FMT(COMMON, "Balls: {}", state.GetBalls().has_value() ? std::to_string(state.GetBalls().value()) : "not set");

    if (j.count("Outs"))
        state.SetOuts(static_cast<uint32_t>(j.at("Outs").get<double>()));
    INFO_LOG_FMT(COMMON, "Outs: {}", state.GetOuts().has_value() ? std::to_string(state.GetOuts().value()) : "not set");

    if (j.count("Away Stars"))
        (p1IsAway ? state.GetP1() : state.GetP2()).SetTeamStars(static_cast<uint8_t>(j.at("Away Stars").get<double>()));
    INFO_LOG_FMT(COMMON, "Away Stars (P{}): {}", p1IsAway ? "1" : "2",
        (p1IsAway ? state.GetP1() : state.GetP2()).GetTeamStars().has_value() ?
            std::to_string((p1IsAway ? state.GetP1() : state.GetP2()).GetTeamStars().value()) : "not set");

    if (j.count("Home Stars"))
        (p1IsAway ? state.GetP2() : state.GetP1()).SetTeamStars(static_cast<uint8_t>(j.at("Home Stars").get<double>()));
    INFO_LOG_FMT(COMMON, "Home Stars (P{}): {}", p1IsAway ? "2" : "1",
        (p1IsAway ? state.GetP2() : state.GetP1()).GetTeamStars().has_value() ?
            std::to_string((p1IsAway ? state.GetP2() : state.GetP1()).GetTeamStars().value()) : "not set");

    if (j.count("Star Chance"))
        state.SetIsStarChance(static_cast<uint8_t>(j.at("Star Chance").get<double>()));
    INFO_LOG_FMT(COMMON, "Star Chance: {}", state.GetIsStarChance().has_value() ? std::to_string(state.GetIsStarChance().value()) : "not set");

    // === POSITIONS BY BATTING ORDER ===
    if (j.count("Away Batter Roster Loc") && j.count("Home Batter Roster Loc"))
    {
        int awayStartingBatter = static_cast<int>(j.at("Away Batter Roster Loc").get<double>());
        int homeStartingBatter = static_cast<int>(j.at("Home Batter Roster Loc").get<double>());

        MSB_Team& awayTeam = p1IsAway ? state.GetP1() : state.GetP2();
        MSB_Team& homeTeam = p1IsAway ? state.GetP2() : state.GetP1();

        for (int i = 0; i < 9; i++)
        {
            std::string awayKey = "Away Roster " + std::to_string((i + awayStartingBatter) % 9);
            std::string homeKey = "Home Roster " + std::to_string((i + homeStartingBatter) % 9);

            if (j.count(awayKey))
            {
                const picojson::object& roster = j.at(awayKey).get<picojson::object>();
                uint8_t position = static_cast<uint8_t>(roster.at("Fielding Position").get<double>());
                if (position < 9)
                {
                    const MSB_Player* existing = awayTeam.GetPlayer(position);
                    if (existing)
                    {
                        MSB_Player updated = *existing;
                        updated.battingOrderSlot = static_cast<uint8_t>(i);
                        awayTeam.SetPlayer(position, updated);
                    }
                }
            }

            if (j.count(homeKey))
            {
                const picojson::object& roster = j.at(homeKey).get<picojson::object>();
                uint8_t position = static_cast<uint8_t>(roster.at("Fielding Position").get<double>());
                if (position < 9)
                {
                    const MSB_Player* existing = homeTeam.GetPlayer(position);
                    if (existing)
                    {
                        MSB_Player updated = *existing;
                        updated.battingOrderSlot = static_cast<uint8_t>(i);
                        homeTeam.SetPlayer(position, updated);
                    }
                }
            }
        }

        // Resolve captain batting slots now that batting order slots are set.
        if (captainFieldingPosP1.has_value())
        {
            const MSB_Player* captain = state.GetP1().GetPlayer(captainFieldingPosP1.value());
            if (captain && captain->battingOrderSlot.has_value())
                state.GetP1().SetCaptainBattingSlot(captain->battingOrderSlot.value());
        }
        if (captainFieldingPosP2.has_value())
        {
            const MSB_Player* captain = state.GetP2().GetPlayer(captainFieldingPosP2.value());
            if (captain && captain->battingOrderSlot.has_value())
                state.GetP2().SetCaptainBattingSlot(captain->battingOrderSlot.value());
        }
    }
    for (int i = 0; i < 9; i++)
    {
        const MSB_Team& awayTeam = p1IsAway ? state.GetP1() : state.GetP2();
        const MSB_Team& homeTeam = p1IsAway ? state.GetP2() : state.GetP1();
        const MSB_Player* awayP = awayTeam.GetPlayerByBattingSlot(static_cast<uint8_t>(i));
        const MSB_Player* homeP = homeTeam.GetPlayerByBattingSlot(static_cast<uint8_t>(i));
        if (awayP && awayP->position.has_value())
            INFO_LOG_FMT(COMMON, "Away Batting Order {}: Position {}", i, awayP->position.value());
        if (homeP && homeP->position.has_value())
            INFO_LOG_FMT(COMMON, "Home Batting Order {}: Position {}", i, homeP->position.value());
    }

    // === RUNNERS ===
    // Runners are indexed 1-3 for each base (1B, 2B, 3B)
    // SetRunner derives fielding position and charID from the batting team via batting slot.

    const std::string runnerKeys[3] = {"Runner 1B", "Runner 2B", "Runner 3B"};

    for (int i = 0; i < 3; i++)
    {
        const std::string& key = runnerKeys[i];
        if (j.count(key))
        {
            const picojson::object& runner = j.at(key).get<picojson::object>();

            if (runner.count("Runner Roster Loc") && j.count("Batter Roster Loc"))
            {
                uint8_t batterRosterLoc    = static_cast<uint8_t>(j.at("Batter Roster Loc").get<double>());
                uint8_t runnerRosterLocRaw = static_cast<uint8_t>(runner.at("Runner Roster Loc").get<double>());
                uint8_t battingSlot        = static_cast<uint8_t>((runnerRosterLocRaw - batterRosterLoc + 9) % 9);
                state.SetRunner(i, battingSlot);
            }

            if (runner.count("Runner Char Id"))
                INFO_LOG_FMT(COMMON, "Runner {} Char Id (HUD): {}", key,
                    static_cast<uint16_t>(runner.at("Runner Char Id").get<double>()));
        }
    }
    for (int i = 0; i < 3; i++)
    {
        INFO_LOG_FMT(COMMON, "Runner {}: RosterSpot={}, CharID={}",
                    runnerKeys[i],
                    state.GetRunnerFieldingPosition(i).has_value() ? std::to_string(state.GetRunnerFieldingPosition(i).value()) : "not set",
                    state.GetRunnerCharacterID(i).has_value() ? std::to_string(state.GetRunnerCharacterID(i).value()) : "not set");
    }

    // === STAMINA ===
    // Stamina is stored per-character in defensive stats.
    // Stamina lives on MSB_Player::pitcherStamina, indexed by batting slot.

    if (j.count("Away Batter Roster Loc") && j.count("Home Batter Roster Loc"))
    {
        int awayStartingBatter = static_cast<int>(j.at("Away Batter Roster Loc").get<double>());
        int homeStartingBatter = static_cast<int>(j.at("Home Batter Roster Loc").get<double>());

        for (int i = 0; i < 9; i++)
        {
            int awayAdjustedIndex = (i + awayStartingBatter) % 9;
            int homeAdjustedIndex = (i + homeStartingBatter) % 9;

            std::string p1Key = p1IsAway ? "Away Roster " + std::to_string(awayAdjustedIndex)
                                                : "Home Roster " + std::to_string(homeAdjustedIndex);
            std::string p2Key = p1IsAway ? "Home Roster " + std::to_string(homeAdjustedIndex)
                                                : "Away Roster " + std::to_string(awayAdjustedIndex);

            if (j.count(p1Key))
            {
                const picojson::object& roster = j.at(p1Key).get<picojson::object>();
                const picojson::object& defensiveStats = roster.at("Defensive Stats").get<picojson::object>();
                if (defensiveStats.count("Stamina"))
                {
                    const MSB_Player* existing = state.GetP1().GetPlayerByBattingSlot(static_cast<uint8_t>(i));
                    if (existing && existing->position.has_value())
                    {
                        MSB_Player updated = *existing;
                        updated.pitcherStamina = static_cast<uint16_t>(defensiveStats.at("Stamina").get<double>());
                        state.GetP1().SetPlayer(existing->position.value(), updated);
                    }
                }
            }

            if (j.count(p2Key))
            {
                const picojson::object& roster = j.at(p2Key).get<picojson::object>();
                const picojson::object& defensiveStats = roster.at("Defensive Stats").get<picojson::object>();
                if (defensiveStats.count("Stamina"))
                {
                    const MSB_Player* existing = state.GetP2().GetPlayerByBattingSlot(static_cast<uint8_t>(i));
                    if (existing && existing->position.has_value())
                    {
                        MSB_Player updated = *existing;
                        updated.pitcherStamina = static_cast<uint16_t>(defensiveStats.at("Stamina").get<double>());
                        state.GetP2().SetPlayer(existing->position.value(), updated);
                    }
                }
            }
        }
    }
    for (int i = 0; i < 9; i++)
    {
        const MSB_Player* p1 = state.GetP1().GetPlayerByBattingSlot(static_cast<uint8_t>(i));
        const MSB_Player* p2 = state.GetP2().GetPlayerByBattingSlot(static_cast<uint8_t>(i));
        if (p1 && p1->pitcherStamina.has_value())
            INFO_LOG_FMT(COMMON, "P1 Stamina Batting Slot {}: {}", i, p1->pitcherStamina.value());
        if (p2 && p2->pitcherStamina.has_value())
            INFO_LOG_FMT(COMMON, "P2 Stamina Batting Slot {}: {}", i, p2->pitcherStamina.value());
    }

    // === DEBUG LOGGING OF LOADED STATE ===
    INFO_LOG_FMT(COMMON, "=== HUD State Loaded ===");

    outState = state;
    return true;
}

int allowLoadFromHUD(const std::string& path,
                     const std::string& p1Username, const std::string& p2Username)
{
    INFO_LOG_FMT(COMMON, "Starting to check if HUD state load is allowed");

    // ===== check HUD file exists and can be parsed. =====
    std::ifstream file(path);
    if (!file.is_open())
    {
        ERROR_LOG_FMT(COMMON, "Failed to open HUD file: {}", path);
        return 2;
    }

    std::string json_str((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());

    picojson::value v;
    std::string err = picojson::parse(v, json_str);

    if (!err.empty())
    {
        ERROR_LOG_FMT(COMMON, "Failed to parse HUD JSON from file: {} ({})", path, err);
        return 2;
    }

    if (!v.is<picojson::object>())
    {
        ERROR_LOG_FMT(COMMON, "HUD JSON is not an object: {}", path);
        return 2;
    }

    const picojson::object& j = v.get<picojson::object>();

    INFO_LOG_FMT(COMMON, "File check passed, checking gamemode.");

    // ===== check lobby gamemode matches HUD =====
    // Get the active tagset from the current netplay session
    std::optional<Tag::TagSet> activeTagSet = Core::GetActiveTagSet(true);

    // Check if HUD file has a TagSetID
    if (j.count("TagSetID"))
    {
        int hudTagSetId = static_cast<int>(j.at("TagSetID").get<double>());

        if (activeTagSet.has_value())
        {
            // Both HUD and lobby have a tagset - they must match
            if (hudTagSetId != activeTagSet.value().id)
            {
                ERROR_LOG_FMT(COMMON, "TagSet mismatch. Lobby TagSet ID={}, HUD TagSet ID={}",
                            activeTagSet.value().id, hudTagSetId);
                return 3;
            }
            INFO_LOG_FMT(COMMON, "TagSet match confirmed: ID={}", hudTagSetId);
        }
        else
        {
            // HUD has a tagset but lobby does not
             ERROR_LOG_FMT(COMMON, "HUD has TagSet ID={} but no game mode is active in lobby.", hudTagSetId);
            // return 3; Actually allowing this situation so no game mode can be used for anything to allow for better customization.
        }
    }
    else if (activeTagSet.has_value())
    {
        // Lobby has a tagset but HUD does not record one
        ERROR_LOG_FMT(COMMON, "Lobby has TagSet ID={} but HUD file has no TagSetID field.",
                    activeTagSet.value().id);
        return 3;
    }

    INFO_LOG_FMT(COMMON, "Gamemode check passed, checking players.");

    // ===== check players match HUD =====
    if (p1Username.empty())
    {
        ERROR_LOG_FMT(COMMON, "Could not find player 1 in netplay data.");
        return 4;
    }
    INFO_LOG_FMT(COMMON, "P1 player: {}", p1Username);
    INFO_LOG_FMT(COMMON, "P2 player: {}", p2Username.empty() ? "none (solo)" : p2Username);

    std::string awayPlayer = j.count("Away Player") ?
        std::string(StripWhitespace(j.at("Away Player").get<std::string>())) : "";
    std::string homePlayer = j.count("Home Player") ?
        std::string(StripWhitespace(j.at("Home Player").get<std::string>())) : "";

    // Validate players match the HUD file
    bool p1IsAway = (p1Username == awayPlayer);
    bool p1IsHome = (p1Username == homePlayer);

    if (!p1IsAway && !p1IsHome)
    {
        ERROR_LOG_FMT(COMMON, "P1 player '{}' not found in HUD file. Away='{}', Home='{}'",
                    p1Username, awayPlayer, homePlayer);
        return 4;
    }

    // If opponent is known, validate they match the other slot
    if (!p2Username.empty())
    {
        bool opponentIsAway = (p2Username == awayPlayer);
        bool opponentIsHome = (p2Username == homePlayer);

        if (!((p1IsAway && opponentIsHome) || (p1IsHome && opponentIsAway)))
        {
            ERROR_LOG_FMT(COMMON, "Player mismatch. P1='{}', Opponent='{}', HUD Away='{}', HUD Home='{}'",
                        p1Username, p2Username, awayPlayer, homePlayer);
            return 4;
        }
    }

    INFO_LOG_FMT(COMMON, "All checks passed. HUD can load.");

    return 0;
}