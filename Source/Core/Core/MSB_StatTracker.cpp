
#include "Core/MSB_StatTracker.h"

#include <iomanip>
#include <fstream>
#include <ctime>

//For LocalPLayers
#include "Common/CommonPaths.h"
#include "Common/IniFile.h"
#include "Core/LocalPlayersConfig.h"
#include "Common/Version.h"

#include "Common/Swap.h"
#include "Common/Thread.h"

// Package for rendering info on screen
#include "VideoCommon/OnScreenDisplay.h"
#include <fmt/chrono.h>
#include <fmt/format.h>

#include <iostream>
#include "Config/MainSettings.h"

#include "Common/TagSet.h"

#include "Core/GeckoCodeConfig.h"
#include <cmath>
#include <sstream>
#include "Core/NetPlayServer.h"

void StatTracker::Run(const Core::CPUThreadGuard& guard)
{
    lookForTriggerEvents(guard);
}

void StatTracker::lookForTriggerEvents(const Core::CPUThreadGuard& guard)
{
    // if (m_game_state != m_game_state_prev) {
    //     state_logger.writeToFile(c_game_state[m_game_state]);
    //     m_game_state_prev = m_game_state;
    // }

    if (m_event_state != m_event_state_prev) {
        // Write state on every change
        //state_logger.writeToFile(c_event_state[m_event_state]);
        if (m_game_info.currentEventVld()){
            // Add state of current event to event.history for logging purposes
            m_game_info.getCurrentEvent().history.push_back(m_event_state);

            if (m_event_state == EVENT_STATE::FINAL_RESULT) {
            // Write state details on play over
                state_logger.writeToFile(fmt::format(
                    "Game State: {}\n"
                    "Event State: {}\n"
                    "Event Num: {}\n"
                    "Inning: {}\n"
                    "Half Inning: {}\n"
                    "Batter: {}\n"
                    "Pitcher: {}\n"
                    "Event History: \n{}\n",
                    c_game_state[m_game_state],
                    c_event_state[m_event_state],
                    m_game_info.getCurrentEvent().event_num,
                    m_game_info.getCurrentEvent().inning,
                    m_game_info.getCurrentEvent().half_inning,
                    (m_game_info.getCurrentEvent().runner_batter) ? cCharIdToCharName.at(m_game_info.getCurrentEvent().runner_batter->char_id) : "None",
                    (m_game_info.getCurrentEvent().pitch) ? cCharIdToCharName.at(m_game_info.getCurrentEvent().pitch->pitcher_char_id) : "Pitch Not Thrown Yet",
                    m_game_info.getCurrentEvent().stringifyHistory()
                ));
            
                            
                if (m_game_info.getCurrentEvent().result_of_atbat != 0) {
                    u8 half_inning = m_game_info.getCurrentEvent().half_inning;

                    u8 batter_char_id = m_game_info.character_summaries[half_inning][m_game_info.getCurrentEvent().batter_roster_loc].char_id;
                    u8 pitcher_char_id = m_game_info.character_summaries[!half_inning][m_game_info.getCurrentEvent().pitcher_roster_loc].char_id;

                    std::string batter_name = cCharIdToCharName.at(batter_char_id);
                    std::string pitcher_name = cCharIdToCharName.at(pitcher_char_id);

                    if (Config::Get(Config::MAIN_ENABLE_DEBUGGING))
                    {
                      OSD::AddTypedMessage(
                          OSD::MessageType::GameStatePreviousPlayResult,
                          fmt::format("====PREVIOUS EVENT RESULT====\n"
                                      "Dead Ball Reason: {}\n"
                                      "Result of At Bat: {}\n"
                                      "RBI: {}\n"
                                      "Outs: {}\n"
                                      "Pitcher: {}\n"
                                      "Batter: {}\n",
                                      m_game_info.getCurrentEvent().dead_ball_reason,
                                      m_game_info.getCurrentEvent().result_of_atbat,
                                      m_game_info.getCurrentEvent().rbi,
                                      m_game_info.getCurrentEvent().outs, pitcher_name,
                                      batter_name),
                          10000, OSD::Color::RED);

                      OSD::AddTypedMessage(
                          OSD::MessageType::GameStatePreviousPlayInfo,
                          fmt::format(
                              "====PREVIOUS EVENT SUMMARY====\n"
                              "Event Num: {}\n"
                              "Inning: {}\n"
                              "Half Inning: {}\n"
                              "Batter: {}\n"
                              "Pitcher: {}\n"
                              "Event History: \n{}\n",
                              m_game_info.getCurrentEvent().event_num,
                              m_game_info.getCurrentEvent().inning,
                              m_game_info.getCurrentEvent().half_inning,
                              (m_game_info.getCurrentEvent().runner_batter) ?
                                  cCharIdToCharName.at(
                                      m_game_info.getCurrentEvent().runner_batter->char_id) :
                                  "None",
                              (m_game_info.getCurrentEvent().pitch) ?
                                  cCharIdToCharName.at(
                                      m_game_info.getCurrentEvent().pitch->pitcher_char_id) :
                                  "Pitch Not Thrown Yet",
                              m_game_info.getCurrentEvent().stringifyHistory()),
                          10000, OSD::Color::BLUE);
                    }
                };
            }
        }
        // Update previous event state variable for checking purposes
        m_event_state_prev = m_event_state;
    }

    if (m_game_state == GAME_STATE::INGAME) {
        if (m_game_info.currentEventVld()){
          if (Config::Get(Config::MAIN_ENABLE_DEBUGGING))
          {
            OSD::AddTypedMessage(
                OSD::MessageType::GameStateInfo,
                fmt::format(
                    "====CURRENT EVENT SUMMARY====\n"
                    "Game State: {}\n"
                    "Event State: {}\n"
                    "Event Num: {}\n"
                    "Inning: {}\n"
                    "Half Inning: {}\n"
                    "Batter: {}\n"
                    "Pitcher: {}\n"
                    "Event History: \n{}\n",
                    c_game_state[m_game_state], c_event_state[m_event_state],
                    m_game_info.getCurrentEvent().event_num, m_game_info.getCurrentEvent().inning,
                    m_game_info.getCurrentEvent().half_inning,
                    (m_game_info.getCurrentEvent().runner_batter) ?
                        cCharIdToCharName.at(m_game_info.getCurrentEvent().runner_batter->char_id) :
                        "None",
                    (m_game_info.getCurrentEvent().pitch) ?
                        cCharIdToCharName.at(m_game_info.getCurrentEvent().pitch->pitcher_char_id) :
                        "Pitch Not Thrown Yet",
                    m_game_info.getCurrentEvent().stringifyHistory()),
                3000, OSD::Color::CYAN);
          }
        }
    } else {
      if (Config::Get(Config::MAIN_ENABLE_DEBUGGING))
      {
        OSD::AddTypedMessage(OSD::MessageType::GameStateInfo, fmt::format(
            "Game State: {}\n"
            "Event State: {}\n",
            c_game_state[m_game_state],
            c_event_state[m_event_state]            
        ), 200, OSD::Color::CYAN);
        }
    }

    //At Bat State Machine
    if (m_game_state == GAME_STATE::INGAME){
        switch(m_event_state){
            case (EVENT_STATE::INIT_EVENT):
                //Create new event, collect runner data

                //Capture the rising edge of the AtBat Scene
        if (PowerPC::MMU::HostRead_U8(guard, aGameControlStateCurr) == 0x1 &&
            PowerPC::MMU::HostRead_U8(guard, aGameControlStatePrev) != 0x1)
        {

                    m_game_info.events[m_game_info.event_num] = Event();
                    m_game_info.getCurrentEvent().event_num = m_game_info.event_num;

                    //Get users and captains
                    if (m_game_info.init_game == true) {
                        m_game_info.init_game = false;
                        initPlayerInfo(guard);
                    }

                    logEventState(guard, m_game_info.getCurrentEvent());
                    logGameInfo(guard);

                    m_game_info.getCurrentEvent().runner_batter = logRunnerInfo(guard, 0);
                    m_game_info.getCurrentEvent().runner_1 = logRunnerInfo(guard, 1);
                    m_game_info.getCurrentEvent().runner_2 = logRunnerInfo(guard, 2);
                    m_game_info.getCurrentEvent().runner_3 = logRunnerInfo(guard, 3);

                    // Initialize BOTH teams' fielder trackers up front (not just the fielding team).
                    // The batting/order/position struct is already populated in memory for both
                    // teams at game setup, so initTracker reads valid current positions for the
                    // batting team too. Without this, the batting team's fielder_map.current_pos
                    // retains stale data from the previous game (e.g. across a fast reset), which
                    // leaks the prior game's fielding layout into this game's first HUD write.
                    // The aBattingOrderAndPosition table is laid out by away/home (block 0 =
                    // away, block 1 = home), NOT by controller-port team0/team1. So the tracker
                    // index and the team_id passed to initTracker are the same away/home value.
                    // (Do not apply the team0/team1 port remap here that the pitcher/character
                    // stat tables require -- that swaps the two teams' fielder maps whenever the
                    // away player is on the team1 port.)
                    for (u8 away_home = 0; away_home < 2; ++away_home){
                        if (!m_fielder_tracker[away_home].initialized){
                            std::cout << " Initializing fielders for team: " << std::to_string(away_home) << "\n";
                            m_fielder_tracker[away_home].initTracker(guard, away_home);
                        }
                    }

                    m_event_state = EVENT_STATE::WAITING_FOR_EVENT;

                    std::cout << "Init event " << std::to_string(m_game_info.event_num) << "\n";
                }
                else if (PowerPC::MMU::HostRead_U32(guard, aGameId) == 0){
                    onGameQuit(guard);

                    //Remove current event, wasn't finished
                    auto it = m_game_info.events.find(m_game_info.event_num);
                    m_game_info.events.erase(it);

                    m_event_state = EVENT_STATE::GAME_OVER;
                }
                break;
            //Look for Pitch
            case (EVENT_STATE::WAITING_FOR_EVENT):
                //Handle quit to main menu
                if (PowerPC::MMU::HostRead_U32(guard, aGameId) == 0){
                    onGameQuit(guard);

                    //Remove current event, wasn't finished
                    auto it = m_game_info.events.find(m_game_info.event_num);
                    m_game_info.events.erase(it);

                    m_event_state = EVENT_STATE::GAME_OVER;
                    break;
                }

                //Update OngoingGame
                if (!m_game_info.post_ongoing_game && m_game_info.update_ongoing_game){
                    updateOngoingGame(m_game_info.getCurrentEvent());
                    m_game_info.update_ongoing_game = false;
                }

                //Trigger Events to look for
                //1. Are runners stealing and pitcher stepped off the mound
                //2. Has pitch started?
                //3. Has game been paused, reinit 
                if (PowerPC::MMU::HostRead_U8(guard, aGameControlStateCurr) == 0xb){
                    std::cout << "Game paused, need to re-init event " << std::to_string(m_game_info.event_num) << "\n";
                    logGameInfo(guard);

                    // if there's a pause before the first pitch, do not run the update function since the initial post of the ongoing game hasn't happened yet.
                    if (m_game_info.post_ongoing_game == false)
                    {
                        updateOngoingGame(m_game_info.getCurrentEvent());
                    }
                    
                    m_event_state = EVENT_STATE::INIT_EVENT;
                }
                //Watch for Runners Stealing
                if (PowerPC::MMU::HostRead_U8(guard, aAB_PitchThrown) || PowerPC::MMU::HostRead_U8(guard, aAB_PickoffAttempt)){
                    //If HUD not produced for this event, produce HUD JSON
                    logGameInfo(guard);

                    if (m_game_info.getCurrentEvent().write_hud_ab.first) {
                        if (!NetPlay::NetPlay_IsDesyncDetected())
                        {
                            // decoded version
                            std::string hud_file_path = File::GetUserPath(D_HUDFILES_IDX) + "decoded.hud.json";
                            std::string json = getHUDJSON(std::to_string(m_game_info.event_num) + "a", m_game_info.getCurrentEvent(), m_game_info.previous_state, true);
                            File::Delete(hud_file_path);
                            File::WriteStringToFile(hud_file_path, json);

                            // encoded version
                            hud_file_path = File::GetUserPath(D_HUDFILES_IDX) + "hud.json";
                            json = getHUDJSON(std::to_string(m_game_info.event_num) + "a", m_game_info.getCurrentEvent(), m_game_info.previous_state, false);
                            File::Delete(hud_file_path);
                            File::WriteStringToFile(hud_file_path, json);
                        }

                        //No longer need to write HUD B
                        m_game_info.getCurrentEvent().write_hud_ab.first = false;
                    }

                    if(PowerPC::MMU::HostRead_U8(guard, aAB_PitchThrown)){
                        std::cout << "Pitch detected!\n";

                        //Check for fielder swaps
                        std::cout << " Evaluating fielders for team: " << std::to_string(!m_game_info.getCurrentEvent().half_inning) << "\n";
                        m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].evaluateFielders(guard);

                        m_game_info.getCurrentEvent().pitch = std::make_optional(Pitch());

                        //Check if pitcher was at center of mound, if so this is a potential DB
                        if (PowerPC::MMU::HostRead_U8(guard, aFielder_Pos_X) == 0){
                            m_game_info.getCurrentEvent().pitch->potential_db = true;
                            std::cout << "Potential DB!\n";
                        }

                        //Pitch has started
                        m_event_state = EVENT_STATE::PITCH_RESULT;
                    }
                    else if(PowerPC::MMU::HostRead_U8(guard, aAB_PickoffAttempt)) {
                        std::cout << "Pick off attempt detected!\n";
                        m_event_state = EVENT_STATE::MONITOR_RUNNERS;
                        m_game_info.getCurrentEvent().pick_off_attempt = true;
                    }
                }
                break;
            case (EVENT_STATE::PITCH_RESULT): //Look for contact or end of pitch

                // === Monitor ===
                //DBs
                //If the pitcher started in the center of the mound this is a potential DB
                //If the ball curves at any point it is no longer a DB
                if (m_game_info.getCurrentEvent().pitch->potential_db && (PowerPC::MMU::HostRead_U8(guard, aAB_PitcherHasCtrlofPitch) == 1)) {
                    if (floatConverter(PowerPC::MMU::HostRead_U32(guard, aAB_PitchCurveInput)) != 0) {
                        std::cout << "No longer potential DB!\n";
                        m_game_info.getCurrentEvent().pitch->potential_db = false;
                    }
                }
                //While pitch is in flight, record runner activity 
                //Log if runners are stealing
                if (m_game_info.getCurrentEvent().runner_1) {
                    logRunnerEvents(guard, & m_game_info.getCurrentEvent().runner_1.value());
                }
                if (m_game_info.getCurrentEvent().runner_2) {
                    logRunnerEvents(guard, & m_game_info.getCurrentEvent().runner_2.value());
                }
                if (m_game_info.getCurrentEvent().runner_3) {
                    logRunnerEvents(guard, &m_game_info.getCurrentEvent().runner_3.value());
                }

                // === Transition ===

                //Conditions to leave the state: Contact, Ball beyond batter, HBP
                //Contact
                if (PowerPC::MMU::HostRead_U8(guard, aAB_ContactMade)){
                    logPitch(guard, m_game_info.getCurrentEvent());
                    logContact(guard, m_game_info.getCurrentEvent());
                    m_event_state = EVENT_STATE::CONTACT_RESULT;
                }
                //If the ball gets behind the batter while mid pitch OR play flag is false (safety incase we miss the first cond), record miss
                else if (PowerPC::MMU::HostRead_U8(guard, aAB_MissedBall)){
                    logPitch(guard, m_game_info.getCurrentEvent());
                    m_event_state = EVENT_STATE::MONITOR_RUNNERS;
                }
                else if (PowerPC::MMU::HostRead_U8(guard, aAB_HitByPitch) == 1){
                    //Log HBP
                    logPitch(guard, m_game_info.getCurrentEvent());
                    if (!PowerPC::MMU::HostRead_U8(guard, aAB_PitchThrown)) {
                        m_game_info.getCurrentEvent().result_of_atbat = PowerPC::MMU::HostRead_U8(guard, aAB_FinalResult);
                        m_game_info.getCurrentEvent().dead_ball_reason = PowerPC::MMU::HostRead_U8(guard, aAB_DeadBallReason);
                        m_event_state = EVENT_STATE::PLAY_OVER;
                    }
                }

                break;
            case (EVENT_STATE::CONTACT_RESULT):                
                //Poll the stadium hazards every frame the ball is live
                logHazardEvents(guard, &m_game_info.getCurrentEvent().pitch->contact.value());
                if (PowerPC::MMU::HostRead_U8(guard, aAB_ContactResult) != 0){
                    //Indicate that pitch resulted in contact and log contact details
                    m_game_info.getCurrentEvent().pitch->pitch_result = 6;
                    logContactResult(guard, &m_game_info.getCurrentEvent().pitch->contact.value()); //Land vs Caught vs Foul, Landing POS.
                    m_event_state = EVENT_STATE::MONITOR_RUNNERS;
                    break;
                }

                // === Monitor === 
                //Log ball pos.
                //Ball is still in air
                else{
                    Contact* contact = &m_game_info.getCurrentEvent().pitch->contact.value();
                    //Final Result Ball
                    contact->ball_x_pos.read_value(guard);
                    contact->ball_y_pos.read_value(guard);
                    contact->ball_z_pos.read_value(guard);
                }
                //Could bobble before the ball hits the ground.
                //Search for bobble if we haven't recorded one yet and the ball hasn't been collected yet
                if (!m_game_info.getCurrentEvent().pitch->contact->first_fielder.has_value() 
                 && !m_game_info.getCurrentEvent().pitch->contact->collect_fielder.has_value()){
                     
                    //Returns a fielder that has bobbled if any exist. Otherwise optional is nullptr
                    m_game_info.getCurrentEvent().pitch->contact->first_fielder = logFielderBobble(guard);
                }

                break;
            case (EVENT_STATE::MONITOR_RUNNERS):
                if (!PowerPC::MMU::HostRead_U8(guard, aAB_PitchThrown) && !PowerPC::MMU::HostRead_U8(guard, aAB_PickoffAttempt)){
                    m_game_info.getCurrentEvent().dead_ball_reason = PowerPC::MMU::HostRead_U8(guard, aAB_DeadBallReason);
                    m_game_info.getCurrentEvent().result_of_atbat = PowerPC::MMU::HostRead_U8(guard, aAB_FinalResult);
                    m_event_state = EVENT_STATE::PLAY_OVER;
                }
                else {
                    //Continue polling for fielder possession and bobbles until the ball is collected.
                    //Guard against pickoff events which reach MONITOR_RUNNERS without a pitch or contact.
                    if (m_game_info.getCurrentEvent().pitch.has_value() && m_game_info.getCurrentEvent().pitch->contact.has_value()){
                        //Keep polling the stadium hazards until the play is over
                        logHazardEvents(guard, &m_game_info.getCurrentEvent().pitch->contact.value());
                        if (!m_game_info.getCurrentEvent().pitch->contact->collect_fielder.has_value()){
                            if (!m_game_info.getCurrentEvent().pitch->contact->first_fielder.has_value())
                                m_game_info.getCurrentEvent().pitch->contact->first_fielder = logFielderBobble(guard);
                            m_game_info.getCurrentEvent().pitch->contact->collect_fielder = logFielderWithBall(guard);
                        }
                    }
                    logRunnerEvents(guard, &m_game_info.getCurrentEvent().runner_batter.value());
                    if (m_game_info.getCurrentEvent().runner_1) {
                        logRunnerEvents(guard, &m_game_info.getCurrentEvent().runner_1.value());
                    }
                    if (m_game_info.getCurrentEvent().runner_2) {
                        logRunnerEvents(guard, &m_game_info.getCurrentEvent().runner_2.value());
                    }
                    if (m_game_info.getCurrentEvent().runner_3) {
                        logRunnerEvents(guard, &m_game_info.getCurrentEvent().runner_3.value());
                    }
                }
                break;
            case (EVENT_STATE::PLAY_OVER):
                if (!PowerPC::MMU::HostRead_U8(guard, aAB_PitchThrown)){
                    m_game_info.getCurrentEvent().rbi = PowerPC::MMU::HostRead_U8(guard, aAB_RBI);

                    //runner_batter out, contact_secondary
                    logFinalResults(guard, m_game_info.getCurrentEvent());

                    //Determine if this was pitch was a DB
                    if (m_game_info.getCurrentEvent().pitch.has_value() && m_game_info.getCurrentEvent().pitch->potential_db){
                        m_game_info.getCurrentEvent().pitch->db = 1;
                        std::cout << "Logging DB!\n";
                    }

                    // Clear result of AB for pickoffs
                    if (m_game_info.getCurrentEvent().pick_off_attempt) {
                        m_game_info.getCurrentEvent().result_of_atbat = 0;
                    }

                    m_event_state = EVENT_STATE::FINAL_RESULT;
                    std::cout << "Play over\n";
                }
                break;
            case (EVENT_STATE::FINAL_RESULT):
                //Log post event HUD to file
                if (m_game_info.getCurrentEvent().write_hud_ab.second){

                    //Fill in current state for HUD
                    logGameInfo(guard);

                    if (m_game_info.post_ongoing_game == true) {
                        m_game_info.post_ongoing_game = false;
                        postOngoingGame(m_game_info.getCurrentEvent());
                    }

                    //Store current state as previous state
                    m_game_info.previous_state = m_game_info.getCurrentEvent();

                    if (!NetPlay::NetPlay_IsDesyncDetected())
                    {
                        // decoded version
                        std::string hud_file_path = File::GetUserPath(D_HUDFILES_IDX) + "decoded.hud.json";
                        std::string json = getHUDJSON(std::to_string(m_game_info.event_num) + "b", m_game_info.getCurrentEvent(), m_game_info.previous_state, true);
                        File::Delete(hud_file_path);
                        File::WriteStringToFile(hud_file_path, json);

                        // encoded version
                        hud_file_path = File::GetUserPath(D_HUDFILES_IDX) + "hud.json";
                        json = getHUDJSON(std::to_string(m_game_info.event_num) + "b", m_game_info.getCurrentEvent(), m_game_info.previous_state, false);
                        File::Delete(hud_file_path);
                        File::WriteStringToFile(hud_file_path, json);
                    }

                    //No longer need to write HUD B
                    m_game_info.getCurrentEvent().write_hud_ab.second = false;
                }

                // === Transitions ===

                if (PowerPC::MMU::HostRead_U8(guard, aGameControlStateCurr) == 0x7){
                    //Increment event count
                    ++m_game_info.event_num;
                    //Save position as prev position
                    u8 fielding_team_id = (m_game_info.previous_state.value().half_inning == 1) ? 0 : 1;
                    m_fielder_tracker[fielding_team_id].incrementBattersForPosition();
                    m_fielder_tracker[fielding_team_id].newBatter();
                    m_event_state = EVENT_STATE::INIT_EVENT;
                    m_game_info.update_ongoing_game = true;
                    std::cout << "Logging Final Result\n" << "Starting next AB\n\n";
                }
                else if (PowerPC::MMU::HostRead_U8(guard, aGameControlStateCurr) == 0x1 && !m_game_info.previous_state.value().pitch.has_value()){
                    //Increment event count
                    ++m_game_info.event_num;
                    m_event_state = EVENT_STATE::INIT_EVENT;
                    std::cout << "Logging Final Result\n" << "Pickoff over\n\n";
                }
                else if ((PowerPC::MMU::HostRead_U8(guard, aGameControlStateCurr) == 0xE) || (PowerPC::MMU::HostRead_U8(guard, aEndOfGameFlag) == 1)){ //MVP screen
                    //Increment event count
                    m_event_state = EVENT_STATE::GAME_OVER;
                    std::cout << "Logging Final Result\n" << "Game Over\n\n";
                }
                else if ((m_game_info.getCurrentEvent().outs + m_game_info.getCurrentEvent().num_outs_during_play.get_value() < 3) &&
                         m_game_info.getCurrentEvent().result_of_atbat == 0) {
                    ++m_game_info.event_num;
                    m_event_state = EVENT_STATE::INIT_EVENT;
                    std::cout << "Logging Final Result\n" << "Starting next pitch of AB\n\n";
                }
                break;
            case (EVENT_STATE::GAME_OVER):
                std::cout << "Game Over. Waiting for next game\n";
                break;
            case (EVENT_STATE::UNDEFINED):
                std::cout << "UNDEFINED STATE\n";
                m_event_state = EVENT_STATE::INIT_EVENT;
                break;                
            default:
                std::cout << "Unknown Event State\n";
                m_event_state = EVENT_STATE::INIT_EVENT;
                break;
        }
    }

    //Game State Machine
    switch (m_game_state){ // crashed here in debugging "Access violation reading location 0xFFFFFFFFFFFFFFFF"
        case (GAME_STATE::PREGAME):
            //Start recording when GameId is set AND record button is pressed AND game has started
            //std::cout << std::hex << "GameId=" << PowerPC::MMU::HostRead_U32(guard, aGameId) << "GameState=" <<  PowerPC::MMU::HostRead_U8(aGameControlStateCurr) << '\n';
            if ((PowerPC::MMU::HostRead_U32(guard, aGameId) != 0) && (PowerPC::MMU::HostRead_U8(guard, aGameControlStateCurr) == 0x5) ) {
                m_game_info.game_id = PowerPC::MMU::HostRead_U32(guard, aGameId);
                //Sample settings

                // Capture fast-reset-from-HUD at game start (the HUD gecko code is already applied
                // at boot, so this flag is authoritative by now), then immediately clear the global.
                // isLoadingFromHUD is a one-shot intent for the game being booted; clearing it here
                // prevents the value leaking into the next game played in the same session, which
                // previously left "Loaded from HUD": 1 on subsequent normal games.
                m_game_info.fastResetFromHUD = Gecko::isLoadingFromHUD;
                Gecko::setFastResetFromHUD(false);

                m_game_info.netplay = m_state.m_netplay_session;
                m_game_info.netplay_opponent_alias = m_state.m_netplay_opponent_alias;
                m_game_info.tag_set_id =
                    m_state.m_netplay_session ? m_state.tag_set_id_netplay : m_state.tag_set_id_local;


                m_game_state = GAME_STATE::INGAME;

                std::string tag_set_id_str = "-1";
                if (m_game_info.tag_set_id.has_value()){
                    tag_set_id_str = std::to_string(m_game_info.tag_set_id.value());
                }
                std::cout << "PREGAME->INGAME (GameID=" << std::to_string(m_game_info.game_id) << ", TagSetID=" << tag_set_id_str <<")\n";
                std::cout << "                (Netplay=" << m_game_info.netplay << ")\n";
            }
            break;
        case (GAME_STATE::INGAME):
            if (m_event_state == EVENT_STATE::GAME_OVER){
                logGameInfo(guard);
                std::cout << "Logging Character Stats\n";

                std::string jsonPath = getStatJsonPath("decoded.");
                std::string json = getStatJSON(true);
                    
                File::WriteStringToFile(jsonPath, json);

                jsonPath = getStatJsonPath("");
                json = getStatJSON(false, true);
                //TODO: See if user has signed up for beta test features in future
                File::WriteStringToFile(jsonPath, json);

                //File::WriteStringToFile(jsonPath, json);
                //https://api.projectrio.app/populate_db

                //Print server warning message
                OSD::AddTypedMessage(OSD::MessageType::GameStateInfo, fmt::format(
                    "Submitting game to server \n",
                    "DO NOT LEAVE THE GAME OR CLOSE RIO"           
                ), 500, OSD::Color::RED);
                
                json = getStatJSON(false, false);
                if (shouldSubmitGame()) {
                    const Common::HttpRequest::Response response =
                    m_http.Post("https://api.projectrio.app/populate_db/", json,
                        {
                            {"Content-Type", "application/json"},
                        }
                    );

                    // Print server warning message
                    OSD::AddTypedMessage(OSD::MessageType::GameStateInfo,
                                         fmt::format("Done submitting game \n", "SAFE TO QUIT"),
                                         5000, OSD::Color::GREEN);
                }

                std::cout << "Logging to " << jsonPath << "\n";
                std::cout << "INGAME->ENDGAME\n";


                m_game_state = GAME_STATE::ENDGAME_LOGGED;
            }
            break;
        case (GAME_STATE::ENDGAME_LOGGED):
            init();

            std::cout << "ENDGAME->PREGAME\n";
            break;
        case (GAME_STATE::UNDEFINED):
            std::cout << "UNDEFINED GAME STATE\n";
            m_event_state = EVENT_STATE::INIT_EVENT;
            break;
        default:
            std::cout << "Unknown Game State\n";
            m_event_state = EVENT_STATE::INIT_EVENT;
            break;        
    }
}

void StatTracker::logGameInfo(const Core::CPUThreadGuard& guard){

    std::time_t unix_time = std::time(nullptr);

    m_game_info.end_unix_date_time = std::to_string(unix_time);
    m_game_info.end_local_date_time = std::asctime(std::localtime(&unix_time));
    m_game_info.end_local_date_time.pop_back();

    m_game_info.stadium = PowerPC::MMU::HostRead_U8(guard, aStadiumId);

    m_game_info.innings_selected = PowerPC::MMU::HostRead_U8(guard, aInningsSelected);
    m_game_info.innings_played = PowerPC::MMU::HostRead_U8(guard, aAB_Inning);

    ////Captains
    //if (m_game_info.away_port == m_game_info.team0_port){
    //    m_game_info.away_captain = PowerPC::MMU::HostRead_U8(aTeam0_Captain);
    //    m_game_info.home_captain = PowerPC::MMU::HostRead_U8(aTeam1_Captain);
    //}
    //else{
    //    m_game_info.away_captain = PowerPC::MMU::HostRead_U8(aTeam1_Captain);
    //    m_game_info.home_captain = PowerPC::MMU::HostRead_U8(aTeam0_Captain);
    //}

    m_game_info.away_score = PowerPC::MMU::HostRead_U16(guard, aAwayTeam_Score);
    m_game_info.home_score = PowerPC::MMU::HostRead_U16(guard, aHomeTeam_Score);

    for (int team=0; team < cNumOfTeams; ++team){
        for (int roster=0; roster < cRosterSize; ++roster){
            logDefensiveStats(guard, team, roster);
            logOffensiveStats(guard, team, roster);
        }
    }
}

void StatTracker::logDefensiveStats(const Core::CPUThreadGuard& guard, int in_team_id, int roster_id)
{
    u32 offset = (in_team_id * cRosterSize * c_defensive_stat_offset) + (roster_id * c_defensive_stat_offset);

    u32 ingame_attribute_table_offset = (in_team_id * cRosterSize * c_roster_table_offset) + (roster_id * c_roster_table_offset);
    u32 is_starred_offset = (in_team_id * cRosterSize) + roster_id;

    u8 team_id_port = (in_team_id == 0) ? m_game_info.team0_port : m_game_info.team1_port;
    u8 idx = (team_id_port == m_game_info.home_port);
    
    auto& stat = m_game_info.character_summaries[idx][roster_id].end_game_defensive_stats;

    m_game_info.character_summaries[idx][roster_id].is_starred = PowerPC::MMU::HostRead_U8(guard, aPitcher_IsStarred + is_starred_offset);

    stat.batters_faced       = PowerPC::MMU::HostRead_U8(guard, aPitcher_BattersFaced + offset);
    stat.runs_allowed        = PowerPC::MMU::HostRead_U16(guard, aPitcher_RunsAllowed + offset);
    stat.earned_runs         = PowerPC::MMU::HostRead_U16(guard, aPitcher_RunsAllowed + offset);
    stat.batters_walked      = PowerPC::MMU::HostRead_U16(guard, aPitcher_BattersWalked + offset);
    stat.batters_hit         = PowerPC::MMU::HostRead_U16(guard, aPitcher_BattersHit + offset);
    stat.hits_allowed        = PowerPC::MMU::HostRead_U16(guard, aPitcher_HitsAllowed + offset);
    stat.homeruns_allowed    = PowerPC::MMU::HostRead_U16(guard, aPitcher_HRsAllowed + offset);
    stat.pitches_thrown      = PowerPC::MMU::HostRead_U16(guard, aPitcher_PitchesThrown + offset);
    stat.stamina             = PowerPC::MMU::HostRead_U16(guard, aPitcher_Stamina + offset);
    stat.was_pitcher         = PowerPC::MMU::HostRead_U8(guard, aPitcher_WasPitcher + offset);
    stat.batter_outs         = PowerPC::MMU::HostRead_U8(guard, aPitcher_BatterOuts + offset);
    stat.outs_pitched        = PowerPC::MMU::HostRead_U8(guard, aPitcher_OutsPitched + offset);
    stat.strike_outs         = PowerPC::MMU::HostRead_U8(guard, aPitcher_StrikeOuts + offset);
    stat.star_pitches_thrown = PowerPC::MMU::HostRead_U8(guard, aPitcher_StarPitchesThrown + offset);

    //Get inherent values. Doesn't strictly belong here but we need the adjusted_team_id
    m_game_info.character_summaries[idx][roster_id].char_id = PowerPC::MMU::HostRead_U8(guard, aInGame_CharAttributes_CharId + ingame_attribute_table_offset);
    m_game_info.character_summaries[idx][roster_id].fielding_hand = PowerPC::MMU::HostRead_U8(guard, aInGame_CharAttributes_FieldingHand + ingame_attribute_table_offset);
    m_game_info.character_summaries[idx][roster_id].batting_hand = PowerPC::MMU::HostRead_U8(guard, aInGame_CharAttributes_BattingHand + ingame_attribute_table_offset);

}

void StatTracker::logOffensiveStats(const Core::CPUThreadGuard& guard, int in_team_id, int roster_id){
    u32 offset = ((in_team_id * cRosterSize * c_offensive_stat_offset)) + (roster_id * c_offensive_stat_offset);

    u8 team_id_port = (in_team_id == 0) ? m_game_info.team0_port : m_game_info.team1_port;
    u8 idx = (team_id_port == m_game_info.home_port);

    auto& stat = m_game_info.character_summaries[idx][roster_id].end_game_offensive_stats;

    stat.at_bats          = PowerPC::MMU::HostRead_U8(guard, aBatter_AtBats + offset);
    stat.hits             = PowerPC::MMU::HostRead_U8(guard, aBatter_Hits + offset);
    stat.singles          = PowerPC::MMU::HostRead_U8(guard, aBatter_Singles + offset);
    stat.doubles          = PowerPC::MMU::HostRead_U8(guard, aBatter_Doubles + offset);
    stat.triples          = PowerPC::MMU::HostRead_U8(guard, aBatter_Triples + offset);
    stat.homeruns         = PowerPC::MMU::HostRead_U8(guard, aBatter_Homeruns + offset);
    stat.successful_bunts = PowerPC::MMU::HostRead_U8(guard, aBatter_BuntSuccess + offset);
    stat.sac_flys         = PowerPC::MMU::HostRead_U8(guard, aBatter_SacFlys + offset);
    stat.strikouts        = PowerPC::MMU::HostRead_U8(guard, aBatter_Strikeouts + offset);
    stat.walks_4balls     = PowerPC::MMU::HostRead_U8(guard, aBatter_Walks_4Balls + offset);
    stat.walks_hit        = PowerPC::MMU::HostRead_U8(guard, aBatter_Walks_Hit + offset);
    stat.rbi              = PowerPC::MMU::HostRead_U8(guard, aBatter_RBI + offset);
    stat.bases_stolen     = PowerPC::MMU::HostRead_U8(guard, aBatter_BasesStolen + offset);
    stat.star_hits        = PowerPC::MMU::HostRead_U8(guard, aBatter_StarHits + offset);

    m_game_info.character_summaries[idx][roster_id].end_game_defensive_stats.big_plays = PowerPC::MMU::HostRead_U8(guard, aBatter_BigPlays + offset);
}

void StatTracker::logEventState(const Core::CPUThreadGuard& guard, Event& in_event){
    in_event.inning          = PowerPC::MMU::HostRead_U8(guard, aAB_Inning);
    in_event.half_inning     = PowerPC::MMU::HostRead_U8(guard, aAB_HalfInning);

    //Figure out scores
    in_event.away_score = PowerPC::MMU::HostRead_U16(guard, aAwayTeam_Score);
    in_event.home_score = PowerPC::MMU::HostRead_U16(guard, aHomeTeam_Score);

    in_event.balls           = PowerPC::MMU::HostRead_U8(guard, aAB_Balls);
    in_event.strikes         = PowerPC::MMU::HostRead_U8(guard, aAB_Strikes);
    in_event.outs            = PowerPC::MMU::HostRead_U8(guard, aAB_Outs);
    
    //Figure out star ownership
    if (m_game_info.team0_port == m_game_info.away_port){
        in_event.away_stars = PowerPC::MMU::HostRead_U8(guard, aAB_P1_Stars);
        in_event.home_stars = PowerPC::MMU::HostRead_U8(guard, aAB_P2_Stars);
    }
    else {
        in_event.away_stars = PowerPC::MMU::HostRead_U8(guard, aAB_P2_Stars);
        in_event.home_stars = PowerPC::MMU::HostRead_U8(guard, aAB_P1_Stars);
    }
    
    in_event.is_star_chance  = PowerPC::MMU::HostRead_U8(guard, aAB_IsStarChance);
    in_event.chem_links_ob   = PowerPC::MMU::HostRead_U8(guard, aAB_ChemLinksOnBase);

    //The following stamina lookup requires team_id to be in teams of team0 or team1

    auto batter_fielder_ports = getBatterFielderPorts(guard);
    u8 pitching_team = (batter_fielder_ports.second == m_game_info.team1_port); //1 if the pitching team is team1
    u8 pitcher_roster_loc = PowerPC::MMU::HostRead_U8(guard, aAB_PitcherRosterID);
    
    //Calc the pitcher stamina offset and add it to the base stamina addr - TODO move to EventSummary
    u32 pitcherStaminaOffset = ((pitching_team * cRosterSize * c_defensive_stat_offset) + (pitcher_roster_loc * c_defensive_stat_offset));
    in_event.pitcher_stamina = PowerPC::MMU::HostRead_U16(guard, aPitcher_Stamina + pitcherStaminaOffset);

    in_event.pitcher_roster_loc = PowerPC::MMU::HostRead_U8(guard, aAB_PitcherRosterID);
    in_event.batter_roster_loc  = PowerPC::MMU::HostRead_U8(guard, aAB_BatterRosterID);
    in_event.catcher_roster_loc = PowerPC::MMU::HostRead_U8(guard, aFielder_RosterLoc + (1 * cFielder_Offset));

    // Track both teams' current batter positions so the fielding team's is preserved across half-innings
    in_event.away_batter_roster_loc = static_cast<u8>(PowerPC::MMU::HostRead_U32(guard, aAB_AwayBatter)) - 1;
    in_event.home_batter_roster_loc = static_cast<u8>(PowerPC::MMU::HostRead_U32(guard, aAB_HomeBatter)) - 1;

    // Read per-inning scores for each team up to the current inning
    // Memory layout: current score (u16) followed by 18 inning scores (u16 each)
    for (u8 i = 0; i < in_event.inning && i < 18; ++i) {
        in_event.away_inning_scores[i] = PowerPC::MMU::HostRead_U16(guard, aAwayTeam_Score + ((i + 1) * 2));
        in_event.home_inning_scores[i] = PowerPC::MMU::HostRead_U16(guard, aHomeTeam_Score + ((i + 1) * 2));
    }
}

void StatTracker::logContact(const Core::CPUThreadGuard& guard, Event& in_event){
    std::cout << "Logging Contact\n";

    Pitch* pitch = &in_event.pitch.value();
    //Create contact object to populate and get a ptr to it
    pitch->contact = std::make_optional(Contact());
    resetHazardTracking();
    std::cout << "  Pitch Type: " << std::to_string(in_event.pitch->pitch_type) << "\n";
    Contact* contact = &in_event.pitch->contact.value();

    contact->power.read_value(guard);
    contact->vert_angle.read_value(guard);
    contact->horiz_angle.read_value(guard);
    contact->ball_x_velo.read_value(guard);
    contact->ball_y_velo.read_value(guard);
    contact->ball_z_velo.read_value(guard);
    contact->ball_contact_x_pos.read_value(guard);
    contact->ball_contact_z_pos.read_value(guard);
    contact->contact_absolute.read_value(guard);
    contact->contact_quality.read_value(guard);
    contact->rng1.read_value(guard);
    contact->rng2.read_value(guard);
    contact->rng3.read_value(guard);
    contact->type_of_contact.read_value(guard);
    contact->moon_shot.read_value(guard);
    contact->charge_power_up.read_value(guard);
    contact->charge_power_down.read_value(guard);
    contact->input_direction_push_pull.read_value(guard);
    contact->frame_of_swing.read_value(guard);

    //More ball flight info
    contact->ball_max_height.read_value(guard);
    contact->ball_hang_time.read_value(guard);

    u32 aStickInput = aAB_ControlStickInput + (getBatterFielderPorts(guard).first * cControl_Offset);
    //std::cout << "Batter Port=" << std::to_string(getBatterFielderPorts().first) << " Stick Addr=" << std::hex << aStickInput << " Stick Value=" << (PowerPC::MMU::HostRead_U16(guard, aStickInput) & 0xF) << "\n";
    contact->input_direction_stick.set_value(PowerPC::MMU::HostRead_U16(guard, aStickInput) & 0xF); //Mask off the lower 4 bits which are the control stick directions
    //std::cout << "  Stick Value Decoded=" << decode("StickVec", contact->input_direction_stick.get_value(), true) << "\n";
    std::cout << "SWING: " << contact->frame_of_swing.get_key_value_string().first << "=" << contact->frame_of_swing.get_key_value_string().second << "\n";
    std::cout << "\n";
}

void StatTracker::logPitch(const Core::CPUThreadGuard& guard, Event& in_event){
    std::cout << "Logging Pitching\n";

    in_event.pitch->logged = true;
    in_event.pitch->pitcher_team_id    = !in_event.half_inning;
    in_event.pitch->pitcher_char_id    = PowerPC::MMU::HostRead_U8(guard, aAB_PitcherID);
    in_event.pitch->pitch_type         = PowerPC::MMU::HostRead_U8(guard, aAB_PitchType);
    in_event.pitch->charge_type        = PowerPC::MMU::HostRead_U8(guard, aAB_ChargePitchType);
    in_event.pitch->star_pitch         = ((PowerPC::MMU::HostRead_U8(guard, aAB_StarPitch_NonCaptain) > 0) || (PowerPC::MMU::HostRead_U8(guard, aAB_StarPitch_Captain) > 0));
    in_event.pitch->pitch_speed        = PowerPC::MMU::HostRead_U8(guard, aAB_PitchSpeed);

    in_event.pitch->ball_z_strike_vs_ball = PowerPC::MMU::HostRead_U32(guard, aAB_PitchBallPosZStrikezone);
    in_event.pitch->bat_contact_x_pos.read_value(guard);
    in_event.pitch->bat_contact_z_pos.read_value(guard);

    float ballposz_strikezone = floatConverter(in_event.pitch->ball_z_strike_vs_ball);
    float strikezone_left = floatConverter(PowerPC::MMU::HostRead_U32(guard, aAB_PitchStrikezoneEdgeLeft));
    float strikezone_right = floatConverter(PowerPC::MMU::HostRead_U32(guard, aAB_PitchStrikezoneEdgeRight));
    in_event.pitch->ball_in_strikezone = (strikezone_left < ballposz_strikezone && ballposz_strikezone < strikezone_right) ? 1 : 0;
    
    // === Batter info ===

    //First slap,charge,star,bunt
    u8 swing_type = PowerPC::MMU::HostRead_U8(guard, aAB_TypeOfSwing);  // 0=Slap, 1=charge, 3=bunt
    u8 star_swing = PowerPC::MMU::HostRead_U8(guard, aAB_StarSwing);
    u8 adjusted_swing = 0; //0=miss, 1=slap, 2=charge, 3=star, 4=bunt
    //Adjust swing to definition
    if (star_swing != 0 && hasEnoughStarsForStarSwing(guard, in_event)){
        adjusted_swing = 3;
    }
    else {
        adjusted_swing = swing_type + 1;
    }

    //Use adjusted swing if swing and miss, else 0 (or 4 for bunt)
    u8 any_swing = PowerPC::MMU::HostRead_U8(guard, aAB_AnySwing);  // 0=No swing, 1=swing
    if (any_swing == 0) {
        in_event.pitch->type_of_swing = 0;
    }
    else if (any_swing >= 1){
        in_event.pitch->type_of_swing = adjusted_swing;
    }

    std::cout << "SWING: Swing Type=" << std::to_string(swing_type) << " Star Swing=" << std::to_string(star_swing) 
              << " AnySwing=" << std::to_string(PowerPC::MMU::HostRead_U8(guard, aAB_AnySwing)) << " Final=" << std::to_string(in_event.pitch->type_of_swing) << "\n";
}

void StatTracker::logContactResult(const Core::CPUThreadGuard& guard, Contact* in_contact){
    std::cout << "Logging Contact Result\n";

    u8 result = PowerPC::MMU::HostRead_U8(guard, aAB_ContactResult);

    //Log primary contact result (and secondary if possible)
    if (result == 1 || result == 2){
        in_contact->primary_contact_result = result+1; //Landed Fair
        in_contact->ball_x_pos.read_value(guard);
        in_contact->ball_y_pos.read_value(guard);
        in_contact->ball_z_pos.read_value(guard);

        //If 2, ball has been caught. Log this as final fielder. If ball has been bobbled they will be logged as bobble
        in_contact->collect_fielder = logFielderWithBall(guard);
    }
    else if (result == 3){
        in_contact->primary_contact_result = 0; //Out (secondary=caught)
        in_contact->secondary_contact_result = 0; //Out (secondary=caught)

        //If the ball is caught, use the balls position from the frame before to avoid the ball_pos
        //from matching the fielders
        in_contact->ball_x_pos.set_value_to_prev();
        in_contact->ball_y_pos.set_value_to_prev();
        in_contact->ball_z_pos.set_value_to_prev();

        //Ball has been caught. Log this as final fielder. If ball has been bobbled they will be logged as bobble
        in_contact->collect_fielder = logFielderWithBall(guard);

        //Increment outs for that position for fielder
        m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].incrementOutForPosition(in_contact->collect_fielder->fielder_roster_loc, in_contact->collect_fielder->fielder_pos);
        //Indicate if fielder had been swapped for this batterid = 
        //fielding team is !half_inning
        in_contact->collect_fielder->fielder_swapped_for_batter = m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].wasFielderSwappedForBatter(in_contact->collect_fielder->fielder_roster_loc);

        std::cout << "Was fielder swapped. Team_id=" << std::to_string(!m_game_info.getCurrentEvent().half_inning) 
                  << " Fielder Roster=" << std::to_string(in_contact->collect_fielder->fielder_roster_loc)
                  << " Swapped=" << std::to_string(in_contact->collect_fielder->fielder_swapped_for_batter) << "\n";
    }
    else if (result == 0xFF){ // Known bug: this will be true for foul or HR. Correct when adjusting secondary contact later
        in_contact->primary_contact_result = 1; //Foul
        in_contact->secondary_contact_result = 3; //Foul
        in_contact->ball_x_pos.read_value(guard);
        in_contact->ball_y_pos.read_value(guard);
        in_contact->ball_z_pos.read_value(guard);
    }
    else{
        in_contact->primary_contact_result = result;
        in_contact->secondary_contact_result = 0xFF; //???
        in_contact->ball_x_pos.read_value(guard);
        in_contact->ball_y_pos.read_value(guard);
        in_contact->ball_z_pos.read_value(guard);
    }
}

void StatTracker::logFinalResults(const Core::CPUThreadGuard& guard, Event& in_event){

    //Indicate strikeout in the runner_batter
    if (in_event.result_of_atbat == 1){
        //0x10 in runner_batter denotes strikeout
        in_event.runner_batter->out_type = 0x10;
    }

    if (in_event.pitch.has_value() && in_event.pitch->contact.has_value()){
        Contact* contact = &in_event.pitch->contact.value();
        //Fill in secondary result for contact
        if (in_event.result_of_atbat >= 0x7 && in_event.result_of_atbat <= 0xF){
            //0x10 in runner_batter denotes strikeout
            contact->secondary_contact_result = in_event.result_of_atbat;
            contact->primary_contact_result = 2; // Correct if set to foul
        }
        else if ((in_event.runner_batter->out_type == 2) || (in_event.runner_batter->out_type == 3)){
            contact->secondary_contact_result = in_event.runner_batter->out_type;
        }
        else if ((in_event.runner_batter->out_type == 0) && (in_event.result_of_atbat == 4)){
            contact->secondary_contact_result = in_event.result_of_atbat;
        }
    }

    //num_outs_during_play
    auto num_outs = in_event.num_outs_during_play.read_value(guard);
    std::cout << "Num outs for play=" << std::to_string(num_outs) << "\n";
    m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].incrementBatterOutForPosition(num_outs);

    //If the runner got out at first (forced), increment outs for the fielder who first touched the ball
    if (in_event.runner_batter->out_type == 2) {
        //First fielder to touch the ball
        Fielder* fielder;

        //If the fielder bobbled but the same fielder collected the ball OR there was no bobble, log single fielde
        if (in_event.pitch->contact->first_fielder.has_value()) { fielder = &in_event.pitch->contact->first_fielder.value(); }
        else {fielder = &in_event.pitch->contact->collect_fielder.value();}

        m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].incrementOutForPosition(fielder->fielder_roster_loc, fielder->fielder_pos);
    }
}

std::string StatTracker::getStatJsonPath(std::string prefix){
    std::string away_player_name;
    std::string home_player_name;
    if (m_game_info.away_port == m_game_info.team0_port) {
        away_player_name = m_game_info.team0_player.GetUsername();
        home_player_name = m_game_info.team1_player.GetUsername();
    }
    else{
        away_player_name = m_game_info.team1_player.GetUsername();
        home_player_name = m_game_info.team0_player.GetUsername();
    }

    std::time_t unix_time = std::time(nullptr);
    char datetime_c[256];
    std::strftime(datetime_c, sizeof(datetime_c), "%Y%m%dT%H%M%S", std::localtime(&unix_time));
    
    std::string file_name = prefix + datetime_c + "_" + away_player_name 
                   + "-Vs-" + home_player_name
                   + "_" + std::to_string(m_game_info.game_id) + ".json";

    std::string full_file_path = File::GetUserPath(D_MSSBFILES_IDX) + file_name;

    return full_file_path;
}

std::string StatTracker::getStatJSON(bool inDecode, bool hide_riokey){
    //TODO switch to IDs when submitting game
    std::string away_player_info = (inDecode || hide_riokey) ? m_game_info.getAwayTeamPlayer().GetUsername() : m_game_info.getAwayTeamPlayer().GetUserID();
    std::string home_player_info = (inDecode || hide_riokey) ? m_game_info.getHomeTeamPlayer().GetUsername() : m_game_info.getHomeTeamPlayer().GetUserID();

    std::stringstream json_stream;

    json_stream << "{\n";
    std::string stadium = (inDecode) ? "\"" + cStadiumIdToStadiumName.at(m_game_info.stadium) + "\"" : std::to_string(m_game_info.stadium);
    std::string start_date_time = (inDecode) ? m_game_info.start_local_date_time : m_game_info.start_unix_date_time;
    std::string end_date_time = (inDecode) ? m_game_info.end_local_date_time : m_game_info.end_unix_date_time;
    json_stream << "  \"GameID\": \"" << m_game_info.game_id << "\",\n";
    json_stream << "  \"Date - Start\": \"" << start_date_time << "\",\n";
    json_stream << "  \"Date - End\": \"" << end_date_time << "\",\n";
    
    std::string tag_set_id_str = "-1";
    if (m_game_info.tag_set_id.has_value()){
        tag_set_id_str = std::to_string(m_game_info.tag_set_id.value());
    }
    json_stream << "  \"TagSetID\": " << tag_set_id_str << ",\n";
    json_stream << "  \"Netplay\": " << std::to_string(m_game_info.netplay) << ",\n";
    json_stream << "  \"Loaded from HUD\": " << std::to_string(m_game_info.fastResetFromHUD) << ",\n";
    json_stream << "  \"StadiumID\": " << decode("Stadium", m_game_info.stadium, inDecode) << ",\n";
    json_stream << "  \"Away Player\": \"" << away_player_info << "\",\n"; //TODO MAKE THIS AN ID
    json_stream << "  \"Home Player\": \"" << home_player_info << "\",\n";

    json_stream << "  \"Away Score\": " << std::dec << m_game_info.away_score << ",\n";
    json_stream << "  \"Home Score\": " << std::dec << m_game_info.home_score << ",\n";

    json_stream << "  \"Innings Selected\": " << std::to_string(m_game_info.innings_selected) << ",\n";
    json_stream << "  \"Innings Played\": " << std::to_string(m_game_info.innings_played) << ",\n";
    json_stream << "  \"Quitter Team\": " << decode("QuitterTeam", m_game_info.quitter_team, inDecode) << ",\n";

    json_stream << "  \"Average Ping\": " << std::to_string(m_game_info.avg_ping) << ",\n";
    json_stream << "  \"Lag Spikes\": " << std::to_string(m_game_info.lag_spikes) << ",\n";
    json_stream << "  \"Version\": \"" << Common::GetRioRevStr() << "\",\n";

    json_stream << "  \"Character Game Stats\": {\n";

    //Defensive Stats
    for (int team=0; team < cNumOfTeams; ++team){
        u8 captain_roster_loc;
        if (team == 0){
            captain_roster_loc = (m_game_info.away_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;
        }
        else{ // team == 1
            captain_roster_loc = (m_game_info.home_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;
        }

        std::string team_string = (team == 0) ? "Away" : "Home";

        for (int roster=0; roster < cRosterSize; ++roster){
            CharacterSummary& char_summary = m_game_info.character_summaries[team][roster];
            
            // team integer home or away
            std::string label = "\"" + team_string + " Roster " + std::to_string(roster) + "\": ";
            json_stream << "    " << label << "{\n";
            json_stream << "      \"Team\": \""        << std::to_string(team) << "\",\n";
            json_stream << "      \"RosterID\": "      << std::to_string(roster) << ",\n";
            json_stream << "      \"CharID\": "        << decode("Character", char_summary.char_id, inDecode) << ",\n";
            json_stream << "      \"Superstar\": "     << std::to_string(char_summary.is_starred) << ",\n";
            json_stream << "      \"Captain\": "       << std::to_string(roster == captain_roster_loc) << ",\n";
            json_stream << "      \"Fielding Hand\": " << decode("Hand", char_summary.fielding_hand, inDecode) << ",\n";
            json_stream << "      \"Batting Hand\": "  << decode("Hand", char_summary.batting_hand, inDecode) << ",\n";

            //=== Defensive Stats ===
            EndGameRosterDefensiveStats& def_stat = char_summary.end_game_defensive_stats;
            json_stream << "      \"Defensive Stats\": {\n";
            json_stream << "        \"Batters Faced\": "       << std::to_string(def_stat.batters_faced) << ",\n";
            json_stream << "        \"Runs Allowed\": "        << std::dec << def_stat.runs_allowed << ",\n";
            json_stream << "        \"Earned Runs\": "        << std::dec << def_stat.earned_runs << ",\n";
            json_stream << "        \"Batters Walked\": "      << def_stat.batters_walked << ",\n";
            json_stream << "        \"Batters Hit\": "         << def_stat.batters_hit << ",\n";
            json_stream << "        \"Hits Allowed\": "        << def_stat.hits_allowed << ",\n";
            json_stream << "        \"HRs Allowed\": "         << def_stat.homeruns_allowed << ",\n";
            json_stream << "        \"Pitches Thrown\": "      << def_stat.pitches_thrown << ",\n";
            json_stream << "        \"Stamina\": "             << def_stat.stamina << ",\n";
            json_stream << "        \"Was Pitcher\": "         << std::to_string(def_stat.was_pitcher) << ",\n";
            json_stream << "        \"Strikeouts\": "          << std::to_string(def_stat.strike_outs) << ",\n";
            json_stream << "        \"Star Pitches Thrown\": " << std::to_string(def_stat.star_pitches_thrown) << ",\n";
            json_stream << "        \"Big Plays\": "           << std::to_string(def_stat.big_plays) << ",\n";
            json_stream << "        \"Outs Pitched\": "        << std::to_string(def_stat.outs_pitched) << ",\n";
            json_stream << "        \"Batters Per Position\": [\n";

            if (m_fielder_tracker[team].battersAtAnyPosition(roster, 0)){
                json_stream << "          {\n";
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[team].fielder_map[roster].batter_count_by_position[pos] > 0){
                        std::string comma = (m_fielder_tracker[team].battersAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[team].fielder_map[roster].batter_count_by_position[pos]) << comma << "\n";
                    }
                }
                json_stream << "          }\n";
            }
            json_stream << "        ],\n";

            json_stream << "        \"Batter Outs Per Position\": [\n";
            if (m_fielder_tracker[team].batterOutsAtAnyPosition(roster, 0)){
                json_stream << "          {\n";
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[team].fielder_map[roster].batter_outs_by_position[pos] > 0){
                        std::string comma = (m_fielder_tracker[team].batterOutsAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[team].fielder_map[roster].batter_outs_by_position[pos]) << comma << "\n";
                    }
                }
                json_stream << "          }\n";
            }
            json_stream << "        ],\n";

            json_stream << "        \"Outs Per Position\": [\n";
            if (m_fielder_tracker[team].outsAtAnyPosition(roster, 0)){
                json_stream << "          {\n";
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[team].fielder_map[roster].out_count_by_position[pos] > 0){
                        std::string comma = (m_fielder_tracker[team].outsAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[team].fielder_map[roster].out_count_by_position[pos]) << comma << "\n";
                    }
                }
                json_stream << "          }\n";
            }
            json_stream << "        ]\n";
            json_stream << "      },\n";

            //=== Offensive Stats ===
            EndGameRosterOffensiveStats& of_stat = char_summary.end_game_offensive_stats;
            json_stream << "      \"Offensive Stats\": {\n";
            json_stream << "        \"At Bats\": "          << std::to_string(of_stat.at_bats) << ",\n";
            json_stream << "        \"Hits\": "             << std::to_string(of_stat.hits) << ",\n";
            json_stream << "        \"Singles\": "          << std::to_string(of_stat.singles) << ",\n";
            json_stream << "        \"Doubles\": "          << std::to_string(of_stat.doubles) << ",\n";
            json_stream << "        \"Triples\": "          << std::to_string(of_stat.triples) << ",\n";
            json_stream << "        \"Homeruns\": "         << std::to_string(of_stat.homeruns) << ",\n";
            json_stream << "        \"Successful Bunts\": " << std::to_string(of_stat.successful_bunts) << ",\n";
            json_stream << "        \"Sac Flys\": "         << std::to_string(of_stat.sac_flys) << ",\n";
            json_stream << "        \"Strikeouts\": "       << std::to_string(of_stat.strikouts) << ",\n";
            json_stream << "        \"Walks (4 Balls)\": "  << std::to_string(of_stat.walks_4balls) << ",\n";
            json_stream << "        \"Walks (Hit)\": "      << std::to_string(of_stat.walks_hit) << ",\n";
            json_stream << "        \"RBI\": "              << std::to_string(of_stat.rbi) << ",\n";
            json_stream << "        \"Bases Stolen\": "     << std::to_string(of_stat.bases_stolen) << ",\n";
            json_stream << "        \"Star Hits\": "        << std::to_string(of_stat.star_hits) << "\n";
            json_stream << "      }\n";
            std::string commas = ((roster == 8) && (team ==1)) ? "" : ",";
            json_stream << "    }" << commas << "\n";
        }
    }
    json_stream << "  },\n";
    //=== Events === 
    json_stream << "  \"Events\": [\n";
    for (auto event_map_iter = m_game_info.events.begin(); event_map_iter != m_game_info.events.end(); event_map_iter++) {
        u8 event_num = event_map_iter->first;
        Event& event = event_map_iter->second;

        //Don't log events with inning == 0. Means game has crashed/quit and this is an empty event
        if (event.inning == 0) {
            continue;
        }

        json_stream << "    {\n";
        json_stream << "      \"Event Num\": "               << std::to_string(event_num) << ",\n";
        json_stream << "      \"Inning\": "                  << std::to_string(event.inning) << ",\n";
        json_stream << "      \"Half Inning\": "             << std::to_string(event.half_inning) << ",\n";
        json_stream << "      \"Away Score\": "              << std::dec << event.away_score << ",\n";
        json_stream << "      \"Home Score\": "              << std::dec << event.home_score << ",\n";
        json_stream << "      \"Balls\": "                   << std::to_string(event.balls) << ",\n";
        json_stream << "      \"Strikes\": "                 << std::to_string(event.strikes) << ",\n";
        json_stream << "      \"Outs\": "                    << std::to_string(event.outs) << ",\n";
        json_stream << "      \"Star Chance\": "             << std::to_string(event.is_star_chance) << ",\n";
        json_stream << "      \"Away Stars\": "              << std::to_string(event.away_stars) << ",\n";
        json_stream << "      \"Home Stars\": "              << std::to_string(event.home_stars) << ",\n";
        json_stream << "      \"Pitcher Stamina\": "         << std::to_string(event.pitcher_stamina) << ",\n";
        json_stream << "      \"Chemistry Links on Base\": " << std::to_string(event.chem_links_ob) << ",\n";
        json_stream << "      \"Pitcher Roster Loc\": "      << std::to_string(event.pitcher_roster_loc) << ",\n";
        json_stream << "      \"Batter Roster Loc\": "       << std::to_string(event.batter_roster_loc) << ",\n";
        json_stream << "      \"Catcher Roster Loc\": "       << std::to_string(event.catcher_roster_loc) << ",\n";
        json_stream << "      \"RBI\": "                     << std::to_string(event.rbi) << ",\n";
        json_stream << "      \"" << event.num_outs_during_play.name << "\": " << event.num_outs_during_play.get_key_value_string().second << ",\n";
        json_stream << "      \"Dead Ball Reason\": "        << decode("DeadBallReason", event.dead_ball_reason, inDecode) << ",\n";
        json_stream << "      \"Result of AB\": "            << decode("AtBatResult", event.result_of_atbat, inDecode) << ",\n";

        //=== Runners ===
        //Build vector of <Runner*, Label/Name>
        std::vector<std::pair<Runner*, std::string>> runners;
        if (event.runner_batter) {
            runners.push_back({&event.runner_batter.value(), "Batter"});
        }
        if (event.runner_1) {
            runners.push_back({&event.runner_1.value(), "1B"});
        }
        if (event.runner_2) {
            runners.push_back({&event.runner_2.value(), "2B"});
        }
        if (event.runner_3) {
            runners.push_back({&event.runner_3.value(), "3B"});
        }

        for (auto runner = runners.begin(); runner != runners.end(); runner++){
            Runner* runner_info = runner->first;
            std::string& label = runner->second;

            json_stream << "      \"Runner " << label << "\": {\n";
            json_stream << "        \"Runner Roster Loc\": "   << std::to_string(runner_info->roster_loc) << ",\n";
            json_stream << "        \"Runner Char Id\": "      << decode("Character", runner_info->char_id, inDecode) << ",\n";
            json_stream << "        \"Runner Initial Base\": " << std::to_string(runner_info->initial_base) << ",\n";
            json_stream << "        \"Out Type\": "            << decode("Out", runner_info->out_type, inDecode) << ",\n";
            json_stream << "        \"Out Location\": "        << std::to_string(runner_info->out_location) << ",\n";
            //json_stream << "        \"Runner Basepath Location\": "  << std::to_string(runner_info->basepath_location) << ",\n";
            json_stream << "        \"Steal\": "               << decode("Steal", runner_info->steal, inDecode) << ",\n";
            json_stream << "        \"Runner Result Base\": "  << std::to_string(runner_info->result_base) << "\n";
            std::string comma = (std::next(runner) == runners.end() && !event.pitch.has_value()) ? "" : ",";
            json_stream << "      }" << comma << "\n";
        }


        //=== Pitch ===
        if (event.pitch.has_value()){
            Pitch* pitch = &event.pitch.value();
            json_stream << "      \"Pitch\": {\n";
            json_stream << "        \"Pitcher Team Id\": "    << std::to_string(pitch->pitcher_team_id) << ",\n";
            json_stream << "        \"Pitcher Char Id\": "    << decode("Character", pitch->pitcher_char_id, inDecode) << ",\n";
            json_stream << "        \"Pitch Type\": "         << decode("Pitch", pitch->pitch_type, inDecode) << ",\n";
            json_stream << "        \"Charge Type\": "        << decode("ChargePitch", pitch->charge_type, inDecode) << ",\n";
            json_stream << "        \"Star Pitch\": "         << std::to_string(pitch->star_pitch) << ",\n";
            json_stream << "        \"Pitch Speed\": "        << std::to_string(pitch->pitch_speed) << ",\n";
            json_stream << "        \"Ball Position - Strikezone\": "   << floatConverter(pitch->ball_z_strike_vs_ball) << ",\n";
            json_stream << "        \"In Strikezone\": "      << std::to_string(pitch->ball_in_strikezone) << ",\n";
            json_stream << "        \"" << pitch->bat_contact_x_pos.name << "\": " << floatConverter(pitch->bat_contact_x_pos.get_value()) << ",\n";
            json_stream << "        \"" << pitch->bat_contact_z_pos.name << "\": " << floatConverter(pitch->bat_contact_z_pos.get_value()) << ",\n";
            json_stream << "        \"DB\": "                 << std::to_string(pitch->db) << ",\n";
            json_stream << "        \"Type of Swing\": "      << decode("Swing", pitch->type_of_swing, inDecode);
            
            //=== Contact ===
            if (pitch->contact.has_value() && pitch->contact->type_of_contact.get_value() != 0xFF){
                json_stream << ",\n";

                Contact* contact = &pitch->contact.value();
                json_stream << "        \"Contact\": {\n";
                json_stream << "          \"" << contact->type_of_contact.name << "\":" << decode("Contact", contact->type_of_contact.get_value(), inDecode) << ",\n";
                json_stream << "          \"" << contact->charge_power_up.name << "\": " << floatConverter(contact->charge_power_up.get_value()) << ",\n";
                json_stream << "          \"" << contact->charge_power_down.name << "\": " << floatConverter(contact->charge_power_down.get_value()) << ",\n";
                json_stream << "          \"" << contact->moon_shot.name << "\": " << contact->moon_shot.get_key_value_string().second << ",\n"; 
                json_stream << "          \"" << contact->input_direction_push_pull.name << "\": " << decode("Stick", contact->input_direction_push_pull.get_value(), inDecode) << ",\n";
                json_stream << "          \"" << contact->input_direction_stick.name << "\": " << decode("StickVec", contact->input_direction_stick.get_value(), inDecode) << ",\n";
                json_stream << "          \"" << contact->frame_of_swing.name << "\": \"" << std::dec << contact->frame_of_swing.get_value() << "\",\n";

                json_stream << "          \"" << contact->power.name << "\": \"" << std::dec << contact->power.get_value() <<"\",\n";
                json_stream << "          \"" << contact->vert_angle.name << "\": \"" << std::dec << contact->vert_angle.get_value() << "\",\n";
                json_stream << "          \"" << contact->horiz_angle.name << "\": \"" << std::dec << contact->horiz_angle.get_value() << "\",\n";

                json_stream << "          \"" << contact->contact_absolute.name << "\": " << floatConverter(contact->contact_absolute.get_value()) << ",\n";
                json_stream << "          \"" << contact->contact_quality.name << "\": " << floatConverter(contact->contact_quality.get_value()) << ",\n";
                
                json_stream << "          \"" << contact->rng1.name << "\": \"" << std::dec << contact->rng1.get_value() << "\",\n";
                json_stream << "          \"" << contact->rng2.name << "\": \"" << std::dec << contact->rng2.get_value() << "\",\n";
                json_stream << "          \"" << contact->rng3.name << "\": \"" << std::dec << contact->rng3.get_value() << "\",\n";

                json_stream << "          \"" << contact->ball_x_velo.name << "\": " << floatConverter(contact->ball_x_velo.get_value()) << ",\n";
                json_stream << "          \"" << contact->ball_y_velo.name << "\": " << floatConverter(contact->ball_y_velo.get_value()) << ",\n";
                json_stream << "          \"" << contact->ball_z_velo.name << "\": " << floatConverter(contact->ball_z_velo.get_value()) << ",\n";

                json_stream << "          \"" << contact->ball_contact_x_pos.name << "\": " << floatConverter(contact->ball_contact_x_pos.get_value()) << ",\n";
                json_stream << "          \"" << contact->ball_contact_z_pos.name << "\": " << floatConverter(contact->ball_contact_z_pos.get_value()) << ",\n";
                                
                json_stream << "          \"" << contact->ball_x_pos.name << "\": " << floatConverter(contact->ball_x_pos.get_value()) << ",\n";
                json_stream << "          \"" << contact->ball_y_pos.name << "\": " << floatConverter(contact->ball_y_pos.get_value()) << ",\n";
                json_stream << "          \"" << contact->ball_z_pos.name << "\": " << floatConverter(contact->ball_z_pos.get_value()) << ",\n";

                json_stream << "          \"" << contact->ball_max_height.name << "\": " << floatConverter(contact->ball_max_height.get_value()) << ",\n";
                json_stream << "          \"" << contact->ball_hang_time.name << "\": \"" << std::dec << contact->ball_hang_time.get_value() << "\",\n";
                json_stream << "          \"Contact Result - Primary\": "         << decode("PrimaryContactResult", contact->primary_contact_result, inDecode) << ",\n";
                json_stream << "          \"Contact Result - Secondary\": "       << decode("SecondaryContactResult", contact->secondary_contact_result, inDecode);

                //=== Hazard Events ===
                if (!contact->hazard_events.empty()){
                    json_stream << ",\n";
                    json_stream << getHazardEventsJSON(contact->hazard_events, "          ", inDecode);
                }

                //=== Fielder ===
                //TODO could be reworked
                if (contact->first_fielder.has_value() || contact->collect_fielder.has_value()){
                    json_stream << ",\n";

                    //First fielder to touch the ball
                    Fielder* fielder;

                    //If the fielder bobbled but the same fielder collected the ball OR there was no bobble, log single fielder
                    
                    if (contact->first_fielder.has_value()) { fielder = &contact->first_fielder.value(); }
                    else {fielder = &contact->collect_fielder.value();}

                    json_stream << "          \"First Fielder\": {\n";
                    json_stream << "            \"Fielder Roster Location\": " << std::to_string(fielder->fielder_roster_loc) << ",\n";
                    json_stream << "            \"Fielder Position\": "        << decode("Position", fielder->fielder_pos, inDecode) << ",\n";
                    json_stream << "            \"Fielder Character\": "       << decode("Character", fielder->fielder_char_id, inDecode) << ",\n";
                    json_stream << "            \"Fielder Action\": "          << decode("Action", fielder->fielder_action, inDecode) << ",\n";
                    json_stream << "            \"Fielder Jump\": "            << std::to_string(fielder->fielder_jump) << ",\n";
                    json_stream << "            \"Fielder Swap\": "            << std::to_string(fielder->fielder_swapped_for_batter) << ",\n";
                    json_stream << "            \"Fielder Manual Selected\": " << decode("ManualSelect", fielder->fielder_manual_select_arg, inDecode) << ",\n";
                    json_stream << "            \"Fielder Position - X\": "    << floatConverter(fielder->fielder_x_pos) << ",\n";
                    json_stream << "            \"Fielder Position - Y\": "    << floatConverter(fielder->fielder_y_pos) << ",\n";
                    json_stream << "            \"Fielder Position - Z\": "    << floatConverter(fielder->fielder_z_pos) << ",\n";
                    json_stream << "            \"Fielder Bobble\": "          << decode("Bobble", fielder->bobble, inDecode) << "\n";
                    json_stream << "          }\n";
                    /*
                    else if (contact->first_fielder.has_value() 
                            && (contact->first_fielder->fielder_roster_loc != contact->collect_fielder->fielder_roster_loc)) {
                        
                        Fielder* first_fielder  = &contact->first_fielder.value();
                        Fielder* second_fielder = &contact->collect_fielder.value();

                        json_stream << "          \"First Fielder\": {\n";
                        json_stream << "            \"Fielder Roster Location\": " << std::to_string(first_fielder->fielder_roster_loc) << ",\n";
                        json_stream << "            \"Fielder Position\": "        << std::to_string(first_fielder->fielder_pos) << ",\n";
                        json_stream << "            \"Fielder Character\": "       << std::to_string(first_fielder->fielder_char_id) << ",\n";
                        json_stream << "            \"Fielder Action\": "          << std::to_string(first_fielder->fielder_action) << ",\n";
                        json_stream << "            \"Fielder Swap\": "            << std::to_string(first_fielder->fielder_swapped_for_batter) << ",\n";
                        json_stream << "            \"Fielder Position - X\": "    << floatConverter(first_fielder->fielder_x_pos) << ",\n";
                        json_stream << "            \"Fielder Position - Y\": "    << floatConverter(first_fielder->fielder_y_pos) << ",\n";
                        json_stream << "            \"Fielder Position - Z\": "    << floatConverter(first_fielder->fielder_z_pos) << ",\n";
                        json_stream << "            \"Fielder Bobble\": "          << std::to_string(first_fielder->bobble) << "\n";
                        json_stream << "          },\n";
                        json_stream << "          \"Second Fielder\": {\n";
                        json_stream << "            \"Fielder Roster Location\": " << std::to_string(second_fielder->fielder_roster_loc) << ",\n";
                        json_stream << "            \"Fielder Position\": "        << std::to_string(second_fielder->fielder_pos) << ",\n";
                        json_stream << "            \"Fielder Character\": "       << std::to_string(second_fielder->fielder_char_id) << ",\n";
                        json_stream << "            \"Fielder Action\": "          << std::to_string(second_fielder->fielder_action) << ",\n";
                        json_stream << "            \"Fielder Swap\": "            << std::to_string(second_fielder->fielder_swapped_for_batter) << ",\n";
                        json_stream << "            \"Fielder Position - X\": "    << floatConverter(second_fielder->fielder_x_pos) << ",\n";
                        json_stream << "            \"Fielder Position - Y\": "    << floatConverter(second_fielder->fielder_y_pos) << ",\n";
                        json_stream << "            \"Fielder Position - Z\": "    << floatConverter(second_fielder->fielder_z_pos) << ",\n";
                        json_stream << "            \"Fielder Bobble\": "          << std::to_string(second_fielder->bobble) << "\n";
                        json_stream << "          }\n";
                    }
                    */
                }
                else{ //Finish contact section
                    json_stream << "\n";
                }
                json_stream << "        }\n";
            }
            else { //Finish pitch section
                json_stream << "\n";
            }
            json_stream << "      }\n";
        }

        std::string end_comma = (std::next(event_map_iter) !=  m_game_info.events.end()) ? "," : "";
        json_stream << "    }" << end_comma << "\n";
    }

    json_stream << "  ]\n";
    json_stream << "}\n";

    return json_stream.str();
}

std::string StatTracker::getHUDJSON(std::string in_event_num, Event& in_curr_event, std::optional<Event> in_prev_event, bool inDecode){
    std::stringstream json_stream;

    if (in_curr_event.inning == 0) {
        return "{}";
    }

    json_stream << "{\n";

    json_stream << "  \"GameID\": \"" << m_game_info.game_id << "\",\n";
    std::string tag_set_id_str = "-1";
    if (m_game_info.tag_set_id.has_value()){
        tag_set_id_str = std::to_string(m_game_info.tag_set_id.value());
    }
    json_stream << "  \"TagSetID\": " << tag_set_id_str << ",\n";
    json_stream << "  \"Loaded from HUD\": " << std::to_string(m_game_info.fastResetFromHUD) << ",\n";
    json_stream << "  \"StadiumID\": " << decode("Stadium", m_game_info.stadium, inDecode) << ",\n";
    json_stream << "  \"Innings Selected\": " << std::to_string(m_game_info.innings_selected) << ",\n";
    json_stream << "  \"First Batting Team\": " << std::to_string(m_game_info.first_batting_team) << ",\n";
    json_stream << "  \"Star Skills On\": "      << std::to_string(m_game_info.star_skills_on) << ",\n";
    json_stream << "  \"Mercy On\": "            << std::to_string(m_game_info.mercy_on) << ",\n";
    json_stream << "  \"Away Logo\": "         << decode("Logo", m_game_info.away_logo, inDecode) << ",\n";
    json_stream << "  \"Home Logo\": "         << decode("Logo", m_game_info.home_logo, inDecode) << ",\n";
    json_stream << "  \"Event Num\": \""             << in_event_num << "\",\n";
    json_stream << "  \"Away Player\": \""           << m_game_info.getAwayTeamPlayer().GetUsername() << "\",\n";
    json_stream << "  \"Home Player\": \""           << m_game_info.getHomeTeamPlayer().GetUsername() << "\",\n";
    json_stream << "  \"Away Port\": "               << std::to_string(m_game_info.away_port) << ",\n";
    json_stream << "  \"Home Port\": "               << std::to_string(m_game_info.home_port) << ",\n";
    json_stream << "  \"Inning\": "                  << std::to_string(in_curr_event.inning) << ",\n";
    json_stream << "  \"Half Inning\": "             << std::to_string(in_curr_event.half_inning) << ",\n";
    json_stream << "  \"Away Score\": "              << std::dec << in_curr_event.away_score << ",\n";
    json_stream << "  \"Home Score\": "              << std::dec << in_curr_event.home_score << ",\n";

    json_stream << "  \"Away Inning Scores\": [";
    for (u8 i = 0; i < in_curr_event.inning && i < 18; ++i) {
        json_stream << in_curr_event.away_inning_scores[i];
        if (i < in_curr_event.inning - 1) json_stream << ", ";
    }
    json_stream << "],\n";

    json_stream << "  \"Home Inning Scores\": [";
    for (u8 i = 0; i < in_curr_event.inning && i < 18; ++i) {
        json_stream << in_curr_event.home_inning_scores[i];
        if (i < in_curr_event.inning - 1) json_stream << ", ";
    }
    json_stream << "],\n";

    json_stream << "  \"Balls\": "                   << std::to_string(in_curr_event.balls) << ",\n";
    json_stream << "  \"Strikes\": "                 << std::to_string(in_curr_event.strikes) << ",\n";
    json_stream << "  \"Outs\": "                    << std::to_string(in_curr_event.outs) << ",\n";
    json_stream << "  \"Star Chance\": "             << std::to_string(in_curr_event.is_star_chance) << ",\n";
    json_stream << "  \"Away Stars\": "              << std::to_string(in_curr_event.away_stars) << ",\n";
    json_stream << "  \"Home Stars\": "              << std::to_string(in_curr_event.home_stars) << ",\n";
    json_stream << "  \"Pitcher Stamina\": "         << std::to_string(in_curr_event.pitcher_stamina) << ",\n";
    json_stream << "  \"Chemistry Links on Base\": " << std::to_string(in_curr_event.chem_links_ob) << ",\n";
    json_stream << "  \"" << in_curr_event.num_outs_during_play.name << "\": " << in_curr_event.num_outs_during_play.get_key_value_string().second << ",\n";
    json_stream << "  \"Pitcher Roster Loc\": "        << std::to_string(in_curr_event.pitcher_roster_loc) << ",\n";
    json_stream << "  \"Batter Roster Loc\": "         << std::to_string(in_curr_event.batter_roster_loc) << ",\n";
    json_stream << "  \"Away Batter Roster Loc\": "    << std::to_string(in_curr_event.away_batter_roster_loc) << ",\n";
    json_stream << "  \"Home Batter Roster Loc\": "    << std::to_string(in_curr_event.home_batter_roster_loc) << ",\n";

    for (int team=0; team < 2; ++team){
        for (int roster=0; roster < cRosterSize; ++roster){

            u8 captain_roster_loc = 0;
            if (team == 0){
                captain_roster_loc = (m_game_info.away_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;
            }
            else{ // team == 1
                captain_roster_loc = (m_game_info.home_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;
            }

            std::string team_string = (team == 0) ? "Away" : "Home";

            CharacterSummary& char_summary = m_game_info.character_summaries[team][roster];
            std::string label = "\"" + team_string + " Roster " + std::to_string(roster) + "\": ";
            json_stream << "  " << label << "{\n";
            json_stream << "    \"Team\": \""        << std::to_string(team) << "\",\n";
            json_stream << "    \"RosterID\": "      << std::to_string(roster) << ",\n";
            json_stream << "    \"CharID\": "        << decode("Character", char_summary.char_id, inDecode) << ",\n";
            json_stream << "    \"Superstar\": "     << std::to_string(char_summary.is_starred) << ",\n";
            json_stream << "    \"Captain\": "       << std::to_string(roster == captain_roster_loc) << ",\n";
            json_stream << "    \"Fielding Hand\": " << decode("Hand", char_summary.fielding_hand, inDecode) << ",\n";
            json_stream << "    \"Batting Hand\": "  << decode("Hand", char_summary.batting_hand, inDecode) << ",\n";
            json_stream << "    \"Fielding Position\": " << decode("Position", m_fielder_tracker[team].fielder_map[roster].current_pos, inDecode) << ",\n";

            //=== Defensive Stats ===
            EndGameRosterDefensiveStats& def_stat = char_summary.end_game_defensive_stats;
            json_stream << "    \"Defensive Stats\": {\n";
            json_stream << "      \"Batters Faced\": "       << std::to_string(def_stat.batters_faced) << ",\n";
            json_stream << "      \"Runs Allowed\": "        << std::dec << def_stat.runs_allowed << ",\n";
            json_stream << "      \"Earned Runs\": "        << std::dec << def_stat.earned_runs << ",\n";
            json_stream << "      \"Batters Walked\": "      << def_stat.batters_walked << ",\n";
            json_stream << "      \"Batters Hit\": "         << def_stat.batters_hit << ",\n";
            json_stream << "      \"Hits Allowed\": "        << def_stat.hits_allowed << ",\n";
            json_stream << "      \"HRs Allowed\": "         << def_stat.homeruns_allowed << ",\n";
            json_stream << "      \"Pitches Thrown\": "      << def_stat.pitches_thrown << ",\n";
            json_stream << "      \"Stamina\": "             << def_stat.stamina << ",\n";
            json_stream << "      \"Was Pitcher\": "         << std::to_string(def_stat.was_pitcher) << ",\n";
            json_stream << "      \"Strikeouts\": "          << std::to_string(def_stat.strike_outs) << ",\n";
            json_stream << "      \"Star Pitches Thrown\": " << std::to_string(def_stat.star_pitches_thrown) << ",\n";
            json_stream << "      \"Big Plays\": "           << std::to_string(def_stat.big_plays) << ",\n";
            json_stream << "      \"Outs Pitched\": "        << std::to_string(def_stat.outs_pitched) << ",\n";
            json_stream << "      \"Batters Per Position\": [\n";

            if (m_fielder_tracker[team].battersAtAnyPosition(roster, 0)){
                json_stream << "        {\n";
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[team].fielder_map[roster].batter_count_by_position[pos] > 0){
                        std::string comma = (m_fielder_tracker[team].battersAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[team].fielder_map[roster].batter_count_by_position[pos]) << comma << "\n";
                    }
                }
                json_stream << "        }\n";
            }
            json_stream << "      ],\n";

            json_stream << "      \"Batter Outs Per Position\": [\n";
            if (m_fielder_tracker[team].batterOutsAtAnyPosition(roster, 0)){
                json_stream << "        {\n";
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[team].fielder_map[roster].batter_outs_by_position[pos] > 0){
                        std::string comma = (m_fielder_tracker[team].batterOutsAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[team].fielder_map[roster].batter_outs_by_position[pos]) << comma << "\n";
                    }
                }
                json_stream << "        }\n";
            }
            json_stream << "      ],\n";

            json_stream << "      \"Outs Per Position\": [\n";
            if (m_fielder_tracker[team].outsAtAnyPosition(roster, 0)){
                json_stream << "        {\n";
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[team].fielder_map[roster].out_count_by_position[pos] > 0){
                        std::string comma = (m_fielder_tracker[team].outsAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[team].fielder_map[roster].out_count_by_position[pos]) << comma << "\n";
                    }
                }
                json_stream << "        }\n";
            }
            json_stream << "      ]\n";
            json_stream << "    },\n";

            //=== Offensive Stats ===
            EndGameRosterOffensiveStats& of_stat = char_summary.end_game_offensive_stats;
            json_stream << "    \"Offensive Stats\": {\n";
            json_stream << "      \"At Bats\": "          << std::to_string(of_stat.at_bats) << ",\n";
            json_stream << "      \"Hits\": "             << std::to_string(of_stat.hits) << ",\n";
            json_stream << "      \"Singles\": "          << std::to_string(of_stat.singles) << ",\n";
            json_stream << "      \"Doubles\": "          << std::to_string(of_stat.doubles) << ",\n";
            json_stream << "      \"Triples\": "          << std::to_string(of_stat.triples) << ",\n";
            json_stream << "      \"Homeruns\": "         << std::to_string(of_stat.homeruns) << ",\n";
            json_stream << "      \"Successful Bunts\": " << std::to_string(of_stat.successful_bunts) << ",\n";
            json_stream << "      \"Sac Flys\": "         << std::to_string(of_stat.sac_flys) << ",\n";
            json_stream << "      \"Strikeouts\": "       << std::to_string(of_stat.strikouts) << ",\n";
            json_stream << "      \"Walks (4 Balls)\": "  << std::to_string(of_stat.walks_4balls) << ",\n";
            json_stream << "      \"Walks (Hit)\": "      << std::to_string(of_stat.walks_hit) << ",\n";
            json_stream << "      \"RBI\": "              << std::to_string(of_stat.rbi) << ",\n";
            json_stream << "      \"Bases Stolen\": "     << std::to_string(of_stat.bases_stolen) << ",\n";
            json_stream << "      \"Star Hits\": "        << std::to_string(of_stat.star_hits) << "\n";
            json_stream << "    }\n";
            json_stream << "  },\n";
        }
    }

    //=== Runners ===
    //Build vector of <Runner*, Label/Name>
    std::vector<std::pair<Runner*, std::string>> runners;
    if (in_curr_event.runner_batter) {
        runners.push_back({&in_curr_event.runner_batter.value(), "Batter"});
    }
    if (in_curr_event.runner_1) {
        runners.push_back({&in_curr_event.runner_1.value(), "1B"});
    }
    if (in_curr_event.runner_2) {
        runners.push_back({&in_curr_event.runner_2.value(), "2B"});
    }
    if (in_curr_event.runner_3) {
        runners.push_back({&in_curr_event.runner_3.value(), "3B"});
    }

    for (auto runner = runners.begin(); runner != runners.end(); runner++){
        Runner* runner_info = runner->first;
        std::string& label = runner->second;

        json_stream << "  \"Runner " << label << "\": {\n";
        json_stream << "    \"Runner Roster Loc\": "   << std::to_string(runner_info->roster_loc) << ",\n";
        json_stream << "    \"Runner Char Id\": "      << decode("Character", runner_info->char_id, inDecode) << ",\n";
        json_stream << "    \"Runner Initial Base\": " << std::to_string(runner_info->initial_base) << ",\n";
        json_stream << "    \"Out Type\": "            << decode("Out", runner_info->out_type, inDecode) << ",\n";
        json_stream << "    \"Out Location\": "        << std::to_string(runner_info->out_location) << ",\n";
        //json_stream << "    \"Runner Basepath Location\": "  << std::to_string(runner_info->basepath_location) << ",\n";
        json_stream << "    \"Steal\": "               << decode("Steal", runner_info->steal, inDecode) << ",\n";
        json_stream << "    \"Runner Result Base\": "  << std::to_string(runner_info->result_base) << "\n";
        std::string comma = (std::next(runner) == runners.end() && !in_prev_event.has_value() ) ? "" : ",";
        json_stream << "  }" << comma << "\n";
    }

    //Previous Event - return if first event of game. Else write the event
    if (!in_prev_event.has_value()){
        json_stream << "}";
        return json_stream.str();
    }

    //=== Pitch ===

    json_stream << "  \"Previous Event\": {\n";
    json_stream << "    \"RBI\": "                     << std::to_string(in_prev_event->rbi) << ",\n";
    json_stream << "    \"Dead Ball Reason\": "        << decode("DeadBallReason", in_prev_event->dead_ball_reason, inDecode) << ",\n";
    std::string comma = (in_prev_event->pitch.has_value()) ? "," : "";
    json_stream << "    \"Result of AB\": "            << decode("AtBatResult", in_prev_event->result_of_atbat, inDecode) << comma << "\n";
    if (in_prev_event->pitch.has_value()){
        Pitch* pitch = &in_prev_event->pitch.value();
        json_stream << "    \"Pitch\": {\n";
        json_stream << "      \"Pitcher Team Id\": "    << std::to_string(pitch->pitcher_team_id) << ",\n";
        json_stream << "      \"Pitcher Char Id\": "    << decode("Character", pitch->pitcher_char_id, inDecode) << ",\n";
        json_stream << "      \"Pitch Type\": "         << decode("Pitch", pitch->pitch_type, inDecode) << ",\n";
        json_stream << "      \"Charge Type\": "        << decode("ChargePitch", pitch->charge_type, inDecode) << ",\n";
        json_stream << "      \"Star Pitch\": "         << std::to_string(pitch->star_pitch) << ",\n";
        json_stream << "      \"Pitch Speed\": "        << std::to_string(pitch->pitch_speed) << ",\n";
        json_stream << "      \"Ball Position - Strikezone\": "   << floatConverter(pitch->ball_z_strike_vs_ball) << ",\n";
        json_stream << "      \"In Strikezone\": "      << std::to_string(pitch->ball_in_strikezone) << ",\n";
        json_stream << "        \"" << pitch->bat_contact_x_pos.name << "\": " << floatConverter(pitch->bat_contact_x_pos.get_value()) << ",\n";
        json_stream << "        \"" << pitch->bat_contact_z_pos.name << "\": " << floatConverter(pitch->bat_contact_z_pos.get_value()) << ",\n";
        json_stream << "      \"DB\": "                 << std::to_string(pitch->db) << ",\n";
        json_stream << "      \"Type of Swing\": "      << decode("Swing", pitch->type_of_swing, inDecode);
        
        //=== Contact ===
        if (pitch->contact.has_value() && pitch->contact->type_of_contact.get_value() != 0xFF){
            json_stream << ",\n";

            Contact* contact = &pitch->contact.value();
            json_stream << "      \"Contact\": {\n";
            json_stream << "        \"" << contact->type_of_contact.name << "\":" << decode("Contact", contact->type_of_contact.get_value(), inDecode) << ",\n";
            json_stream << "        \"" << contact->charge_power_up.name << "\": " << floatConverter(contact->charge_power_up.get_value()) << ",\n";
            json_stream << "        \"" << contact->charge_power_down.name << "\": " << floatConverter(contact->charge_power_down.get_value()) << ",\n";
            json_stream << "        \"" << contact->moon_shot.name << "\": " << contact->moon_shot.get_key_value_string().second << ",\n"; 
            json_stream << "        \"" << contact->input_direction_push_pull.name << "\": " << decode("Stick", contact->input_direction_push_pull.get_value(), inDecode) << ",\n";
            json_stream << "        \"" << contact->input_direction_stick.name << "\": " << decode("StickVec", contact->input_direction_stick.get_value(), inDecode) << ",\n";
            json_stream << "        \"" << contact->frame_of_swing.name << "\": \"" << std::dec << contact->frame_of_swing.get_value() << "\",\n";
            json_stream << "        \"" << contact->power.name << "\": \"" << std::dec << contact->power.get_value() <<"\",\n";
            json_stream << "        \"" << contact->vert_angle.name << "\": \"" << std::dec << contact->vert_angle.get_value() << "\",\n";
            json_stream << "        \"" << contact->horiz_angle.name << "\": \"" << std::dec << contact->horiz_angle.get_value() << "\",\n";
            json_stream << "        \"" << contact->contact_absolute.name << "\": " << floatConverter(contact->contact_absolute.get_value()) << ",\n";
            json_stream << "        \"" << contact->contact_quality.name << "\": " << floatConverter(contact->contact_quality.get_value()) << ",\n";
            json_stream << "        \"" << contact->rng1.name << "\": \"" << std::dec << contact->rng1.get_value() << "\",\n";
            json_stream << "        \"" << contact->rng2.name << "\": \"" << std::dec << contact->rng2.get_value() << "\",\n";
            json_stream << "        \"" << contact->rng3.name << "\": \"" << std::dec << contact->rng3.get_value() << "\",\n";
            json_stream << "        \"" << contact->ball_x_velo.name << "\": " << floatConverter(contact->ball_x_velo.get_value()) << ",\n";
            json_stream << "        \"" << contact->ball_y_velo.name << "\": " << floatConverter(contact->ball_y_velo.get_value()) << ",\n";
            json_stream << "        \"" << contact->ball_z_velo.name << "\": " << floatConverter(contact->ball_z_velo.get_value()) << ",\n";
            json_stream << "        \"" << contact->ball_contact_x_pos.name << "\": " << floatConverter(contact->ball_contact_x_pos.get_value()) << ",\n";
            json_stream << "        \"" << contact->ball_contact_z_pos.name << "\": " << floatConverter(contact->ball_contact_z_pos.get_value()) << ",\n";
            json_stream << "        \"" << contact->ball_x_pos.name << "\": " << floatConverter(contact->ball_x_pos.get_value()) << ",\n";
            json_stream << "        \"" << contact->ball_y_pos.name << "\": " << floatConverter(contact->ball_y_pos.get_value()) << ",\n";
            json_stream << "        \"" << contact->ball_z_pos.name << "\": " << floatConverter(contact->ball_z_pos.get_value()) << ",\n";
            json_stream << "        \"" << contact->ball_hang_time.name << "\": " << std::dec << contact->ball_hang_time.get_value() << ",\n";
            json_stream << "        \"" << contact->ball_max_height.name << "\": " << floatConverter(contact->ball_max_height.get_value()) << ",\n";
            json_stream << "        \"Contact Result - Primary\": "         << decode("PrimaryContactResult", contact->primary_contact_result, inDecode) << ",\n";
            json_stream << "        \"Contact Result - Secondary\": "       << decode("SecondaryContactResult", contact->secondary_contact_result, inDecode);

            //=== Hazard Events ===
            if (!contact->hazard_events.empty()){
                json_stream << ",\n";
                json_stream << getHazardEventsJSON(contact->hazard_events, "        ", inDecode);
            }

            //=== Fielder ===
            //TODO could be reworked
            if (contact->first_fielder.has_value() || contact->collect_fielder.has_value()){
                json_stream << ",\n";

                //First fielder to touch the ball
                Fielder* fielder;

                //If the fielder bobbled but the same fielder collected the ball OR there was no bobble, log single fielder
                
                if (contact->first_fielder.has_value()) { fielder = &contact->first_fielder.value(); }
                else {fielder = &contact->collect_fielder.value();}

                json_stream << "        \"First Fielder\": {\n";
                json_stream << "          \"Fielder Roster Location\": " << std::to_string(fielder->fielder_roster_loc) << ",\n";
                json_stream << "          \"Fielder Position\": "        << decode("Position", fielder->fielder_pos, inDecode) << ",\n";
                json_stream << "          \"Fielder Character\": "       << decode("Character", fielder->fielder_char_id, inDecode) << ",\n";
                json_stream << "          \"Fielder Action\": "          << decode("Action", fielder->fielder_action, inDecode) << ",\n";
                json_stream << "          \"Fielder Jump\": "            << std::to_string(fielder->fielder_jump) << ",\n";
                json_stream << "          \"Fielder Swap\": "            << std::to_string(fielder->fielder_swapped_for_batter) << ",\n";
                json_stream << "          \"Fielder Manual Selected\": " << decode("ManualSelect", fielder->fielder_manual_select_arg, inDecode) << ",\n";
                json_stream << "          \"Fielder Position - X\": "    << floatConverter(fielder->fielder_x_pos) << ",\n";
                json_stream << "          \"Fielder Position - Y\": "    << floatConverter(fielder->fielder_y_pos) << ",\n";
                json_stream << "          \"Fielder Position - Z\": "    << floatConverter(fielder->fielder_z_pos) << ",\n";
                json_stream << "          \"Fielder Bobble\": "          << decode("Bobble", fielder->bobble, inDecode) << "\n";
                json_stream << "        }\n";
            }
            else{ //Finish contact section
                json_stream << "\n";
            }
            json_stream << "      }\n"; //close contact
        }
        else { //Finish pitch section
            json_stream << "\n";
        }
        json_stream << "    }\n"; //Close pitch
    }
    json_stream << "  }\n"; //Close Previous Event
    json_stream << "}";
    return json_stream.str();
}

//Scans player for possession
std::optional<StatTracker::Fielder> StatTracker::logFielderWithBall(const Core::CPUThreadGuard& guard) {
    std::optional<Fielder> fielder;
    for (u8 pos=0; pos < cRosterSize; ++pos){
        u32 aFielderControlStatus = aFielder_ControlStatus + (pos * cFielder_Offset);
        u32 aFielderPosX = aFielder_Pos_X + (pos * cFielder_Offset);
        u32 aFielderPosY = aFielder_Pos_Y + (pos * cFielder_Offset);
        u32 aFielderPosZ = aFielder_Pos_Z + (pos * cFielder_Offset);

        u32 aFielderJump = aFielder_AnyJump + (pos * cFielder_Offset);
        u32 aFielderAction = aFielder_Action + (pos * cFielder_Offset);

        u32 aFielderRosterLoc = aFielder_RosterLoc + (pos * cFielder_Offset);
        u32 aFielderCharId = aFielder_CharId + (pos * cFielder_Offset);

        bool fielder_has_ball = (PowerPC::MMU::HostRead_U8(guard, aFielderControlStatus) == 0xA);

        if (fielder_has_ball) {
            Fielder fielder_with_ball;
            //get char id
            fielder_with_ball.fielder_roster_loc = PowerPC::MMU::HostRead_U8(guard, aFielderRosterLoc);
            fielder_with_ball.fielder_char_id = PowerPC::MMU::HostRead_U8(guard, aFielderCharId);
            fielder_with_ball.fielder_pos = pos;

            fielder_with_ball.fielder_x_pos = PowerPC::MMU::HostRead_U32(guard, aFielderPosX);
            fielder_with_ball.fielder_y_pos = PowerPC::MMU::HostRead_U32(guard, aFielderPosY);
            fielder_with_ball.fielder_z_pos = PowerPC::MMU::HostRead_U32(guard, aFielderPosZ);

            if (PowerPC::MMU::HostRead_U8(guard, aFielderAction)) {
                fielder_with_ball.fielder_action = PowerPC::MMU::HostRead_U8(guard, aFielderAction); //2 = Slide, 3 = Walljump
            }
            if (PowerPC::MMU::HostRead_U8(guard, aFielderJump)) {
                fielder_with_ball.fielder_jump = PowerPC::MMU::HostRead_U8(guard, aFielderJump); //1 = jump
            }

            fielder_with_ball.fielder_manual_select_arg = PowerPC::MMU::HostRead_U8(guard, aFielder_ManualSelectArg);

            std::cout << "Fielder Pos=" << std::to_string(pos) << " Fielder RosterLoc=" << std::to_string(fielder_with_ball.fielder_roster_loc)
                      << " Fielder Action: " << std::to_string(fielder_with_ball.fielder_action)
                      << " Manual Select=" << std::to_string(fielder_with_ball.fielder_manual_select_arg)
                      << " Jump=" << std::to_string(fielder_with_ball.fielder_jump) << "\n";

            std::cout << "Logging Fielder\n";
            fielder = std::make_optional(fielder_with_ball);
            return fielder;
        }
    }
    return std::nullopt;
}

std::optional<StatTracker::Fielder> StatTracker::logFielderBobble(const Core::CPUThreadGuard& guard) {
    std::optional<Fielder> fielder;
    for (u8 pos=0; pos < cRosterSize; ++pos){
        u32 aFielderBobbleStatus = aFielder_Bobble + (pos * cFielder_Offset);
        u32 aFielderKnockoutStatus = aFielder_Knockout + (pos * cFielder_Offset);

        u32 aFielderJump = aFielder_AnyJump + (pos * cFielder_Offset);
        u32 aFielderAction = aFielder_Action + (pos * cFielder_Offset);

        u32 aFielderPosX = aFielder_Pos_X + (pos * cFielder_Offset);
        u32 aFielderPosY = aFielder_Pos_Y + (pos * cFielder_Offset);
        u32 aFielderPosZ = aFielder_Pos_Z + (pos * cFielder_Offset);

        u32 aFielderRosterLoc = aFielder_RosterLoc + (pos * cFielder_Offset);
        u32 aFielderCharId = aFielder_CharId + (pos * cFielder_Offset);
        
        u8 typeOfFielderDisruption = 0x0;
        u8 bobble_addr = PowerPC::MMU::HostRead_U8(guard, aFielderBobbleStatus);
        u8 knockout_addr = PowerPC::MMU::HostRead_U8(guard, aFielderKnockoutStatus);

        if (knockout_addr) {
            typeOfFielderDisruption = 0x10; //Knockout - no bobble
        }
        else if (bobble_addr){
            typeOfFielderDisruption = bobble_addr; //Different types of bobbles
        }

        if (typeOfFielderDisruption > 0x1) {
            Fielder fielder_that_bobbled;
            //get char id
            fielder_that_bobbled.fielder_roster_loc = PowerPC::MMU::HostRead_U8(guard, aFielderRosterLoc);
            fielder_that_bobbled.fielder_char_id = PowerPC::MMU::HostRead_U8(guard, aFielderCharId);

            fielder_that_bobbled.fielder_x_pos = PowerPC::MMU::HostRead_U32(guard, aFielderPosX);
            fielder_that_bobbled.fielder_y_pos = PowerPC::MMU::HostRead_U32(guard, aFielderPosY);
            fielder_that_bobbled.fielder_z_pos = PowerPC::MMU::HostRead_U32(guard, aFielderPosZ);
            fielder_that_bobbled.fielder_pos = pos;
            fielder_that_bobbled.bobble = typeOfFielderDisruption;

            if (PowerPC::MMU::HostRead_U8(guard, aFielderAction)) {
                fielder_that_bobbled.fielder_action = PowerPC::MMU::HostRead_U8(guard, aFielderAction); //2 = Slide, 3 = Walljump
            }
            if (PowerPC::MMU::HostRead_U8(guard, aFielderJump)) {
                fielder_that_bobbled.fielder_jump = PowerPC::MMU::HostRead_U8(guard, aFielderJump); //1 = jump
            }

            //We can read manual select now because we don't have the ball
            fielder_that_bobbled.fielder_manual_select_arg = PowerPC::MMU::HostRead_U8(guard, aFielder_ManualSelectArg);

            std::cout << "Fielder Pos=" << std::to_string(pos) << " Fielder RosterLoc=" << std::to_string(fielder_that_bobbled.fielder_roster_loc)
                      << " Fielder Action: " << std::to_string(fielder_that_bobbled.fielder_action) 
                      << " Jump=" << std::to_string(fielder_that_bobbled.fielder_jump)
                      << " Manual Select=" << std::to_string(fielder_that_bobbled.fielder_manual_select_arg)
                      << " Bobble=" << std::to_string(fielder_that_bobbled.bobble) << "\n";
                      
            fielder = std::make_optional(fielder_that_bobbled);
            return fielder;
        }
    }
    return std::nullopt;
}

//=== Stadium hazards ===

void StatTracker::resetHazardTracking(){
    m_hazard_state = HazardTrackerState();
}

StatTracker::HazardEvent& StatTracker::addHazardEvent(Contact* in_contact, u8 hazard_type, u8 hazard_id, u8 interaction, u16 parent_sequence, u16 frame){
    HazardEvent hazard_event;
    hazard_event.sequence = static_cast<u16>(in_contact->hazard_events.size() + 1);
    hazard_event.parent_sequence = parent_sequence;
    hazard_event.hazard_type = hazard_type;
    hazard_event.hazard_id = hazard_id;
    hazard_event.interaction = interaction;
    hazard_event.frame = frame;
    in_contact->hazard_events.push_back(hazard_event);
    return in_contact->hazard_events.back();
}

//Fills in the position and fielder fields of an event from the fielder in the given position slot
void StatTracker::addFielderToHazardEvent(const Core::CPUThreadGuard& guard, HazardEvent& in_event, u8 fielder_pos){
    in_event.pos_x = PowerPC::MMU::HostRead_U32(guard, aFielder_Pos_X + (fielder_pos * cFielder_Offset));
    in_event.pos_y = PowerPC::MMU::HostRead_U32(guard, aFielder_Pos_Y + (fielder_pos * cFielder_Offset));
    in_event.pos_z = PowerPC::MMU::HostRead_U32(guard, aFielder_Pos_Z + (fielder_pos * cFielder_Offset));
    in_event.fielder_roster_loc = PowerPC::MMU::HostRead_U8(guard, aFielder_RosterLoc + (fielder_pos * cFielder_Offset));
    in_event.fielder_pos        = fielder_pos;
    in_event.fielder_char_id    = PowerPC::MMU::HostRead_U8(guard, aFielder_CharId + (fielder_pos * cFielder_Offset));
}

//Polls the stadium hazards every frame the ball is live and records an event whenever one interacts with the ball or a fielder.
//Yoshi Park: red plants eat the ball (Ball Contact), spit it back out (Projectile) and knock out fielders that touch them
//(Fielder Contact). Yellow plants bounce the ball and award the batting team a star (Ball Contact).
//Wario Palace: chomps lunge (Projectile), knock the ball away (Ball Contact) and knock out fielders (Fielder Contact).
//Tornados pull the ball in (Ball Contact) and throw it back out (Projectile). Sand stars award a star when hit (Ball Contact).
//Bowser Castle: thwomps drop (Projectile) and the ball bounces off them (Ball Contact). Fireballs burn fielders (Fielder Contact)
//and are put out by the ball (Ball Contact); logging each launch (Projectile) is off, see cHazard_LogFlameShots. Star pads award
//a star when hit (Ball Contact).
//DK Jungle: cannons fire barrels (Projectile, aimed at where the ball will land) that knock out fielders (Fielder Contact) and
//bounce the ball (Ball Contact). Klaptraps bite fielders (Fielder Contact) and are knocked off when the ball hits them (Ball Contact).
//Peach Garden: the ball hits blocks (Ball Contact). Bricks break and award a star, note blocks knock the ball back, mystery blocks
//reveal what they are.
//Every object lives in the game's stadium object array and is identified by its update function pointer.
void StatTracker::logHazardEvents(const Core::CPUThreadGuard& guard, Contact* in_contact){
    if (m_game_info.stadium != cStadiumId_YoshiPark && m_game_info.stadium != cStadiumId_WarioPalace
     && m_game_info.stadium != cStadiumId_BowserCastle && m_game_info.stadium != cStadiumId_DKJungle
     && m_game_info.stadium != cStadiumId_PeachGarden) { return; }
    //The hazards only update while the game reports a live ball (cGameControlState 0x2)
    if (PowerPC::MMU::HostRead_U8(guard, aGameControlStateCurr) != 0x2) { return; }

    u32 obj_array = PowerPC::MMU::HostRead_U32(guard, aStadiumObj_ArrayPtr);
    //The object array is heap allocated. Bail if the pointer doesn't point into MEM1
    if (obj_array < 0x80000000 || obj_array >= 0x81800000) { return; }
    u32 obj_count = PowerPC::MMU::HostRead_U32(guard, aStadiumObj_Count);
    if (obj_count > cStadiumObj_MaxCount) { obj_count = cStadiumObj_MaxCount; }

    //Plant spits and tornado releases: the game holds the ball for a few frames after release, wait for that countdown before reading the velocity
    const int cHeldVelocityMinFrames = 4;
    const int cVelocityMaxWaitFrames = 10;

    //=== Per-frame values shared by every hazard ===
    u16 frame = PowerPC::MMU::HostRead_U16(guard, aAB_FramesSinceContact);
    std::array<u32, 3> ball_pos = {PowerPC::MMU::HostRead_U32(guard, aAB_BallPos_X),
                                   PowerPC::MMU::HostRead_U32(guard, aAB_BallPos_Y),
                                   PowerPC::MMU::HostRead_U32(guard, aAB_BallPos_Z)};
    float ball_x = floatConverter(ball_pos[0]);
    float ball_y = floatConverter(ball_pos[1]);
    float ball_z = floatConverter(ball_pos[2]);
    //Hazards that knock the ball around are skipped by the game while a fielder is holding it
    u8 ball_contact_result = PowerPC::MMU::HostRead_U8(guard, aAB_ContactResult);
    bool fielder_has_ball = (ball_contact_result == cContactResult_Fielded || ball_contact_result == cContactResult_Caught);
    std::array<u8, cRosterSize> knockouts = {};
    for (u8 pos = 0; pos < cRosterSize; ++pos){
        knockouts[pos] = PowerPC::MMU::HostRead_U8(guard, aFielder_Knockout + (pos * cFielder_Offset));
    }
    s16 frames_since_bounce = static_cast<s16>(PowerPC::MMU::HostRead_U16(guard, aAB_FramesSinceLastBounce));
    u8 hold_countdown = PowerPC::MMU::HostRead_U8(guard, aAB_FrameCountdownAfterPlantSpit);
    u8 chomp_ball_marker = PowerPC::MMU::HostRead_U8(guard, aChomp_BallHitMarker);
    u8 block_hit_active = PowerPC::MMU::HostRead_U8(guard, aStadiumObj_BlockHitActive);
    std::array<u8, cGarden_HitMarkerCount> garden_hit_markers = {};
    for (u32 n = 0; n < cGarden_HitMarkerCount; ++n){
        garden_hit_markers[n] = PowerPC::MMU::HostRead_U8(guard, aGarden_BlockHitPending + n);
    }
    std::array<u8, cRosterSize> on_fire = {};
    for (u8 pos = 0; pos < cRosterSize; ++pos){
        on_fire[pos] = PowerPC::MMU::HostRead_U8(guard, aFielder_OnFire + (pos * cFielder_Offset));
    }
    std::array<u8, cCastle_HitMarkerCount> castle_hit_markers = {};
    for (u32 n = 0; n < cCastle_HitMarkerCount; ++n){
        castle_hit_markers[n] = PowerPC::MMU::HostRead_U8(guard, aCastle_HazardHitPending + n);
    }

    auto readBallPos = [&](HazardEvent& in_event){
        in_event.pos_x = ball_pos[0];
        in_event.pos_y = ball_pos[1];
        in_event.pos_z = ball_pos[2];
    };
    auto readBallVelocity = [&](){
        return std::array<u32, 3>{PowerPC::MMU::HostRead_U32(guard, aAB_BallVel_X),
                                  PowerPC::MMU::HostRead_U32(guard, aAB_BallVel_Y),
                                  PowerPC::MMU::HostRead_U32(guard, aAB_BallVel_Z)};
    };
    auto floatToJSON = [](float in_value){
        std::stringstream ss;
        ss << in_value;
        return ss.str();
    };
    auto xzDistance = [](float x1, float z1, float x2, float z2){
        float dx = x1 - x2;
        float dz = z1 - z2;
        return std::sqrt((dx * dx) + (dz * dz));
    };
    auto waitForVelocity = [&](u32 obj_index, bool wait_for_hold_countdown){
        m_hazard_state.pending_velocity.push_back({in_contact->hazard_events.size() - 1, obj_index, 0, wait_for_hold_countdown});
    };
    //Adds a detail, or overwrites it if it is already there. For counts that grow over several frames
    auto setHazardDetail = [](HazardEvent& in_event, const std::string& in_key, const std::string& in_value){
        for (auto& detail : in_event.details){
            if (detail.key == in_key){
                detail.value = in_value;
                return;
            }
        }
        in_event.details.push_back({in_key, in_value, ""});
    };
    auto floatToBits = [&](float in_value){
        float_converter.fnum = in_value;
        return float_converter.num;
    };
    auto blockTypeName = [](u8 in_type) -> std::string {
        switch (in_type){
            case cBlockType_Brick:   return "\"Brick\"";
            case cBlockType_HitFx:   return "\"Hit FX\"";
            case cBlockType_Note:    return "\"Note\"";
            case cBlockType_Mystery: return "\"Mystery\"";
            default:                 return "\"Unknown\"";
        }
    };

    auto isCastleKind = [](u8 in_kind){
        return in_kind == static_cast<u8>(HAZARD_TYPE::THWOMP) || in_kind == static_cast<u8>(HAZARD_TYPE::FIREBALL) || in_kind == static_cast<u8>(HAZARD_TYPE::STAR_PAD);
    };
    auto thwompStateName = [](u8 in_state) -> std::string {
        switch (in_state){
            case cThwompState_Perched:   return "\"Perched\"";
            case cThwompState_Windup:    return "\"Windup\"";
            case cThwompState_Falling:   return "\"Falling\"";
            case cThwompState_Grounded:  return "\"Grounded\"";
            case cThwompState_Returning: return "\"Returning\"";
            default:                     return "\"Unknown\"";
        }
    };

    //Record the ball velocity for events that were waiting on it. Done before this frame's events so a
    //bounce logged last frame reads the post-bounce velocity
    for (auto it = m_hazard_state.pending_velocity.begin(); it != m_hazard_state.pending_velocity.end();){
        if (it->from_object_delta) { ++it; continue; } //Resolved once this frame's object positions are read
        ++it->frames_waited;
        bool ready = it->wait_for_hold_countdown ? (hold_countdown == 0 && it->frames_waited >= cHeldVelocityMinFrames)
                   : it->wait_for_block_knockback ? (block_hit_active == 0 && it->frames_waited >= 1)
                   : true;
        if (ready || it->frames_waited >= cVelocityMaxWaitFrames){
            in_contact->hazard_events[it->event_index].result_velocity = readBallVelocity();
            it = m_hazard_state.pending_velocity.erase(it);
        }
        else {
            ++it;
        }
    }

    std::array<u8, cJungle_MaxCannons> cannon_state = {};
    //=== Snapshot every object this frame ===
    std::array<HazardObjSnapshot, cStadiumObj_MaxCount> objects;
    for (u32 i = 0; i < obj_count; ++i){
        u32 obj = obj_array + (i * cStadiumObj_Size);
        HazardObjSnapshot& snap = objects[i];
        u32 update_fn = PowerPC::MMU::HostRead_U32(guard, obj + cStadiumObj_UpdateFn);
        u32 on_collision_fn = PowerPC::MMU::HostRead_U32(guard, obj + cStadiumObj_OnCollisionFn);
        if (update_fn == cUpdateFn_YoshiParkPlant){
            snap.kind  = static_cast<u8>(HAZARD_TYPE::RED_PLANT);
            snap.state = PowerPC::MMU::HostRead_U8(guard, obj + cPlant_State);
            snap.aux   = PowerPC::MMU::HostRead_U8(guard, obj + cPlant_Type);
            snap.flag  = PowerPC::MMU::HostRead_U8(guard, obj + cPlant_BallInMouth);
            snap.angle = floatConverter(PowerPC::MMU::HostRead_U32(guard, obj + cPlant_SpitAngle));
        }
        else if (update_fn == cUpdateFn_PalaceChomp){
            snap.kind  = static_cast<u8>(HAZARD_TYPE::CHOMP);
            snap.state = PowerPC::MMU::HostRead_U8(guard, obj + cChomp_State);
        }
        else if (update_fn == cUpdateFn_PalaceTornado){
            snap.kind  = static_cast<u8>(HAZARD_TYPE::TORNADO);
            snap.state = PowerPC::MMU::HostRead_U8(guard, obj + cTornado_State);
            snap.aux   = PowerPC::MMU::HostRead_U8(guard, obj + cTornado_SpinDir);
            snap.angle = floatConverter(PowerPC::MMU::HostRead_U32(guard, obj + cTornado_ReleaseAngle));
        }
        else if (update_fn == cUpdateFn_PalaceSandStar){
            snap.kind  = static_cast<u8>(HAZARD_TYPE::SAND_STAR);
            snap.flag  = PowerPC::MMU::HostRead_U8(guard, obj + cSandStar_HitFlag);
            snap.ptr   = PowerPC::MMU::HostRead_U32(guard, obj + cSandStar_HitAnimPtr);
            snap.state = (snap.ptr != 0) ? 1 : 0;
        }
        else if (update_fn == cUpdateFn_CastleThwomp){
            snap.kind  = static_cast<u8>(HAZARD_TYPE::THWOMP);
            snap.state = PowerPC::MMU::HostRead_U8(guard, obj + cThwomp_State);
        }
        else if (update_fn == cUpdateFn_CastleFlame){
            snap.kind  = static_cast<u8>(HAZARD_TYPE::FIREBALL);
            snap.state = PowerPC::MMU::HostRead_U8(guard, obj + cFlame_State);
        }
        else if (update_fn == 0 && on_collision_fn == cOnCollisionFn_CastleStarPad){
            snap.kind  = static_cast<u8>(HAZARD_TYPE::STAR_PAD);
            snap.state = PowerPC::MMU::HostRead_U8(guard, obj + cCastleObj_SubType);
        }
        else if (update_fn == cUpdateFn_JungleKlaptrap){
            snap.kind  = static_cast<u8>(HAZARD_TYPE::KLAPTRAP);
            snap.state = PowerPC::MMU::HostRead_U8(guard, obj + cKlaptrap_State);
            snap.aux   = PowerPC::MMU::HostRead_U8(guard, obj + cKlaptrap_AttachedFielder);
            snap.flag  = PowerPC::MMU::HostRead_U8(guard, obj + cKlaptrap_StarAwarded);
        }
        else if (update_fn == 0 && on_collision_fn == cOnCollisionFn_JungleBarrel){
            snap.kind  = static_cast<u8>(HAZARD_TYPE::BARREL);
            snap.state = PowerPC::MMU::HostRead_U8(guard, obj + cBarrel_State);
            snap.angle = floatConverter(PowerPC::MMU::HostRead_U32(guard, obj + cBarrel_Yaw));
        }
        else if (m_game_info.stadium == cStadiumId_DKJungle && PowerPC::MMU::HostRead_U8(guard, obj + cJungleObj_Type) == cJungleType_Cannon){
            //Cannons aren't a hazard of their own, but their armed state says what triggered a barrel
            u8 cannon_slot = PowerPC::MMU::HostRead_U8(guard, obj + cStadiumObj_SlotIndex);
            if (cannon_slot < cJungle_MaxCannons) { cannon_state[cannon_slot] = PowerPC::MMU::HostRead_U8(guard, obj + cCannon_State); }
            continue;
        }
        else if (m_game_info.stadium == cStadiumId_PeachGarden){
            //Blocks. A broken brick loses its callback, so keep tracking anything that was a block last frame
            u8 block_type = PowerPC::MMU::HostRead_U8(guard, obj + cGardenObj_Type);
            bool is_block = (on_collision_fn == cOnCollisionFn_GardenBrick || on_collision_fn == cOnCollisionFn_GardenHitFx
                          || on_collision_fn == cOnCollisionFn_GardenNote || on_collision_fn == cOnCollisionFn_GardenMystery
                          || m_hazard_state.objects[i].kind == static_cast<u8>(HAZARD_TYPE::BLOCK));
            if (!is_block || block_type > cBlockType_Mystery) { continue; }
            snap.kind  = static_cast<u8>(HAZARD_TYPE::BLOCK);
            snap.state = PowerPC::MMU::HostRead_U8(guard, obj + cGardenObj_Flags);
            snap.aux   = block_type;
            snap.flag  = PowerPC::MMU::HostRead_U8(guard, obj + cGardenObj_Variant);
            snap.valid = true;
            snap.slot  = PowerPC::MMU::HostRead_U8(guard, obj + cGardenObj_SlotIndex);
            u32 table_entry = aGarden_BlockTable + (snap.slot * cGarden_BlockTableEntry);
            snap.x = floatConverter(PowerPC::MMU::HostRead_U32(guard, table_entry));
            snap.y = floatConverter(PowerPC::MMU::HostRead_U32(guard, table_entry + 4));
            snap.z = floatConverter(PowerPC::MMU::HostRead_U32(guard, table_entry + 8));
            continue;
        }
        else {
            continue; //Not a hazard we track
        }
        snap.valid = true;
        //Bowser Castle objects keep their position and index at different offsets
        bool castle_layout = isCastleKind(snap.kind);
        snap.slot = PowerPC::MMU::HostRead_U8(guard, obj + (castle_layout ? cCastleObj_SlotIndex : cStadiumObj_SlotIndex));
        snap.x = floatConverter(PowerPC::MMU::HostRead_U32(guard, obj + (castle_layout ? cCastleObj_Pos_X : cStadiumObj_Pos_X)));
        snap.y = floatConverter(PowerPC::MMU::HostRead_U32(guard, obj + (castle_layout ? cCastleObj_Pos_Y : cStadiumObj_Pos_Y)));
        snap.z = floatConverter(PowerPC::MMU::HostRead_U32(guard, obj + (castle_layout ? cCastleObj_Pos_Z : cStadiumObj_Pos_Z)));
    }

    //Barrel launches: velocity is how far the barrel moved since it was logged
    for (auto it = m_hazard_state.pending_velocity.begin(); it != m_hazard_state.pending_velocity.end();){
        if (!it->from_object_delta) { ++it; continue; }
        ++it->frames_waited;
        HazardObjSnapshot& moved = objects[it->obj_index];
        if (moved.valid && it->frames_waited >= 1){
            in_contact->hazard_events[it->event_index].result_velocity = std::array<u32, 3>{floatToBits(moved.x - it->prev_x),
                                                                                             floatToBits(moved.y - it->prev_y),
                                                                                             floatToBits(moved.z - it->prev_z)};
            it = m_hazard_state.pending_velocity.erase(it);
        }
        else if (it->frames_waited >= cVelocityMaxWaitFrames){
            it = m_hazard_state.pending_velocity.erase(it);
        }
        else {
            ++it;
        }
    }

    //First frame after contact: just store the baseline
    if (!m_hazard_state.initialized){
        m_hazard_state.objects = objects;
        m_hazard_state.fielder_knockout = knockouts;
        m_hazard_state.frames_since_last_bounce = frames_since_bounce;
        m_hazard_state.chomp_ball_marker = chomp_ball_marker;
        m_hazard_state.fielder_on_fire = on_fire;
        m_hazard_state.castle_hit_markers = castle_hit_markers;
        m_hazard_state.jungle_cannon_state = cannon_state;
        m_hazard_state.garden_hit_markers = garden_hit_markers;
        m_hazard_state.initialized = true;
        return;
    }

    //Every interaction from one activation (plant pop-up, chomp attack, tornado spin-up) shares a parent sequence
    auto getParentSequence = [&](HazardObjSnapshot& in_prev) -> u16 {
        if (in_prev.parent_sequence == 0) { in_prev.parent_sequence = m_hazard_state.next_parent_sequence++; }
        return in_prev.parent_sequence;
    };
    auto plantIsActive = [](const HazardObjSnapshot& in_snap){
        return in_snap.state >= cPlantState_PopUp && in_snap.state <= cPlantState_Star;
    };
    auto chompIsHunting = [](const HazardObjSnapshot& in_snap){
        return in_snap.state == cChompState_Stalking || in_snap.state == cChompState_Attacking;
    };

    //=== Ball interactions ===
    std::array<bool, cStadiumObj_MaxCount> ball_contact_logged = {};
    for (u32 i = 0; i < obj_count; ++i){
        if (!objects[i].valid || !m_hazard_state.objects[i].valid) { continue; }
        HazardObjSnapshot& prev = m_hazard_state.objects[i];
        HazardObjSnapshot& now  = objects[i];
        u32 obj = obj_array + (i * cStadiumObj_Size);

        if (now.kind == static_cast<u8>(HAZARD_TYPE::RED_PLANT)){
            //Red plant ate the ball
            if (!prev.flag && now.flag){
                HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::RED_PLANT), now.slot,
                                                           static_cast<u8>(HAZARD_INTERACTION::BALL_CONTACT), getParentSequence(prev), frame);
                readBallPos(hazard_event);
                hazard_event.details.push_back({"Spit Angle", floatToJSON(now.angle), ""});
                ball_contact_logged[i] = true;
                std::cout << "Hazard: Plant " << std::to_string(now.slot) << " ate the ball. Frame=" << std::to_string(frame) << "\n";
            }
            //Red plant spat the ball out
            else if (prev.flag && !now.flag && now.state == cPlantState_Spit){
                HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::RED_PLANT), now.slot,
                                                           static_cast<u8>(HAZARD_INTERACTION::PROJECTILE), getParentSequence(prev), frame);
                readBallPos(hazard_event);
                waitForVelocity(i, true);
                ball_contact_logged[i] = true;
                std::cout << "Hazard: Plant " << std::to_string(now.slot) << " spat the ball. Frame=" << std::to_string(frame) << "\n";
            }

            //Ball hit a yellow plant. The game awards the batting team a star and the plant is red for the rest of the game
            if (now.state == cPlantState_Star && prev.state != cPlantState_Star){
                HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::YELLOW_PLANT), now.slot,
                                                           static_cast<u8>(HAZARD_INTERACTION::BALL_CONTACT), getParentSequence(prev), frame);
                readBallPos(hazard_event);
                hazard_event.details.push_back({"Star Awarded", m_game_info.star_skills_on ? "true" : "false", ""});
                waitForVelocity(i, false);
                ball_contact_logged[i] = true;
                std::cout << "Hazard: Ball hit yellow plant " << std::to_string(now.slot) << ". Frame=" << std::to_string(frame) << "\n";
            }
        }
        else if (now.kind == static_cast<u8>(HAZARD_TYPE::CHOMP)){
            //Chomp lunged at the ball/fielder. Its velocity and target angle are set the same frame
            if (now.state == cChompState_Attacking && prev.state != cChompState_Attacking){
                HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::CHOMP), now.slot,
                                                           static_cast<u8>(HAZARD_INTERACTION::PROJECTILE), getParentSequence(prev), frame);
                hazard_event.pos_x = PowerPC::MMU::HostRead_U32(guard, obj + cStadiumObj_Pos_X);
                hazard_event.pos_y = PowerPC::MMU::HostRead_U32(guard, obj + cStadiumObj_Pos_Y);
                hazard_event.pos_z = PowerPC::MMU::HostRead_U32(guard, obj + cStadiumObj_Pos_Z);
                hazard_event.result_velocity = std::array<u32, 3>{PowerPC::MMU::HostRead_U32(guard, obj + cChomp_Velo_X),
                                                                  PowerPC::MMU::HostRead_U32(guard, obj + cChomp_Velo_Y),
                                                                  PowerPC::MMU::HostRead_U32(guard, obj + cChomp_Velo_Z)};
                hazard_event.details.push_back({"From State", (prev.state == cChompState_Stalking) ? "\"Stalking\"" : "\"Awake\"", ""});
                std::cout << "Hazard: Chomp " << std::to_string(now.slot) << " attacked. Frame=" << std::to_string(frame) << "\n";
            }
            //Chomp knocked the ball away. A lunge also ends when the chain runs out or when it knocks a fielder over,
            //and both of those leave it sitting next to the ball, so the hit marker the game posts is the only proof
            //it actually hit the ball. chomp_attack only sets that marker on the frame it knocks the ball
            if (prev.state == cChompState_Attacking && now.state == cChompState_ReturningHome){
                bool marker_shown = (m_hazard_state.chomp_ball_marker == 0 && chomp_ball_marker != 0);
                if (marker_shown){
                    HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::CHOMP), now.slot,
                                                               static_cast<u8>(HAZARD_INTERACTION::BALL_CONTACT), getParentSequence(prev), frame);
                    readBallPos(hazard_event);
                    waitForVelocity(i, false);
                    ball_contact_logged[i] = true;
                    std::cout << "Hazard: Chomp " << std::to_string(now.slot) << " knocked the ball. Frame=" << std::to_string(frame) << "\n";
                }
            }
        }
        else if (now.kind == static_cast<u8>(HAZARD_TYPE::TORNADO)){
            //Ball came close enough to spin the tornado up. Not an event by itself, remember where it happened
            if (prev.state == cTornadoState_Idle && now.state == cTornadoState_Triggered){
                getParentSequence(prev);
                prev.aux_pos = ball_pos;
            }
            //Ball got pulled in
            if (prev.state == cTornadoState_Triggered && now.state == cTornadoState_BallCaptured){
                HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::TORNADO), now.slot,
                                                           static_cast<u8>(HAZARD_INTERACTION::BALL_CONTACT), getParentSequence(prev), frame);
                readBallPos(hazard_event);
                hazard_event.details.push_back({"Entered", "true", ""});
                hazard_event.details.push_back({"Spin Direction", std::to_string(static_cast<s8>(now.aux)), ""});
                hazard_event.details.push_back({"Release Angle", floatToJSON(now.angle), ""});
                ball_contact_logged[i] = true;
                std::cout << "Hazard: Ball entered tornado " << std::to_string(now.slot) << ". Frame=" << std::to_string(frame) << "\n";
            }
            //Ball spun the tornado up but got away before it was pulled in
            else if (prev.state == cTornadoState_Triggered && now.state == cTornadoState_WindingDown){
                HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::TORNADO), now.slot,
                                                           static_cast<u8>(HAZARD_INTERACTION::BALL_CONTACT), getParentSequence(prev), frame);
                hazard_event.pos_x = prev.aux_pos[0];
                hazard_event.pos_y = prev.aux_pos[1];
                hazard_event.pos_z = prev.aux_pos[2];
                hazard_event.details.push_back({"Entered", "false", ""});
                hazard_event.details.push_back({"Spin Direction", std::to_string(static_cast<s8>(now.aux)), ""});
                std::cout << "Hazard: Ball triggered tornado " << std::to_string(now.slot) << " without entering. Frame=" << std::to_string(frame) << "\n";
            }
            //Tornado threw the ball back out
            if (prev.state == cTornadoState_BallCaptured && now.state == cTornadoState_WindingDown){
                HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::TORNADO), now.slot,
                                                           static_cast<u8>(HAZARD_INTERACTION::PROJECTILE), getParentSequence(prev), frame);
                readBallPos(hazard_event);
                hazard_event.details.push_back({"Spin Direction", std::to_string(static_cast<s8>(now.aux)), ""});
                waitForVelocity(i, true);
                ball_contact_logged[i] = true;
                std::cout << "Hazard: Tornado " << std::to_string(now.slot) << " released the ball. Frame=" << std::to_string(frame) << "\n";
            }
        }
        else if (now.kind == static_cast<u8>(HAZARD_TYPE::SAND_STAR)){
            //Ball hit the star. The hit animation starts every time, the collected flag only when star skills are on
            bool hit_anim_started = (prev.ptr == 0 && now.ptr != 0);
            bool star_collected   = (prev.flag == 0 && now.flag != 0);
            if (hit_anim_started || star_collected){
                HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::SAND_STAR), now.slot,
                                                           static_cast<u8>(HAZARD_INTERACTION::BALL_CONTACT), getParentSequence(prev), frame);
                readBallPos(hazard_event);
                hazard_event.details.push_back({"Star Awarded", star_collected ? "true" : "false", ""});
                waitForVelocity(i, false);
                ball_contact_logged[i] = true;
                std::cout << "Hazard: Ball hit sand star " << std::to_string(now.slot) << ". Frame=" << std::to_string(frame) << "\n";
            }
        }
        else if (now.kind == static_cast<u8>(HAZARD_TYPE::THWOMP)){
            //Thwomp dropped. Its landing spot is filled in as the target once it hits the ground
            if (now.state == cThwompState_Falling && prev.state != cThwompState_Falling){
                HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::THWOMP), now.slot,
                                                           static_cast<u8>(HAZARD_INTERACTION::PROJECTILE), getParentSequence(prev), frame);
                hazard_event.pos_x = PowerPC::MMU::HostRead_U32(guard, obj + cCastleObj_Pos_X);
                hazard_event.pos_y = PowerPC::MMU::HostRead_U32(guard, obj + cCastleObj_Pos_Y);
                hazard_event.pos_z = PowerPC::MMU::HostRead_U32(guard, obj + cCastleObj_Pos_Z);
                u32 fall_speed = PowerPC::MMU::HostRead_U32(guard, obj + cThwomp_FallSpeed);
                hazard_event.result_velocity = std::array<u32, 3>{0, fall_speed, 0};
                prev.aux_event = in_contact->hazard_events.size() - 1;
                prev.aux_event_valid = true;
                std::cout << "Hazard: Thwomp " << std::to_string(now.slot) << " dropped. Frame=" << std::to_string(frame) << "\n";
            }
            if (now.state == cThwompState_Grounded && prev.state != cThwompState_Grounded && prev.aux_event_valid){
                in_contact->hazard_events[prev.aux_event].target = std::array<u32, 3>{PowerPC::MMU::HostRead_U32(guard, obj + cCastleObj_Pos_X),
                                                                                       PowerPC::MMU::HostRead_U32(guard, obj + cCastleObj_Pos_Y),
                                                                                       PowerPC::MMU::HostRead_U32(guard, obj + cCastleObj_Pos_Z)};
                prev.aux_event_valid = false;
            }
        }
        else if (now.kind == static_cast<u8>(HAZARD_TYPE::FIREBALL)){
            //Fireball launched. Off by default, see cHazard_LogFlameShots
            if (cHazard_LogFlameShots && now.state == cFlameState_Flying && prev.state != cFlameState_Flying){
                HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::FIREBALL), now.slot,
                                                           static_cast<u8>(HAZARD_INTERACTION::PROJECTILE), getParentSequence(prev), frame);
                hazard_event.pos_x = PowerPC::MMU::HostRead_U32(guard, obj + cCastleObj_Pos_X);
                hazard_event.pos_y = PowerPC::MMU::HostRead_U32(guard, obj + cCastleObj_Pos_Y);
                hazard_event.pos_z = PowerPC::MMU::HostRead_U32(guard, obj + cCastleObj_Pos_Z);
                std::array<u32, 3> velocity = {PowerPC::MMU::HostRead_U32(guard, obj + cFlame_Velo_X),
                                               PowerPC::MMU::HostRead_U32(guard, obj + cFlame_Velo_Y),
                                               PowerPC::MMU::HostRead_U32(guard, obj + cFlame_Velo_Z)};
                hazard_event.result_velocity = velocity;
                std::cout << "Hazard: Flame " << std::to_string(now.slot) << " launched a fireball. Frame=" << std::to_string(frame) << "\n";
            }
            //Ball flew through the fireball and put it out. flameControl ends the fireball the same way when it hits the
            //ground, which every fireball eventually does, so this transition alone proves nothing. The game only runs its
            //ball test when no fielder has the ball, and uses a 3D radius of cHazard_FlameRadius. Its end branch overwrites
            //the fireball's Y (to 10) but leaves X/Z on the spot it died, so only the XZ part of that test can be redone
            //here. XZ at the same radius never rejects a real hit - it just also accepts a ball passing high overhead
            if (prev.state == cFlameState_Flying && now.state == cFlameState_Ended && !fielder_has_ball){
                float dist = xzDistance(ball_x, ball_z, now.x, now.z);
                if (dist <= cHazard_FlameRadius){
                    HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::FIREBALL), now.slot,
                                                               static_cast<u8>(HAZARD_INTERACTION::BALL_CONTACT), getParentSequence(prev), frame);
                    readBallPos(hazard_event);
                    ball_contact_logged[i] = true;
                    //Y values are logged so the stored-Y convention can be confirmed and this tightened to the real 3D test
                    std::cout << "Hazard: Ball put out fireball from flame " << std::to_string(now.slot) << ". Frame=" << std::to_string(frame)
                              << " XZ dist=" << dist << " ball Y=" << ball_y << " flame Y last frame=" << prev.y << "\n";
                }
            }
        }
        else if (now.kind == static_cast<u8>(HAZARD_TYPE::STAR_PAD)){
            //Ball hit a star pad. The game only marks it used (and awards the star) when star skills are on
            if (now.state == cStarPadType_Used && prev.state != cStarPadType_Used){
                HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::STAR_PAD), now.slot,
                                                           static_cast<u8>(HAZARD_INTERACTION::BALL_CONTACT), getParentSequence(prev), frame);
                readBallPos(hazard_event);
                hazard_event.details.push_back({"Star Awarded", "true", ""});
                hazard_event.details.push_back({"Pad Type", (prev.state == cStarPadType_Wall) ? "\"Wall\"" : "\"Floor\"", ""});
                waitForVelocity(i, false);
                ball_contact_logged[i] = true;
                std::cout << "Hazard: Ball hit star pad " << std::to_string(now.slot) << ". Frame=" << std::to_string(frame) << "\n";
            }
        }
        else if (now.kind == static_cast<u8>(HAZARD_TYPE::BARREL)){
            //Cannon fired its barrel. The game aims at where the ball will land, clamped to the outfield
            if (now.state == cBarrelState_Rolling && prev.state != cBarrelState_Rolling){
                HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::BARREL), now.slot,
                                                           static_cast<u8>(HAZARD_INTERACTION::PROJECTILE), getParentSequence(prev), frame);
                hazard_event.pos_x = PowerPC::MMU::HostRead_U32(guard, obj + cStadiumObj_Pos_X);
                hazard_event.pos_y = PowerPC::MMU::HostRead_U32(guard, obj + cStadiumObj_Pos_Y);
                hazard_event.pos_z = PowerPC::MMU::HostRead_U32(guard, obj + cStadiumObj_Pos_Z);
                hazard_event.target = std::array<u32, 3>{PowerPC::MMU::HostRead_U32(guard, aJungle_BarrelTarget_X),
                                                         PowerPC::MMU::HostRead_U32(guard, aJungle_BarrelTarget_Y),
                                                         PowerPC::MMU::HostRead_U32(guard, aJungle_BarrelTarget_Z)};
                hazard_event.details.push_back({"Launch Yaw", floatToJSON(now.angle), ""});
                u8 trigger = (now.slot < cJungle_MaxCannons) ? m_hazard_state.jungle_cannon_state[now.slot] : 0;
                std::string trigger_name = (trigger == cCannonState_ArmedGrounder) ? "\"Grounder\"" : (trigger == cCannonState_ArmedFlyBall) ? "\"Fly Ball\"" : "\"Unknown\"";
                hazard_event.details.push_back({"Trigger", trigger_name, ""});
                m_hazard_state.pending_velocity.push_back({in_contact->hazard_events.size() - 1, i, 0, false, true, now.x, now.y, now.z});
                prev.aux_event = in_contact->hazard_events.size() - 1;
                prev.aux_event_valid = true;
                std::cout << "Hazard: Cannon " << std::to_string(now.slot) << " fired a barrel. Frame=" << std::to_string(frame) << "\n";
            }
            //Barrel broke against a wall. Note where on its launch event
            if (now.state == cBarrelState_Breaking && prev.state != cBarrelState_Breaking && prev.aux_event_valid){
                HazardEvent& launch_event = in_contact->hazard_events[prev.aux_event];
                launch_event.details.push_back({"Break Position - X", floatToJSON(now.x), ""});
                launch_event.details.push_back({"Break Position - Y", floatToJSON(now.y), ""});
                launch_event.details.push_back({"Break Position - Z", floatToJSON(now.z), ""});
                prev.aux_event_valid = false;
            }
        }
        else if (now.kind == static_cast<u8>(HAZARD_TYPE::KLAPTRAP)){
            //Klaptrap bit a fielder
            if (now.state == cKlaptrapState_Attached && prev.state != cKlaptrapState_Attached && now.aux < cRosterSize){
                HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::KLAPTRAP), now.slot,
                                                           static_cast<u8>(HAZARD_INTERACTION::FIELDER_CONTACT), getParentSequence(prev), frame);
                addFielderToHazardEvent(guard, hazard_event, now.aux);
                //Running count carried across at bats, sampled at the moment of the bite (includes this one)
                hazard_event.details.push_back({"Total Attached Klaptraps", std::to_string(PowerPC::MMU::HostRead_U8(guard, aFielder_AttachedKlaptraps + (now.aux * cFielder_Offset))), ""});
                std::cout << "Hazard: Klaptrap " << std::to_string(now.slot) << " bit fielder pos " << std::to_string(now.aux) << ". Frame=" << std::to_string(frame) << "\n";
            }
            //Ball hit the klaptrap and knocked it off (a star is awarded when star skills are on)
            if (now.state == cKlaptrapState_Launched && prev.state < cKlaptrapState_Attached){
                HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::KLAPTRAP), now.slot,
                                                           static_cast<u8>(HAZARD_INTERACTION::BALL_CONTACT), getParentSequence(prev), frame);
                readBallPos(hazard_event);
                hazard_event.details.push_back({"Star Awarded", (prev.flag == 0 && now.flag != 0) ? "true" : "false", ""});
                waitForVelocity(i, false);
                ball_contact_logged[i] = true;
                std::cout << "Hazard: Ball hit klaptrap " << std::to_string(now.slot) << ". Frame=" << std::to_string(frame) << "\n";
            }
            //A rolling barrel knocked it off a fielder or ran it over. Both are counted on that barrel:
            //a detach goes on its fielder contact event (that is when it happens), a flattening on its launch event
            bool detached = (prev.state == cKlaptrapState_Attached && now.state == cKlaptrapState_Launched);
            bool run_over = (prev.state < cKlaptrapState_Attached && now.state == cKlaptrapState_RunOver);
            if (detached || run_over){
                int closest = -1;
                float closest_dist = 0;
                for (u32 b = 0; b < obj_count; ++b){
                    if (!objects[b].valid || objects[b].kind != static_cast<u8>(HAZARD_TYPE::BARREL)) { continue; }
                    if (objects[b].state != cBarrelState_Rolling && m_hazard_state.objects[b].state != cBarrelState_Rolling) { continue; }
                    float dist = xzDistance(now.x, now.z, objects[b].x, objects[b].z);
                    if (closest < 0 || dist < closest_dist){
                        closest = b;
                        closest_dist = dist;
                    }
                }
                if (closest >= 0){
                    HazardObjSnapshot& barrel = m_hazard_state.objects[closest];
                    if (detached){
                        //The knockout event may not exist yet this frame. It picks the count up when it is created
                        ++barrel.detached_klaptraps;
                        if (barrel.fielder_event_valid){
                            setHazardDetail(in_contact->hazard_events[barrel.fielder_event], "Detached Klaptraps", std::to_string(barrel.detached_klaptraps));
                        }
                    }
                    else {
                        ++barrel.ran_over_klaptraps;
                        if (barrel.aux_event_valid){
                            setHazardDetail(in_contact->hazard_events[barrel.aux_event], "Ran Over Klaptraps", std::to_string(barrel.ran_over_klaptraps));
                        }
                    }
                }
                std::cout << "Hazard: Barrel " << (detached ? "detached" : "ran over") << " klaptrap " << std::to_string(now.slot) << ". Frame=" << std::to_string(frame) << "\n";
            }
        }
    }

    //=== Ball hit a thwomp === The game posts a hit marker whenever the ball bounces off one
    bool thwomp_hit = false;
    for (u32 n = 2; n <= 3; ++n){
        if (m_hazard_state.castle_hit_markers[n] == 0 && castle_hit_markers[n] != 0) { thwomp_hit = true; }
    }
    if (thwomp_hit){
        int closest = -1;
        float closest_dist = cHazard_ThwompAttributionRadius;
        for (u32 i = 0; i < obj_count; ++i){
            if (!objects[i].valid || objects[i].kind != static_cast<u8>(HAZARD_TYPE::THWOMP)) { continue; }
            float dist = xzDistance(ball_x, ball_z, objects[i].x, objects[i].z);
            if (dist < closest_dist){
                closest = i;
                closest_dist = dist;
            }
        }
        if (closest >= 0){
            HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::THWOMP), objects[closest].slot,
                                                       static_cast<u8>(HAZARD_INTERACTION::BALL_CONTACT), getParentSequence(m_hazard_state.objects[closest]), frame);
            readBallPos(hazard_event);
            hazard_event.details.push_back({"Thwomp State", thwompStateName(objects[closest].state), ""});
            waitForVelocity(static_cast<u32>(closest), false);
            ball_contact_logged[closest] = true;
            std::cout << "Hazard: Ball hit thwomp " << std::to_string(objects[closest].slot) << ". Frame=" << std::to_string(frame) << "\n";
        }
        else {
            std::cout << "Hazard: Ball hit a thwomp but none is nearby\n";
        }
    }

    //=== Fielder set on fire === Fireballs burn any fielder they pass within cHazard_FlameRadius of
    for (u8 pos = 0; pos < cRosterSize; ++pos){
        if (m_hazard_state.fielder_on_fire[pos] != 0 || on_fire[pos] == 0) { continue; }

        float fielder_x = floatConverter(PowerPC::MMU::HostRead_U32(guard, aFielder_Pos_X + (pos * cFielder_Offset)));
        float fielder_z = floatConverter(PowerPC::MMU::HostRead_U32(guard, aFielder_Pos_Z + (pos * cFielder_Offset)));

        //The burn can be processed before or after the flame's own update, so accept a fireball that is flying or just exploded
        int closest = -1;
        float closest_dist = cHazard_FlameAttributionRadius;
        for (u32 i = 0; i < obj_count; ++i){
            if (!objects[i].valid || objects[i].kind != static_cast<u8>(HAZARD_TYPE::FIREBALL)) { continue; }
            HazardObjSnapshot& prev = m_hazard_state.objects[i];
            HazardObjSnapshot& now  = objects[i];
            if (prev.state != cFlameState_Flying && now.state != cFlameState_Flying && now.state != cFlameState_Exploding) { continue; }
            float dist = xzDistance(fielder_x, fielder_z, now.x, now.z);
            if (dist < closest_dist){
                closest = i;
                closest_dist = dist;
            }
        }
        if (closest < 0){
            std::cout << "Hazard: Fielder pos " << std::to_string(pos) << " caught fire but no fireball nearby\n";
            continue;
        }

        HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::FIREBALL), objects[closest].slot,
                                                   static_cast<u8>(HAZARD_INTERACTION::FIELDER_CONTACT), getParentSequence(m_hazard_state.objects[closest]), frame);
        addFielderToHazardEvent(guard, hazard_event, pos);
        std::cout << "Hazard: Fireball from flame " << std::to_string(objects[closest].slot) << " burned fielder pos " << std::to_string(pos) << ". Frame=" << std::to_string(frame) << "\n";
    }

    //=== Ball hit a block === Every block hit posts a marker whose index says which kind of block it was
    for (u32 n = 0; n < cGarden_HitMarkerCount; ++n){
        if (m_hazard_state.garden_hit_markers[n] != 0 || garden_hit_markers[n] == 0) { continue; }
        u8 hit_variant = static_cast<u8>(n / 2);

        //Closest block of that kind to the ball. The table stores the block's Y with the opposite sign to the ball's
        int closest = -1;
        float closest_dist = cHazard_BlockAttributionRadius;
        for (u32 i = 0; i < obj_count; ++i){
            if (!objects[i].valid || objects[i].kind != static_cast<u8>(HAZARD_TYPE::BLOCK) || ball_contact_logged[i]) { continue; }
            if (objects[i].flag != hit_variant && m_hazard_state.objects[i].aux != cBlockType_Mystery) { continue; }
            float dx = ball_x - objects[i].x;
            float dy = std::fabs(ball_y) - std::fabs(objects[i].y);
            float dz = ball_z - objects[i].z;
            float dist = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
            if (dist < closest_dist){
                closest = i;
                closest_dist = dist;
            }
        }
        if (closest < 0){
            std::cout << "Hazard: Ball hit a block of kind " << std::to_string(hit_variant) << " but none is nearby\n";
            continue;
        }

        HazardObjSnapshot& prev = m_hazard_state.objects[closest];
        HazardObjSnapshot& now  = objects[closest];
        bool broken = ((prev.state & 0x80) != 0 && (now.state & 0x80) == 0 && now.flag == cBlockType_Brick);
        HazardEvent& hazard_event = addHazardEvent(in_contact, static_cast<u8>(HAZARD_TYPE::BLOCK), now.slot,
                                                   static_cast<u8>(HAZARD_INTERACTION::BALL_CONTACT), getParentSequence(prev), frame);
        readBallPos(hazard_event);
        hazard_event.details.push_back({"Block Type", blockTypeName(now.flag), ""});
        hazard_event.details.push_back({"Broken", broken ? "true" : "false", ""});
        hazard_event.details.push_back({"Revealed", (prev.aux == cBlockType_Mystery && now.aux != cBlockType_Mystery) ? "true" : "false", ""});
        hazard_event.details.push_back({"Visible", ((now.state & 0x80) != 0) ? "true" : "false", ""});
        if (broken) { hazard_event.details.push_back({"Star Awarded", m_game_info.star_skills_on ? "true" : "false", ""}); }
        //Note blocks push the ball for a few frames before it flies off
        m_hazard_state.pending_velocity.push_back({in_contact->hazard_events.size() - 1, static_cast<u32>(closest), 0, false, false, 0, 0, 0, (now.flag == cBlockType_Note)});
        ball_contact_logged[closest] = true;
        std::cout << "Hazard: Ball hit block " << std::to_string(now.slot) << " " << blockTypeName(now.flag) << (broken ? " and broke it" : "") << ". Frame=" << std::to_string(frame) << "\n";
    }

    //=== Fielder knockouts === Red plants and chomps knock out fielders that get too close. Nothing else knocks
    //fielders out in these stadiums while one is active, so a new knockout is attributed to the closest active one
    for (u8 pos = 0; pos < cRosterSize; ++pos){
        if (m_hazard_state.fielder_knockout[pos] != 0 || knockouts[pos] == 0) { continue; }

        float fielder_x = floatConverter(PowerPC::MMU::HostRead_U32(guard, aFielder_Pos_X + (pos * cFielder_Offset)));
        float fielder_z = floatConverter(PowerPC::MMU::HostRead_U32(guard, aFielder_Pos_Z + (pos * cFielder_Offset)));

        int closest = -1;
        float closest_dist = 0;
        for (u32 i = 0; i < obj_count; ++i){
            if (!objects[i].valid || !m_hazard_state.objects[i].valid) { continue; }
            HazardObjSnapshot& prev = m_hazard_state.objects[i];
            HazardObjSnapshot& now  = objects[i];
            float radius = 0;
            //The hazard may already be retreating this frame, so check last frame's state too
            if (now.kind == static_cast<u8>(HAZARD_TYPE::RED_PLANT)){
                if (now.aux != cPlantType_Red || !plantIsActive(prev)) { continue; }
                radius = cHazard_PlantKnockoutRadius;
            }
            else if (now.kind == static_cast<u8>(HAZARD_TYPE::CHOMP)){
                if (!chompIsHunting(prev) && !chompIsHunting(now) && now.state != cChompState_ReturningHome) { continue; }
                radius = cHazard_ChompAttributionRadius;
            }
            else if (now.kind == static_cast<u8>(HAZARD_TYPE::BARREL)){
                if (prev.state != cBarrelState_Rolling && now.state != cBarrelState_Rolling) { continue; }
                radius = cHazard_BarrelAttributionRadius;
            }
            else {
                continue;
            }
            float dist = xzDistance(fielder_x, fielder_z, now.x, now.z);
            if (dist < radius && (closest < 0 || dist < closest_dist)){
                closest = i;
                closest_dist = dist;
            }
        }
        if (closest < 0){
            std::cout << "Hazard: Fielder pos " << std::to_string(pos) << " knocked out but no active hazard nearby\n";
            continue;
        }

        HazardObjSnapshot& hazard = objects[closest];
        HazardEvent& hazard_event = addHazardEvent(in_contact, hazard.kind, hazard.slot,
                                                   static_cast<u8>(HAZARD_INTERACTION::FIELDER_CONTACT), getParentSequence(m_hazard_state.objects[closest]), frame);
        addFielderToHazardEvent(guard, hazard_event, pos);
        //Klaptraps riding the fielder are knocked off by the same hit. They can come loose a frame either side of it
        if (hazard.kind == static_cast<u8>(HAZARD_TYPE::BARREL)){
            m_hazard_state.objects[closest].fielder_event = in_contact->hazard_events.size() - 1;
            m_hazard_state.objects[closest].fielder_event_valid = true;
            setHazardDetail(hazard_event, "Detached Klaptraps", std::to_string(m_hazard_state.objects[closest].detached_klaptraps));
        }
        std::cout << "Hazard: " << decode("HazardType", hazard.kind, true) << " " << std::to_string(hazard.slot)
                  << " knocked out fielder pos " << std::to_string(pos) << ". Frame=" << std::to_string(frame) << "\n";
    }

    //=== Other bounces off a plant, star pad or rolling barrel === (red plant body, retracted plant, star pad with star skills off).
    //The game zeroes the bounce counter on the frame of a bounce and stores the surface type it hit
    if (frames_since_bounce == 0 && m_hazard_state.frames_since_last_bounce != 0){
        u8 collision_code = PowerPC::MMU::HostRead_U32(guard, aAB_BallCollisionCode) & 0x7F;
        std::cout << "Hazard: Ball bounce. Collision code=0x" << std::hex << static_cast<int>(collision_code) << std::dec << " Frame=" << std::to_string(frame) << "\n";
        if (cHazardBounceCollisionCodes.count(collision_code)){
            int closest = -1;
            float closest_dist = 0;
            for (u32 i = 0; i < obj_count; ++i){
                if (!objects[i].valid || ball_contact_logged[i]) { continue; }
                bool is_plant    = (objects[i].kind == static_cast<u8>(HAZARD_TYPE::RED_PLANT));
                bool is_star_pad = (objects[i].kind == static_cast<u8>(HAZARD_TYPE::STAR_PAD));
                bool is_barrel   = (objects[i].kind == static_cast<u8>(HAZARD_TYPE::BARREL) && objects[i].state == cBarrelState_Rolling);
                if (!is_plant && !is_star_pad && !is_barrel) { continue; }
                if (is_plant && objects[i].flag) { continue; } //Ball is in its mouth
                //Ignore the plant that just spat the ball out until the ball is on its way
                bool spitting = false;
                for (auto& pending : m_hazard_state.pending_velocity){
                    if (pending.obj_index == i && pending.wait_for_hold_countdown) { spitting = true; }
                }
                if (spitting) { continue; }

                float radius = is_plant ? cHazard_PlantBounceRadius : is_barrel ? cHazard_BarrelBounceRadius : cHazard_StarPadAttributionRadius;
                float dist = xzDistance(ball_x, ball_z, objects[i].x, objects[i].z);
                if (dist < radius && (closest < 0 || dist < closest_dist)){
                    closest = i;
                    closest_dist = dist;
                }
            }
            if (closest >= 0){
                u8 hazard_type = objects[closest].kind;
                if (hazard_type == static_cast<u8>(HAZARD_TYPE::RED_PLANT) && objects[closest].aux == cPlantType_Yellow){
                    hazard_type = static_cast<u8>(HAZARD_TYPE::YELLOW_PLANT);
                }
                HazardEvent& hazard_event = addHazardEvent(in_contact, hazard_type, objects[closest].slot,
                                                           static_cast<u8>(HAZARD_INTERACTION::BALL_CONTACT), getParentSequence(m_hazard_state.objects[closest]), frame);
                readBallPos(hazard_event);
                if (hazard_type == static_cast<u8>(HAZARD_TYPE::STAR_PAD)) { hazard_event.details.push_back({"Star Awarded", "false", ""}); }
                waitForVelocity(static_cast<u32>(closest), false);
                std::cout << "Hazard: Ball bounced off " << decode("HazardType", hazard_type, true) << " " << std::to_string(objects[closest].slot) << ". Frame=" << std::to_string(frame) << "\n";
            }
        }
    }

    //=== Store this frame as the baseline for the next one === A hazard's group ends once it is back to rest
    for (u32 i = 0; i < obj_count; ++i){
        HazardObjSnapshot& prev = m_hazard_state.objects[i];
        HazardObjSnapshot& now  = objects[i];
        if (!now.valid){
            prev = now;
            continue;
        }
        bool group_over = true;
        if (now.kind == static_cast<u8>(HAZARD_TYPE::RED_PLANT)) { group_over = (now.state == cPlantState_Idle); }
        else if (now.kind == static_cast<u8>(HAZARD_TYPE::CHOMP)) { group_over = (now.state <= cChompState_Awake); }
        else if (now.kind == static_cast<u8>(HAZARD_TYPE::TORNADO)) { group_over = (now.state == cTornadoState_Idle); }
        else if (now.kind == static_cast<u8>(HAZARD_TYPE::THWOMP)) { group_over = (now.state == cThwompState_Perched); }
        else if (now.kind == static_cast<u8>(HAZARD_TYPE::FIREBALL)) { group_over = (now.state == cFlameState_Idle); }
        else if (now.kind == static_cast<u8>(HAZARD_TYPE::BARREL)) { group_over = (now.state == cBarrelState_Idle); }
        else if (now.kind == static_cast<u8>(HAZARD_TYPE::KLAPTRAP)) { group_over = (now.state <= cKlaptrapState_Walking || now.state == cKlaptrapState_Despawning); }
        u16 parent_sequence = group_over ? 0 : prev.parent_sequence;
        std::array<u32, 3> aux_pos = prev.aux_pos;
        size_t aux_event = prev.aux_event;
        bool aux_event_valid = prev.aux_event_valid;
        size_t fielder_event = prev.fielder_event;
        bool fielder_event_valid = prev.fielder_event_valid && !group_over;
        u8 detached_klaptraps = group_over ? 0 : prev.detached_klaptraps;
        u8 ran_over_klaptraps = group_over ? 0 : prev.ran_over_klaptraps;
        prev = now;
        prev.parent_sequence = parent_sequence;
        prev.aux_pos = aux_pos;
        prev.aux_event = aux_event;
        prev.aux_event_valid = aux_event_valid;
        prev.fielder_event = fielder_event;
        prev.fielder_event_valid = fielder_event_valid;
        prev.detached_klaptraps = detached_klaptraps;
        prev.ran_over_klaptraps = ran_over_klaptraps;
    }
    m_hazard_state.fielder_knockout = knockouts;
    m_hazard_state.frames_since_last_bounce = frames_since_bounce;
    m_hazard_state.chomp_ball_marker = chomp_ball_marker;
    m_hazard_state.fielder_on_fire = on_fire;
    m_hazard_state.castle_hit_markers = castle_hit_markers;
    m_hazard_state.jungle_cannon_state = cannon_state;
    m_hazard_state.garden_hit_markers = garden_hit_markers;
}

//Writes the "Hazard Events" list for a contact. No trailing newline so the caller controls the separator
std::string StatTracker::getHazardEventsJSON(std::vector<HazardEvent>& in_events, std::string indent, bool inDecode){
    std::stringstream json_stream;
    std::string indent2 = indent + "  ";
    std::string indent3 = indent + "    ";
    std::string indent4 = indent + "      ";

    json_stream << indent << "\"Hazard Events\": [\n";
    for (size_t n = 0; n < in_events.size(); ++n){
        HazardEvent& hazard_event = in_events[n];
        json_stream << indent2 << "{\n";
        json_stream << indent3 << "\"Sequence\": "        << std::to_string(hazard_event.sequence) << ",\n";
        json_stream << indent3 << "\"Parent Sequence\": " << std::to_string(hazard_event.parent_sequence) << ",\n";
        json_stream << indent3 << "\"Hazard Type\": "     << decode("HazardType", hazard_event.hazard_type, inDecode) << ",\n";
        json_stream << indent3 << "\"Hazard ID\": "       << std::to_string(hazard_event.hazard_id) << ",\n";
        json_stream << indent3 << "\"Interaction\": "     << decode("HazardInteraction", hazard_event.interaction, inDecode) << ",\n";
        json_stream << indent3 << "\"Frame\": "           << std::to_string(hazard_event.frame) << ",\n";
        json_stream << indent3 << "\"Position - X\": "    << floatConverter(hazard_event.pos_x) << ",\n";
        json_stream << indent3 << "\"Position - Y\": "    << floatConverter(hazard_event.pos_y) << ",\n";
        json_stream << indent3 << "\"Position - Z\": "    << floatConverter(hazard_event.pos_z) << ",\n";
        if (hazard_event.result_velocity.has_value()){
            json_stream << indent3 << "\"Result Velocity - X\": " << floatConverter(hazard_event.result_velocity->at(0)) << ",\n";
            json_stream << indent3 << "\"Result Velocity - Y\": " << floatConverter(hazard_event.result_velocity->at(1)) << ",\n";
            json_stream << indent3 << "\"Result Velocity - Z\": " << floatConverter(hazard_event.result_velocity->at(2)) << ",\n";
        }
        if (hazard_event.target.has_value()){
            json_stream << indent3 << "\"Target - X\": " << floatConverter(hazard_event.target->at(0)) << ",\n";
            json_stream << indent3 << "\"Target - Y\": " << floatConverter(hazard_event.target->at(1)) << ",\n";
            json_stream << indent3 << "\"Target - Z\": " << floatConverter(hazard_event.target->at(2)) << ",\n";
        }
        if (hazard_event.fielder_roster_loc.has_value()){
            json_stream << indent3 << "\"Fielder Roster Loc\": " << std::to_string(hazard_event.fielder_roster_loc.value()) << ",\n";
        }
        if (hazard_event.fielder_pos.has_value()){
            json_stream << indent3 << "\"Fielder Position\": "   << decode("Position", hazard_event.fielder_pos.value(), inDecode) << ",\n";
        }
        if (hazard_event.fielder_char_id.has_value()){
            json_stream << indent3 << "\"Fielder Character\": "  << decode("Character", hazard_event.fielder_char_id.value(), inDecode) << ",\n";
        }
        //Details is always last so nothing above has to worry about trailing commas
        json_stream << indent3 << "\"Details\": {";
        for (size_t d = 0; d < hazard_event.details.size(); ++d){
            const HazardEvent::Detail& detail = hazard_event.details[d];
            std::string value = detail.decode_type.empty() ? detail.value : decode(detail.decode_type, static_cast<u8>(std::stoi(detail.value)), inDecode);
            json_stream << ((d == 0) ? "\n" : ",\n") << indent4 << "\"" << detail.key << "\": " << value;
        }
        if (!hazard_event.details.empty()) { json_stream << "\n" << indent3; }
        json_stream << "}\n";
        json_stream << indent2 << "}" << ((n + 1 < in_events.size()) ? "," : "") << "\n";
    }
    json_stream << indent << "]";
    return json_stream.str();
}

//Read players from ini file and assign to team
void StatTracker::readPlayerNames(bool local_game) {
  int team0_port = m_game_info.team0_port;
  int team1_port = m_game_info.team1_port;

  if (local_game)
  {
    // Player 1
    if (team0_port == 0)
        m_game_info.team0_player = LocalPlayers::m_local_player_1;
    else {
        m_game_info.team0_player = LocalPlayers::LocalPlayers::Player();
        m_game_info.team0_player.username = "CPU";
        m_game_info.team0_player.userid = "CPU";
    }

    // other player
    if (team1_port == 1)
        m_game_info.team1_player = LocalPlayers::m_local_player_2;
    else if (team1_port == 2)
        m_game_info.team1_player = LocalPlayers::m_local_player_3;
    else if (team1_port == 3)
        m_game_info.team1_player = LocalPlayers::m_local_player_4;
    else { // 4/5 are CPUs
        m_game_info.team1_player = LocalPlayers::LocalPlayers::Player();
        m_game_info.team1_player.username = "CPU";
        m_game_info.team1_player.userid = "CPU";
    }
  }

  else
  {
    m_game_info.team0_player = m_game_info.NetplayerUserInfo[team0_port + 1];
    m_game_info.team1_player = m_game_info.NetplayerUserInfo[team1_port + 1];
  }
}


void StatTracker::setTagSetId(Tag::TagSet tag_set, bool netplay) {
    std::cout << "TagSet Id=" << tag_set.id << "," << "TagSet Name=" << tag_set.name << "\n";
    netplay ? m_state.tag_set_id_netplay = tag_set.id : m_state.tag_set_id_local = tag_set.id;
}

void StatTracker::clearTagSetId(bool netplay) {
    std::cout << "Clearing TagSet" << "\n";
    netplay ? m_state.tag_set_id_netplay = std::nullopt : m_state.tag_set_id_local = std::nullopt;
}


bool StatTracker::shouldSubmitGame() {
    bool cpuInGame = (m_game_info.getAwayTeamPlayer().GetUserID() == "CPU") || (m_game_info.getHomeTeamPlayer().GetUserID() == "CPU");
    bool tag_set_game = m_game_info.tag_set_id.has_value();
    std::cout << "Checking game submission. TagSetSelected=" << tag_set_game << " cpuInGame=" << cpuInGame << "\n";

    return (!cpuInGame && tag_set_game);
}

void StatTracker::setNetplaySession(bool netplay_session, std::string opponent_name){
    m_state.m_netplay_session = netplay_session;
    m_state.m_netplay_opponent_alias = opponent_name;
}

void StatTracker::setAvgPing(int avgPing)
{
  //std::cout << "Avg Ping=" << avgPing << "\n";
  m_game_info.avg_ping = avgPing;
}

void StatTracker::setLagSpikes(int nLagSpikes)
{
  //std::cout << "Number of Lag Spikes=" << nLagSpikes << "\n";
  m_game_info.lag_spikes = nLagSpikes;
}
void StatTracker::setNetplayerUserInfo(std::map<int, LocalPlayers::LocalPlayers::Player> userInfo)
{
  for (auto player : userInfo)
    m_game_info.NetplayerUserInfo[player.first] = player.second;
}

void StatTracker::setGameID(u32 gameID)
{
  m_game_info.game_id = gameID;
}

void StatTracker::initPlayerInfo(const Core::CPUThreadGuard& guard){
    //Read start time
    std::time_t unix_time = std::time(nullptr);
    m_game_info.start_unix_date_time = std::to_string(unix_time);
    m_game_info.start_local_date_time = std::asctime(std::localtime(&unix_time));
    m_game_info.start_local_date_time.pop_back();

    m_game_info.first_batting_team = PowerPC::MMU::HostRead_U8(guard, aFirstBattingTeam);
    m_game_info.star_skills_on     = PowerPC::MMU::HostRead_U8(guard, aStarSkillsOn);
    m_game_info.mercy_on           = PowerPC::MMU::HostRead_U8(guard, aMercyOn);
    m_game_info.away_logo         = PowerPC::MMU::HostRead_U32(guard, aAway_Logo);
    m_game_info.home_logo         = PowerPC::MMU::HostRead_U32(guard, aHome_Logo);

    //Collect port info for players
    if (m_game_info.team0_port == 0xFF && m_game_info.team1_port == 0xFF){
        //From Roeming
        std::array<u8, 2> ports = {PowerPC::MMU::HostRead_U8(guard, aPlayer1Port), PowerPC::MMU::HostRead_U8(guard, aPlayer2Port)};
        
        u8 BattingPort = ports[PowerPC::MMU::HostRead_U32(guard, aBattingTeam_P1P2)];
        u8 FieldingPort = ports[PowerPC::MMU::HostRead_U32(guard, aFieldingTeam_P1P2)];
        
        m_game_info.team0_port = ports[0];
        m_game_info.team1_port = ports[1];

        //Need to acount for possibility to init during the bottom of the inning if the fast load mod is on.
        //Read half_inning from live memory here: initPlayerInfo runs before logEventState populates
        //the current event's half_inning, so the event field is still stale (default 0) at this point.
        //BattingPort/FieldingPort above are live reads, so half_inning must be too or away/home flip.
        u8 half_inning = PowerPC::MMU::HostRead_U8(guard, aAB_HalfInning);
        if (half_inning == 0)
        {
            m_game_info.home_port = FieldingPort;
            m_game_info.away_port = BattingPort;
        }
        else
        {
            m_game_info.home_port = BattingPort;
            m_game_info.away_port = FieldingPort;
        }

        readPlayerNames(!m_game_info.netplay);

        std::string away_player_name;
        std::string home_player_name;
        if (m_game_info.away_port == m_game_info.team0_port) {
            away_player_name = m_game_info.team0_player.GetUsername();
            home_player_name = m_game_info.team1_player.GetUsername();
        }
        else{
            away_player_name = m_game_info.team1_player.GetUsername();
            home_player_name = m_game_info.team0_player.GetUsername();
        }

        std::cout << "ports[0]=" << std::to_string(PowerPC::MMU::HostRead_U8(guard, aPlayer1Port)) << " ports[1]=" << std::to_string(PowerPC::MMU::HostRead_U8(guard, aPlayer2Port)) << "\n";
        std::cout << "BattingPort=" << std::to_string(PowerPC::MMU::HostRead_U32(guard, aBattingTeam_P1P2)) << " FieldingPort=" << std::to_string(PowerPC::MMU::HostRead_U32(guard, aFieldingTeam_P1P2)) << "\n";

        std::cout << "Info:  Fielder Port=" << std::to_string(FieldingPort) << ", Batter Port=" << std::to_string(BattingPort) << "\n";
        std::cout << "Info:  Team0 Port=" << std::to_string(m_game_info.team0_port) << ", Team1 Port=" << std::to_string(m_game_info.team1_port) << "\n";
        std::cout << "Info:  Away Port=" << std::to_string(m_game_info.away_port) << ", Home Port=" << std::to_string(m_game_info.home_port) << "\n";
        std::cout << "Info:  Away Player=" << (away_player_name) << ", Home Player=" << (home_player_name) << "\n";

        initCaptains(guard);
    }
}

void StatTracker::initCaptains(const Core::CPUThreadGuard& guard)
{
    m_game_info.team0_captain_roster_loc = PowerPC::MMU::HostRead_U8(guard, aTeam0_Captain_Roster_Loc);
    m_game_info.team1_captain_roster_loc = PowerPC::MMU::HostRead_U8(guard, aTeam1_Captain_Roster_Loc);

    u8 away_captain_roster_loc = (m_game_info.away_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;
    u8 home_captain_roster_loc = (m_game_info.home_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;

    std::cout << "Info:  Team0 Captain=" << std::to_string(m_game_info.team0_captain_roster_loc) << ", Home Captain=" << (std::to_string(m_game_info.team1_captain_roster_loc)) << "\n";
    std::cout << "Info:  Away Captain=" << std::to_string(away_captain_roster_loc) << ", Home Captain=" << (std::to_string(home_captain_roster_loc)) << "\n\n";
}

void StatTracker::onGameQuit(const Core::CPUThreadGuard& guard){
    u8 quitter_port = PowerPC::MMU::HostRead_U8(guard, aWhoQuit);
    m_game_info.quitter_team = (quitter_port == m_game_info.away_port);
    logGameInfo(guard);

    std::cout << "Quit detected\n";

    //Game has ended. Write file but do not submit
    std::string jsonPath = getStatJsonPath("quit.decode.");
    std::string json = getStatJSON(true);
    
    File::WriteStringToFile(jsonPath, json);

    jsonPath = getStatJsonPath("quit.");
    json = getStatJSON(false, true);
    
    File::WriteStringToFile(jsonPath, json);

    // json = getStatJSON(false, false);
    // if (shouldSubmitGame()) {
    //     const Common::HttpRequest::Response response =
    //     m_http.Post("https://api.projectrio.app/populate_db/", json,
    //         {
    //             {"Content-Type", "application/json"},
    //         }
    //     );
    // }
}

std::optional<StatTracker::Runner> StatTracker::logRunnerInfo(const Core::CPUThreadGuard& guard, u8 base){
    std::optional<Runner> runner;
    //See if there is a runner in this pos
    if (PowerPC::MMU::HostRead_U8(guard, aRunner_RosterLoc + (base * cRunner_Offset)) != 0xFF){
        Runner init_runner;
        init_runner.roster_loc = PowerPC::MMU::HostRead_U8(guard, aRunner_RosterLoc + (base * cRunner_Offset));
        init_runner.char_id = PowerPC::MMU::HostRead_U8(guard, aRunner_CharId + (base * cRunner_Offset));
        init_runner.initial_base = base;
        init_runner.basepath_location = PowerPC::MMU::HostRead_U32(guard, aRunner_BasepathPercentage + (base * cRunner_Offset));
        runner = std::make_optional(init_runner);
        return runner;        
    }
    return runner;
}

bool StatTracker::anyRunnerStealing(const Core::CPUThreadGuard& guard, Event& in_event)
{
    u8 runner_1_stealing = PowerPC::MMU::HostRead_U8(guard, aRunner_Stealing + (1 * cRunner_Offset));
    u8 runner_2_stealing = PowerPC::MMU::HostRead_U8(guard, aRunner_Stealing + (2 * cRunner_Offset));
    u8 runner_3_stealing = PowerPC::MMU::HostRead_U8(guard, aRunner_Stealing + (3 * cRunner_Offset));

    return (runner_1_stealing || runner_2_stealing || runner_3_stealing);
}

void StatTracker::logRunnerEvents(const Core::CPUThreadGuard& guard, Runner* in_runner){
    //Return if no runner
    if (in_runner->out_type != 0 ) { return; }

    //Return if runner has already gotten out, or the ball is dead due to HR, GRD, or Ball Dead.
    in_runner->out_type = PowerPC::MMU::HostRead_U8(guard, aRunner_OutType + (in_runner->initial_base * cRunner_Offset));
    u8 dead_ball_reason = PowerPC::MMU::HostRead_U8(guard, aAB_DeadBallReason);
    if (in_runner->out_type != 0) {
        in_runner->out_location = PowerPC::MMU::HostRead_U8(guard, aRunner_CurrentBase + (in_runner->initial_base * cRunner_Offset));
        in_runner->result_base = 0xFF;
        in_runner->basepath_location = PowerPC::MMU::HostRead_U32(guard, aRunner_BasepathPercentage + (in_runner->initial_base * cRunner_Offset));

        std::cout << "Logging Runner " << std::to_string(in_runner->initial_base) << ": Out. Type=" << std::to_string(in_runner->out_type)
        << " Location=" << std::to_string(in_runner->out_location) << "\n";
    }
    else if (dead_ball_reason == static_cast<u8>(DEAD_BALL_REASON::HOME_RUN))
        in_runner->result_base = 4;
    else if (dead_ball_reason == static_cast<u8>(DEAD_BALL_REASON::GROUND_RULE_DOUBLE))
        in_runner->result_base = std::min<u8>(in_runner->initial_base + 2, 4);
    else if (dead_ball_reason == static_cast<u8>(DEAD_BALL_REASON::BALL_DEAD))
        // techincally, ball dead is "base reached at time of the throw" + 2 bases.
        // For simplicity, we are assuming the current base == base reached at time of throw, since they should be very similar.
        in_runner->result_base = std::min<u8>(PowerPC::MMU::HostRead_U8(guard, aRunner_CurrentBase + (in_runner->initial_base * cRunner_Offset)) + 2, 4);
    else {
        in_runner->result_base = PowerPC::MMU::HostRead_U8(guard, aRunner_CurrentBase + (in_runner->initial_base * cRunner_Offset));
    }

    if (PowerPC::MMU::HostRead_U8(guard, aRunner_Stealing + (in_runner->initial_base * cRunner_Offset)) > in_runner->steal){
        in_runner->steal = PowerPC::MMU::HostRead_U8(guard, aRunner_Stealing + (in_runner->initial_base * cRunner_Offset));
        std::cout << "Logging Runner " << std::to_string(in_runner->initial_base) << ": Steal. Type=" << std::to_string(in_runner->steal)<< "\n";
    }
}

std::string StatTracker::decode(std::string type, u8 value, bool decode){
    if (!decode) { return std::to_string(value);}

    std::string retVal = "Unable to Decode";
    
    if (type == "Character"){
        if (cCharIdToCharName.count(value)){
            retVal = cCharIdToCharName.at(value);
        }
    }
    else if (type == "Stadium"){
        if (cStadiumIdToStadiumName.count(value)){
            retVal = cStadiumIdToStadiumName.at(value);
        }
    }
    else if (type == "Logo"){
        if (cLogoIdToTeamName.count(value)){
            retVal = cLogoIdToTeamName.at(value);
        }
    }
    else if (type == "Contact"){
        if (cTypeOfContactToHR.count(value)){
            retVal = cTypeOfContactToHR.at(value);
        }
    }
    else if (type == "Hand"){
        if (cHandToHR.count(value)){
            retVal = cHandToHR.at(value);
        }
    }
    else if (type == "Stick"){
        if (cInputDirectionToHR.count(value)){
            retVal = cInputDirectionToHR.at(value);
        }
    }
    else if (type == "StickVec"){
        retVal = "";
        if ( (value & 0x1) > 0 ){
            if (retVal != ""){
                retVal += "+";
            }
            retVal += "Left";
        }
        if ( (value & 0x2) > 0 ){
            if (retVal != ""){
                retVal += "+";
            }
            retVal += "Right";
        } 
        if ( (value & 0x4) > 0 ){
            if (retVal != ""){
                retVal += "+";
            }
            retVal += "Down";
        } 
        if ( (value & 0x8) > 0 ){
            if (retVal != ""){
                retVal += "+";
            }
            retVal += "Up";
        } 
    }
    else if (type == "Pitch"){
        if (cPitchTypeToHR.count(value)){
            retVal = cPitchTypeToHR.at(value);
        }
    }
    else if (type == "ChargePitch"){
        if (cChargePitchTypeToHR.count(value)){
            retVal = cChargePitchTypeToHR.at(value);
        }
    }
    else if (type == "Swing"){
        if (cTypeOfSwing.count(value)){
            retVal = cTypeOfSwing.at(value);
        }
    }
    else if (type == "Position"){
        if (cPosition.count(value)){
            retVal = cPosition.at(value);
        }
    }
    else if (type == "Action"){
        if (cFielderActions.count(value)){
            retVal = cFielderActions.at(value);
        }
    }
    else if (type == "Bobble"){
        if (cFielderBobbles.count(value)){
            retVal = cFielderBobbles.at(value);
        }
    }
    else if (type == "ManualSelect"){
        if (cManualSelectDecode.count(value)){
            retVal = cManualSelectDecode.at(value);
        }
    }
    else if (type == "Steal"){
        if (cStealType.count(value)){
            retVal = cStealType.at(value);
        }
    }
    else if (type == "Out"){
        if (cOutType.count(value)){
            retVal = cOutType.at(value);
        }
    }
    else if (type == "PrimaryContactResult"){
        if (cPrimaryContactResult.count(value)){
            retVal = cPrimaryContactResult.at(value);
        }
    }
    else if (type == "SecondaryContactResult"){
        if (cSecondaryContactResult.count(value)){
            retVal = cSecondaryContactResult.at(value);
        }
    }
    else if (type == "PitchResult"){
        if (cPitchResult.count(value)){
            retVal = cPitchResult.at(value);
        }
    }
    else if (type == "AtBatResult"){
        if (cAtBatResult.count(value)){
            retVal = cAtBatResult.at(value);
        }
    }
    else if (type == "DeadBallReason"){
        if (cDeadBallReason.count(value)){
            retVal = cDeadBallReason.at(value);
        }
    }
    else if (type == "QuitterTeam"){
        if (value == 0){
            retVal = "Home";
        }
        else if (value == 1){
            retVal = "Away";
        }
        else if (value == 2){
            retVal = "Crash";
        }
        else if (value == 0xFF){
            retVal = "None";
        }
    }
    else if (type == "HazardType"){
        if (cHazardType.count(value)){
            retVal = cHazardType.at(value);
        }
    }
    else if (type == "HazardInteraction"){
        if (cHazardInteraction.count(value)){
            retVal = cHazardInteraction.at(value);
        }
    }
    else{
        retVal += ". Invalid Type (" + type + ")";
    }
    
    if (retVal == "Unable to Decode"){
        retVal += ". Invalid Value (" + std::to_string(value) + ").";
    }
    return ("\"" + retVal + "\"");
}

void StatTracker::postOngoingGame(Event& in_curr_event){
    if (!shouldSubmitGame()){ return; }

    std::cout << "postOngoingGame()\n";

    std::stringstream json_stream;

    json_stream << "{\n";
    std::string start_date_time = m_game_info.start_unix_date_time;
    json_stream << "  \"GameID\": \"" << m_game_info.game_id << "\",\n";
    json_stream << "  \"Date - Start\": \"" << start_date_time << "\",\n";

    std::string tag_set_id_str = "-1";
    if (m_game_info.tag_set_id.has_value()){
        tag_set_id_str = std::to_string(m_game_info.tag_set_id.value());
    }
    json_stream << "  \"TagSetID\": " << tag_set_id_str << ",\n";
    json_stream << "  \"Loaded from HUD\": " << std::to_string(m_game_info.fastResetFromHUD) << ",\n";
    json_stream << "  \"StadiumID\": " << decode("Stadium", m_game_info.stadium, false) << ",\n";
    json_stream << "  \"Away Logo\": "  << decode("Logo", m_game_info.away_logo, false) << ",\n";
    json_stream << "  \"Home Logo\": "  << decode("Logo", m_game_info.home_logo, false) << ",\n";
    json_stream << "  \"Away Player\": \""  << m_game_info.getAwayTeamPlayer().GetUserID() << "\",\n";
    json_stream << "  \"Home Player\": \""  << m_game_info.getHomeTeamPlayer().GetUserID() << "\",\n";

    u8 away_captain_roster_loc = (m_game_info.away_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;
    u8 home_captain_roster_loc = (m_game_info.home_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;

    json_stream << "  \"Away Captain\": " << std::to_string(away_captain_roster_loc) << ",\n";
    json_stream << "  \"Home Captain\": " << std::to_string(home_captain_roster_loc) << ",\n";

    for (int team = 0; team < 2; ++team) {
        std::string team_str = (team == 0) ? "Away" : "Home";

        json_stream << "  \"" << team_str << " CharIDs\": [";
        for (int roster = 0; roster < 9; ++roster) {
            json_stream << std::to_string(m_game_info.character_summaries[team][roster].char_id);
            if (roster < 8) json_stream << ", ";
        }
        json_stream << "],\n";

        json_stream << "  \"" << team_str << " Fielding Positions\": [";
        for (int roster = 0; roster < 9; ++roster) {
            json_stream << decode("Position", m_fielder_tracker[team].fielder_map[roster].current_pos, false);
            if (roster < 8) json_stream << ", ";
        }
        json_stream << "],\n";

        json_stream << "  \"" << team_str << " Superstars\": [";
        for (int roster = 0; roster < 9; ++roster) {
            json_stream << std::to_string(m_game_info.character_summaries[team][roster].is_starred);
            if (roster < 8) json_stream << ", ";
        }
        json_stream << "],\n";
    }

    json_stream << "  \"Inning\": "       << std::to_string(in_curr_event.inning) << ",\n";
    json_stream << "  \"Half Inning\": "  << std::to_string(in_curr_event.half_inning) << ",\n";
    json_stream << "  \"Away Score\": "   << std::dec << in_curr_event.away_score << ",\n";
    json_stream << "  \"Home Score\": "   << std::dec << in_curr_event.home_score << ",\n";
    json_stream << "  \"Away Stars\": "   << std::to_string(in_curr_event.away_stars) << ",\n";
    json_stream << "  \"Home Stars\": "   << std::to_string(in_curr_event.home_stars) << ",\n";
    json_stream << "  \"Outs\": "         << std::to_string(in_curr_event.outs) << ",\n";

    json_stream << "  \"Away Inning Scores\": [";
    for (u8 i = 0; i < in_curr_event.inning && i < 18; ++i) {
        json_stream << in_curr_event.away_inning_scores[i];
        if (i < in_curr_event.inning - 1) json_stream << ", ";
    }
    json_stream << "],\n";

    u8 home_inning_limit = (in_curr_event.half_inning == 0) ? in_curr_event.inning - 1 : in_curr_event.inning;
    json_stream << "  \"Home Inning Scores\": [";
    for (u8 i = 0; i < home_inning_limit && i < 18; ++i) {
        json_stream << in_curr_event.home_inning_scores[i];
        if (i < home_inning_limit - 1) json_stream << ", ";
    }
    json_stream << "],\n";

    json_stream << "  \"Innings Selected\": " << std::to_string(m_game_info.innings_selected) << ",\n";
    json_stream << "  \"Star Chance\": "      << std::to_string(in_curr_event.is_star_chance) << ",\n";
    json_stream << "  \"Chemistry Links on Base\": " << std::to_string(in_curr_event.chem_links_ob) << ",\n";
    json_stream << "  \"Pitcher Roster Loc\": "  << std::to_string(in_curr_event.pitcher_roster_loc) << ",\n";
    json_stream << "  \"Batter Roster Loc\": "   << std::to_string(in_curr_event.batter_roster_loc) << ",\n";

    int pitcher_team = (in_curr_event.half_inning == 0) ? 1 : 0;
    int batter_team  = (in_curr_event.half_inning == 0) ? 0 : 1;

    u8 batter_hand = m_game_info.character_summaries[batter_team][in_curr_event.batter_roster_loc].batting_hand;
    json_stream << "  \"Batter Hand\": " << decode("Hand", batter_hand, false) << ",\n";

    bool runner_1 = (in_curr_event.runner_1.has_value());
    bool runner_2 = (in_curr_event.runner_2.has_value());
    bool runner_3 = (in_curr_event.runner_3.has_value());
    json_stream << "  \"Runner 1B Roster\": " << (runner_1 ? std::to_string(in_curr_event.runner_1.value().roster_loc) : "-1") << ",\n";
    json_stream << "  \"Runner 2B Roster\": " << (runner_2 ? std::to_string(in_curr_event.runner_2.value().roster_loc) : "-1") << ",\n";
    json_stream << "  \"Runner 3B Roster\": " << (runner_3 ? std::to_string(in_curr_event.runner_3.value().roster_loc) : "-1") << ",\n";

    EndGameRosterDefensiveStats& p_stat = m_game_info.character_summaries[pitcher_team][in_curr_event.pitcher_roster_loc].end_game_defensive_stats;
    json_stream << "  \"Pitcher Stats\": {\n";
    json_stream << "    \"Batters Faced\": "       << std::to_string(p_stat.batters_faced) << ",\n";
    json_stream << "    \"Runs Allowed\": "        << std::dec << p_stat.runs_allowed << ",\n";
    json_stream << "    \"Earned Runs\": "         << std::dec << p_stat.earned_runs << ",\n";
    json_stream << "    \"Batters Walked\": "      << p_stat.batters_walked << ",\n";
    json_stream << "    \"Batters Hit\": "         << p_stat.batters_hit << ",\n";
    json_stream << "    \"Hits Allowed\": "        << p_stat.hits_allowed << ",\n";
    json_stream << "    \"HRs Allowed\": "         << p_stat.homeruns_allowed << ",\n";
    json_stream << "    \"Pitches Thrown\": "      << p_stat.pitches_thrown << ",\n";
    json_stream << "    \"Stamina\": "             << p_stat.stamina << ",\n";
    json_stream << "    \"Strikeouts\": "          << std::to_string(p_stat.strike_outs) << ",\n";
    json_stream << "    \"Star Pitches Thrown\": " << std::to_string(p_stat.star_pitches_thrown) << ",\n";
    json_stream << "    \"Outs Pitched\": "        << std::to_string(p_stat.outs_pitched) << "\n";
    json_stream << "  },\n";

    EndGameRosterOffensiveStats& b_stat = m_game_info.character_summaries[batter_team][in_curr_event.batter_roster_loc].end_game_offensive_stats;
    json_stream << "  \"Batter Stats\": {\n";
    json_stream << "    \"At Bats\": "          << std::to_string(b_stat.at_bats) << ",\n";
    json_stream << "    \"Hits\": "             << std::to_string(b_stat.hits) << ",\n";
    json_stream << "    \"Singles\": "          << std::to_string(b_stat.singles) << ",\n";
    json_stream << "    \"Doubles\": "          << std::to_string(b_stat.doubles) << ",\n";
    json_stream << "    \"Triples\": "          << std::to_string(b_stat.triples) << ",\n";
    json_stream << "    \"Homeruns\": "         << std::to_string(b_stat.homeruns) << ",\n";
    json_stream << "    \"Successful Bunts\": " << std::to_string(b_stat.successful_bunts) << ",\n";
    json_stream << "    \"Sac Flys\": "         << std::to_string(b_stat.sac_flys) << ",\n";
    json_stream << "    \"Strikeouts\": "       << std::to_string(b_stat.strikouts) << ",\n";
    json_stream << "    \"Walks (4 Balls)\": "  << std::to_string(b_stat.walks_4balls) << ",\n";
    json_stream << "    \"Walks (Hit)\": "      << std::to_string(b_stat.walks_hit) << ",\n";
    json_stream << "    \"RBI\": "              << std::to_string(b_stat.rbi) << ",\n";
    json_stream << "    \"Bases Stolen\": "     << std::to_string(b_stat.bases_stolen) << ",\n";
    json_stream << "    \"Star Hits\": "        << std::to_string(b_stat.star_hits) << "\n";
    json_stream << "  }\n";
    json_stream << "}\n";

    // Hand the finished payload off to the background worker so the blocking POST
    // does not stall the emulation thread. The string is passed by value, so the
    // worker never touches game state the emulation thread is still mutating.
    m_ongoing_game_submitter.QueuePost(json_stream.str());
}
void StatTracker::updateOngoingGame(Event& in_curr_event){
    if (!shouldSubmitGame()){ return; }
    
    std::stringstream json_stream;

    json_stream << "{\n";
    json_stream << "  \"GameID\": \"" << m_game_info.game_id << "\",\n";
    json_stream << "  \"Inning\": "                  << std::to_string(in_curr_event.inning) << ",\n";
    json_stream << "  \"Half Inning\": "             << std::to_string(in_curr_event.half_inning) << ",\n";
    json_stream << "  \"Away Score\": "              << std::dec << in_curr_event.away_score << ",\n";
    json_stream << "  \"Home Score\": "              << std::dec << in_curr_event.home_score << ",\n";
    json_stream << "  \"Outs\": "                    << std::to_string(in_curr_event.outs) << ",\n";
    json_stream << "  \"Away Stars\": "              << std::to_string(in_curr_event.away_stars) << ",\n";
    json_stream << "  \"Home Stars\": "              << std::to_string(in_curr_event.home_stars) << ",\n";
    json_stream << "  \"Chemistry Links on Base\": " << std::to_string(in_curr_event.chem_links_ob) << ",\n";
    json_stream << "  \"Pitcher Roster Loc\": "      << std::to_string(in_curr_event.pitcher_roster_loc) << ",\n";
    json_stream << "  \"Batter Roster Loc\": "       << std::to_string(in_curr_event.batter_roster_loc) << ",\n";

    int batter_team_for_hand = (in_curr_event.half_inning == 0) ? 0 : 1;
    u8 batter_hand = m_game_info.character_summaries[batter_team_for_hand][in_curr_event.batter_roster_loc].batting_hand;
    json_stream << "  \"Batter Hand\": "  << decode("Hand", batter_hand, false) << ",\n";

    bool runner_1, runner_2, runner_3;
    runner_1 = (in_curr_event.runner_1.has_value());
    runner_2 = (in_curr_event.runner_2.has_value());
    runner_3 = (in_curr_event.runner_3.has_value());
    
    json_stream << "  \"Runner 1B Roster\": "   << (runner_1 ? std::to_string(in_curr_event.runner_1.value().roster_loc) : "-1") << ",\n";
    json_stream << "  \"Runner 2B Roster\": "   << (runner_2 ? std::to_string(in_curr_event.runner_2.value().roster_loc) : "-1") << ",\n";
    json_stream << "  \"Runner 3B Roster\": "   << (runner_3 ? std::to_string(in_curr_event.runner_3.value().roster_loc) : "-1") << ",\n";

    json_stream << "  \"Away Inning Scores\": [";
    for (u8 i = 0; i < in_curr_event.inning && i < 18; ++i) {
        json_stream << in_curr_event.away_inning_scores[i];
        if (i < in_curr_event.inning - 1) json_stream << ", ";
    }
    json_stream << "],\n";

    u8 home_inning_limit = (in_curr_event.half_inning == 0) ? in_curr_event.inning - 1 : in_curr_event.inning;
    json_stream << "  \"Home Inning Scores\": [";
    for (u8 i = 0; i < home_inning_limit && i < 18; ++i) {
        json_stream << in_curr_event.home_inning_scores[i];
        if (i < home_inning_limit - 1) json_stream << ", ";
    }
    json_stream << "],\n";

    json_stream << "  \"Star Chance\": "     << std::to_string(in_curr_event.is_star_chance) << ",\n";

    for (int team = 0; team < 2; ++team) {
        std::string team_str = (team == 0) ? "Away" : "Home";

        json_stream << "  \"" << team_str << " Fielding Positions\": [";
        for (int roster = 0; roster < 9; ++roster) {
            json_stream << decode("Position", m_fielder_tracker[team].fielder_map[roster].current_pos, false);
            if (roster < 8) json_stream << ", ";
        }
        json_stream << "],\n";
    }

    // Pitcher is on the team NOT batting: half_inning 0 = away bats, home pitches
    int pitcher_team = (in_curr_event.half_inning == 0) ? 1 : 0;
    int batter_team  = (in_curr_event.half_inning == 0) ? 0 : 1;

    EndGameRosterDefensiveStats& p_stat = m_game_info.character_summaries[pitcher_team][in_curr_event.pitcher_roster_loc].end_game_defensive_stats;
    json_stream << "  \"Pitcher Stats\": {\n";
    json_stream << "    \"Batters Faced\": "       << std::to_string(p_stat.batters_faced) << ",\n";
    json_stream << "    \"Runs Allowed\": "        << std::dec << p_stat.runs_allowed << ",\n";
    json_stream << "    \"Earned Runs\": "         << std::dec << p_stat.earned_runs << ",\n";
    json_stream << "    \"Batters Walked\": "      << p_stat.batters_walked << ",\n";
    json_stream << "    \"Batters Hit\": "         << p_stat.batters_hit << ",\n";
    json_stream << "    \"Hits Allowed\": "        << p_stat.hits_allowed << ",\n";
    json_stream << "    \"HRs Allowed\": "         << p_stat.homeruns_allowed << ",\n";
    json_stream << "    \"Pitches Thrown\": "      << p_stat.pitches_thrown << ",\n";
    json_stream << "    \"Stamina\": "             << p_stat.stamina << ",\n";
    json_stream << "    \"Strikeouts\": "          << std::to_string(p_stat.strike_outs) << ",\n";
    json_stream << "    \"Star Pitches Thrown\": " << std::to_string(p_stat.star_pitches_thrown) << ",\n";
    json_stream << "    \"Outs Pitched\": "        << std::to_string(p_stat.outs_pitched) << "\n";
    json_stream << "  },\n";

    EndGameRosterOffensiveStats& b_stat = m_game_info.character_summaries[batter_team][in_curr_event.batter_roster_loc].end_game_offensive_stats;
    json_stream << "  \"Batter Stats\": {\n";
    json_stream << "    \"At Bats\": "          << std::to_string(b_stat.at_bats) << ",\n";
    json_stream << "    \"Hits\": "             << std::to_string(b_stat.hits) << ",\n";
    json_stream << "    \"Singles\": "          << std::to_string(b_stat.singles) << ",\n";
    json_stream << "    \"Doubles\": "          << std::to_string(b_stat.doubles) << ",\n";
    json_stream << "    \"Triples\": "          << std::to_string(b_stat.triples) << ",\n";
    json_stream << "    \"Homeruns\": "         << std::to_string(b_stat.homeruns) << ",\n";
    json_stream << "    \"Successful Bunts\": " << std::to_string(b_stat.successful_bunts) << ",\n";
    json_stream << "    \"Sac Flys\": "         << std::to_string(b_stat.sac_flys) << ",\n";
    json_stream << "    \"Strikeouts\": "       << std::to_string(b_stat.strikouts) << ",\n";
    json_stream << "    \"Walks (4 Balls)\": "  << std::to_string(b_stat.walks_4balls) << ",\n";
    json_stream << "    \"Walks (Hit)\": "      << std::to_string(b_stat.walks_hit) << ",\n";
    json_stream << "    \"RBI\": "              << std::to_string(b_stat.rbi) << ",\n";
    json_stream << "    \"Bases Stolen\": "     << std::to_string(b_stat.bases_stolen) << ",\n";
    json_stream << "    \"Star Hits\": "        << std::to_string(b_stat.star_hits) << "\n";
    json_stream << "  }\n";
    json_stream << "}\n";

    // Hand the finished payload off to the background worker (see postOngoingGame).
    m_ongoing_game_submitter.QueueUpdate(json_stream.str());
}

StatTracker::OngoingGameSubmitter::~OngoingGameSubmitter()
{
    {
        std::lock_guard lg(m_lock);
        m_shutdown = true;
    }
    m_cv.notify_one();
    if (m_thread.joinable())
        m_thread.join();
}

void StatTracker::OngoingGameSubmitter::QueuePost(std::string payload)
{
    Enqueue(Item{Kind::Post, std::move(payload)});
}

void StatTracker::OngoingGameSubmitter::QueueUpdate(std::string payload)
{
    Enqueue(Item{Kind::Update, std::move(payload)});
}

void StatTracker::OngoingGameSubmitter::EnsureThreadStarted()
{
    // Lazily start the worker on first use so games that never submit (and the
    // brief windows where a StatTracker is constructed and torn down) pay nothing.
    // Caller must hold m_lock.
    if (!m_thread_started)
    {
        m_thread_started = true;
        m_thread = std::thread(&OngoingGameSubmitter::ThreadLoop, this);
    }
}

void StatTracker::OngoingGameSubmitter::Enqueue(Item&& item)
{
    std::lock_guard lg(m_lock);
    if (m_shutdown)
        return;

    // Coalesce: an update only supersedes an update that is still waiting at the
    // tail of the queue. The item the worker is currently sending has already been
    // popped, so it is never affected; a post at the tail is never overwritten or
    // reordered. This keeps a backlog from a slow request bounded to one pending
    // update while preserving post-before-update ordering.
    if (item.kind == Kind::Update && !m_items.empty() && m_items.back().kind == Kind::Update)
    {
        m_items.back().payload = std::move(item.payload);
    }
    else
    {
        m_items.push(std::move(item));
    }

    EnsureThreadStarted();
    m_cv.notify_one();
}

void StatTracker::OngoingGameSubmitter::ThreadLoop()
{
    Common::SetCurrentThreadName("RioOngoingGameQueue");

    while (true)
    {
        Item item;
        {
            std::unique_lock lg(m_lock);
            m_cv.wait(lg, [&] { return !m_items.empty() || m_shutdown; });

            // On shutdown, stop promptly: discard whatever is still queued instead of
            // draining it. A request already in flight (popped before m_shutdown was
            // set) finishes on its own, so teardown is bounded by a single request's
            // timeout rather than the whole backlog.
            if (m_shutdown)
                return;

            item = std::move(m_items.front());
            m_items.pop();
        }

        // Blocking POST happens here, off the emulation thread. m_http is owned and
        // touched only by this thread, so it is never used concurrently.
        m_http.Post(s_url, item.payload, {{"Content-Type", "application/json"}});
    }
}

bool StatTracker::hasEnoughStarsForStarSwing(const Core::CPUThreadGuard& guard, Event& in_event)
{
    u8 batter_stars = (in_event.half_inning == 0) ? in_event.away_stars : in_event.home_stars;

    u8 batter_roster_loc = in_event.batter_roster_loc;
    u8 batter_char_id = m_game_info.character_summaries[in_event.half_inning][batter_roster_loc].char_id;

    u8 captain_roster_loc = (in_event.half_inning == 0)
        ? ((m_game_info.away_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc)
        : ((m_game_info.home_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc);

    u8 star_cost;
    if (batter_roster_loc == captain_roster_loc) {
        star_cost = 1; // actual captain
    } else if (cCaptainTypeCharIds.count(batter_char_id)) {
        star_cost = 2; // captain-type but not the captain this game
    } else {
        star_cost = 1; // non-captain character
    }

    return batter_stars >= star_cost;
}