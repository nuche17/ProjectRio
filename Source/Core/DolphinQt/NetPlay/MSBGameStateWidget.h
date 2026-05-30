// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include <QButtonGroup>
#include <QGroupBox>

#include "Core/MSB_GenerateQuickMatchSetupGeckoCode.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QScrollArea;
class QSpinBox;
class QTableWidget;
class QVBoxLayout;
class QWidget;

class MSBGameStateWidget : public QGroupBox
{
  Q_OBJECT
public:
  explicit MSBGameStateWidget(QWidget* parent = nullptr);

  void SetHostMode(bool is_host);
  void PopulateFromState(const MSB_QuickMatchState& state);
  void Clear();
  bool IsEnabled() const;
  bool IsDirty() const;
  void ApplyNow();
  MSB_QuickMatchState BuildState() const;

signals:
  void ApplyRequested(const MSB_QuickMatchState& state);
  void ClearRequested();
  void LoadFromHUDRequested();

private:
  void CreateLayout();
  void CreatePreGameSection(QVBoxLayout* content_layout);
  void CreateRosterSection(QVBoxLayout* content_layout);
  QTableWidget* CreateTeamTable(QButtonGroup* captain_group);
  void CreateInGameSection(QVBoxLayout* content_layout);
  void ConnectWidgets();
  void UpdateEditability();
  void UpdateRunnerLabels();
  void UpdateP1Side();

  // Helpers for BuildState / PopulateFromState
  void PopulateTeamFromState(const MSB_Team& team, QTableWidget* table,
                             QButtonGroup* captain_group) const;
  MSB_Team BuildTeamFromTable(QTableWidget* table,
                              const QButtonGroup* captain_group) const;

  // Header
  QCheckBox*   m_enable_check;
  QPushButton* m_load_hud_btn;
  QPushButton* m_apply_btn;
  QPushButton* m_clear_btn;

  // Pre-game
  QGroupBox* m_pregame_group;
  QComboBox* m_stadium_combo;
  QComboBox* m_innings_combo;
  QComboBox* m_first_batter_combo;
  QComboBox* m_star_skills_combo;
  QComboBox* m_mercy_combo;

  // Rosters
  QGroupBox*    m_roster_group;
  QComboBox*    m_p1_side_combo;   // which side P1 is on: Away or Home
  QCheckBox*    m_show_hand_check; // reveals batting/fielding hand columns
  QTableWidget* m_away_table;
  QTableWidget* m_home_table;
  QButtonGroup* m_away_captain_group;
  QButtonGroup* m_home_captain_group;

  // In-Game State
  QGroupBox*  m_ingame_group;
  QComboBox*  m_inning_combo;
  QComboBox*  m_half_inning_combo;
  QSpinBox*   m_away_score_spin;
  QSpinBox*   m_home_score_spin;
  QComboBox*  m_balls_combo;
  QComboBox*  m_strikes_combo;
  QComboBox*  m_outs_combo;
  QComboBox*  m_star_chance_combo;
  std::array<QCheckBox*, 3> m_runner_check;
  std::array<QComboBox*, 3> m_runner_slot_combo;
  std::array<QLabel*, 3>    m_runner_char_label;

  // Scroll content
  QWidget* m_scroll_content;

  bool m_is_host    = false;
  bool m_state_dirty = false;

  // Column indices for the roster table
  enum Col
  {
    COL_CHARACTER = 0,
    COL_POSITION  = 1,
    COL_CAPTAIN   = 2,
    COL_SUPERSTAR = 3,
    COL_BAT_HAND  = 4,
    COL_FLD_HAND  = 5,
    COL_COUNT     = 6,
  };
};