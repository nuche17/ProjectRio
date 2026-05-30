// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/MSBGameStateWidget.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "Core/MSB_Constants.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

static QComboBox* MakeCharCombo()
{
  auto* combo = new QComboBox;
  combo->addItem(QStringLiteral("— Not Set —"), QVariant());
  for (const auto& [id, name] : MSB::CHAR_LIST)
    combo->addItem(QString::fromUtf8(name), QVariant(static_cast<int>(id)));
  return combo;
}

static QComboBox* MakePosCombo()
{
  auto* combo = new QComboBox;
  combo->addItem(QStringLiteral("— Not Set —"), QVariant());
  for (uint8_t p = 0; p < 9; ++p)
    combo->addItem(QString::fromUtf8(MSB::POSITION_NAMES[p]), QVariant(static_cast<int>(p)));
  return combo;
}

static QComboBox* MakeHandCombo()
{
  auto* combo = new QComboBox;
  combo->addItem(QStringLiteral("—"), QVariant());
  combo->addItem(QStringLiteral("R"), QVariant(0));
  combo->addItem(QStringLiteral("L"), QVariant(1));
  return combo;
}

static void SetComboByData(QComboBox* combo, std::optional<uint8_t> opt)
{
  if (opt.has_value())
  {
    const int idx = combo->findData(QVariant(static_cast<int>(opt.value())));
    combo->setCurrentIndex(idx >= 0 ? idx : 0);
  }
  else
  {
    combo->setCurrentIndex(0);
  }
}

// ── Construction ──────────────────────────────────────────────────────────────

MSBGameStateWidget::MSBGameStateWidget(QWidget* parent)
    : QGroupBox(tr("Game State"), parent)
{
  CreateLayout();
  ConnectWidgets();
  UpdateEditability();
}

void MSBGameStateWidget::CreateLayout()
{
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(4, 8, 4, 4);
  outer->setSpacing(4);

  // ── Header row ──────────────────────────────────────────────────────────────
  m_enable_check = new QCheckBox(tr("Enable Fast Reset from State"));
  m_load_hud_btn = new QPushButton(tr("Load from HUD"));
  m_apply_btn    = new QPushButton(tr("Apply"));
  m_clear_btn    = new QPushButton(tr("Clear"));

  m_load_hud_btn->setToolTip(tr("Populate all fields from the latest HUD game state file."));
  m_apply_btn->setToolTip(tr("Send the current state to all clients and enable fast reset."));
  m_clear_btn->setToolTip(tr("Reset all fields and disable fast reset."));

  auto* header = new QHBoxLayout;
  header->addWidget(m_enable_check);
  header->addStretch();
  header->addWidget(m_load_hud_btn);
  header->addWidget(m_apply_btn);
  header->addWidget(m_clear_btn);
  outer->addLayout(header);

  auto* sep = new QFrame;
  sep->setFrameShape(QFrame::HLine);
  sep->setFrameShadow(QFrame::Sunken);
  outer->addWidget(sep);

  // ── Scroll area ─────────────────────────────────────────────────────────────
  auto* scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  outer->addWidget(scroll);

  m_scroll_content = new QWidget;
  scroll->setWidget(m_scroll_content);

  auto* content = new QVBoxLayout(m_scroll_content);
  content->setAlignment(Qt::AlignTop);
  content->setSpacing(6);

  CreatePreGameSection(content);
  CreateRosterSection(content);
  CreateInGameSection(content);

  content->addStretch();
}

void MSBGameStateWidget::CreatePreGameSection(QVBoxLayout* content)
{
  m_pregame_group = new QGroupBox(tr("Pre-Game Settings"));
  content->addWidget(m_pregame_group);

  auto* form = new QFormLayout(m_pregame_group);
  form->setLabelAlignment(Qt::AlignRight);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

  // Stadium — use MSB_Constants names
  m_stadium_combo = new QComboBox;
  m_stadium_combo->addItem(tr("— Not Set —"), QVariant());
  for (const auto& [id, name] : MSB::STADIUM_LIST)
    m_stadium_combo->addItem(QString::fromUtf8(name), QVariant(static_cast<int>(id)));
  form->addRow(tr("Stadium:"), m_stadium_combo);

  m_innings_combo = new QComboBox;
  m_innings_combo->addItem(tr("— Not Set —"), QVariant());
  for (int v : {1, 3, 5, 7, 9, 11, 13, 15, 17, 19})
    m_innings_combo->addItem(QString::number(v), QVariant(v));
  form->addRow(tr("Innings:"), m_innings_combo);

  m_first_batter_combo = new QComboBox;
  m_first_batter_combo->addItem(tr("P1"), QVariant(0));
  m_first_batter_combo->addItem(tr("P2"), QVariant(1));
  form->addRow(tr("First Batter:"), m_first_batter_combo);

  m_star_skills_combo = new QComboBox;
  m_star_skills_combo->addItem(tr("Off"), QVariant(0));
  m_star_skills_combo->addItem(tr("On"),  QVariant(1));
  form->addRow(tr("Star Skills:"), m_star_skills_combo);

  m_mercy_combo = new QComboBox;
  m_mercy_combo->addItem(tr("Off"), QVariant(0));
  m_mercy_combo->addItem(tr("On"),  QVariant(1));
  form->addRow(tr("Mercy Rule:"), m_mercy_combo);
}

void MSBGameStateWidget::CreateRosterSection(QVBoxLayout* content)
{
  m_roster_group = new QGroupBox(tr("Rosters"));
  content->addWidget(m_roster_group);

  auto* outer = new QVBoxLayout(m_roster_group);

  // Options row: which side is P1, and handedness toggle
  auto* opts = new QHBoxLayout;
  opts->addWidget(new QLabel(tr("P1 side (auto):")));
  m_p1_side_combo = new QComboBox;
  m_p1_side_combo->addItem(tr("Away"), QVariant(true));
  m_p1_side_combo->addItem(tr("Home"), QVariant(false));
  m_p1_side_combo->setEnabled(false);
  m_p1_side_combo->setToolTip(tr("Derived from First Batter and Half Inning."));
  opts->addWidget(m_p1_side_combo);
  opts->addStretch();
  m_show_hand_check = new QCheckBox(tr("Show Handedness"));
  opts->addWidget(m_show_hand_check);
  outer->addLayout(opts);

  // Away team
  auto* away_group = new QGroupBox(tr("Away Team"));
  auto* away_layout = new QVBoxLayout(away_group);
  m_away_captain_group = new QButtonGroup(this);
  m_away_table = CreateTeamTable(m_away_captain_group);
  away_layout->addWidget(m_away_table);
  outer->addWidget(away_group);

  // Home team (stacked below)
  auto* home_group = new QGroupBox(tr("Home Team"));
  auto* home_layout = new QVBoxLayout(home_group);
  m_home_captain_group = new QButtonGroup(this);
  m_home_table = CreateTeamTable(m_home_captain_group);
  home_layout->addWidget(m_home_table);
  outer->addWidget(home_group);

  // Hide hand columns until the checkbox is ticked
  for (QTableWidget* t : {m_away_table, m_home_table})
  {
    t->setColumnHidden(COL_BAT_HAND, true);
    t->setColumnHidden(COL_FLD_HAND, true);
  }
}

QTableWidget* MSBGameStateWidget::CreateTeamTable(QButtonGroup* captain_group)
{
  auto* table = new QTableWidget(9, COL_COUNT);
  table->setHorizontalHeaderLabels(
      {tr("Character"), tr("Position"), tr("Capt"), QString::fromUtf8("★"), tr("Bat"), tr("Fld")});
  table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  table->verticalHeader()->setDefaultSectionSize(24);
  table->horizontalHeader()->setStretchLastSection(false);
  table->horizontalHeader()->setSectionResizeMode(COL_CHARACTER, QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(COL_POSITION,  QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(COL_CAPTAIN,   QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(COL_SUPERSTAR, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(COL_BAT_HAND,  QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(COL_FLD_HAND,  QHeaderView::ResizeToContents);
  table->setSelectionMode(QAbstractItemView::NoSelection);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);

  // Row labels = batting order slots 1–9
  for (int row = 0; row < 9; ++row)
  {
    table->setVerticalHeaderItem(row, new QTableWidgetItem(QString::number(row + 1)));

    table->setCellWidget(row, COL_CHARACTER, MakeCharCombo());
    table->setCellWidget(row, COL_POSITION,  MakePosCombo());

    auto* capt = new QRadioButton;
    capt->setProperty("row", row);
    captain_group->addButton(capt, row);
    auto* capt_cell = new QWidget;
    auto* capt_lay  = new QHBoxLayout(capt_cell);
    capt_lay->setContentsMargins(0, 0, 0, 0);
    capt_lay->setAlignment(Qt::AlignCenter);
    capt_lay->addWidget(capt);
    table->setCellWidget(row, COL_CAPTAIN, capt_cell);

    auto* ss = new QCheckBox;
    auto* ss_cell = new QWidget;
    auto* ss_lay  = new QHBoxLayout(ss_cell);
    ss_lay->setContentsMargins(0, 0, 0, 0);
    ss_lay->setAlignment(Qt::AlignCenter);
    ss_lay->addWidget(ss);
    table->setCellWidget(row, COL_SUPERSTAR, ss_cell);

    table->setCellWidget(row, COL_BAT_HAND, MakeHandCombo());
    table->setCellWidget(row, COL_FLD_HAND, MakeHandCombo());
  }

  // Compact height: 9 rows * 24px + header
  table->setMinimumHeight(9 * 24 + table->horizontalHeader()->height() + 4);
  table->setMaximumHeight(table->minimumHeight());

  return table;
}

void MSBGameStateWidget::CreateInGameSection(QVBoxLayout* content)
{
  m_ingame_group = new QGroupBox(tr("In-Game State"));
  content->addWidget(m_ingame_group);

  auto* form = new QFormLayout(m_ingame_group);
  form->setLabelAlignment(Qt::AlignRight);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

  m_inning_combo = new QComboBox;
  for (int v = 1; v <= 18; ++v)
    m_inning_combo->addItem(QString::number(v), QVariant(v));
  form->addRow(tr("Inning:"), m_inning_combo);

  m_half_inning_combo = new QComboBox;
  m_half_inning_combo->addItem(tr("Top"),    QVariant(0));
  m_half_inning_combo->addItem(tr("Bottom"), QVariant(1));
  form->addRow(tr("Half:"), m_half_inning_combo);

  m_away_score_spin = new QSpinBox;
  m_away_score_spin->setRange(0, 255);
  form->addRow(tr("Away Score:"), m_away_score_spin);

  m_home_score_spin = new QSpinBox;
  m_home_score_spin->setRange(0, 255);
  form->addRow(tr("Home Score:"), m_home_score_spin);

  m_balls_combo = new QComboBox;
  for (int v = 0; v <= 3; ++v)
    m_balls_combo->addItem(QString::number(v), QVariant(v));
  form->addRow(tr("Balls:"), m_balls_combo);

  m_strikes_combo = new QComboBox;
  for (int v = 0; v <= 2; ++v)
    m_strikes_combo->addItem(QString::number(v), QVariant(v));
  form->addRow(tr("Strikes:"), m_strikes_combo);

  m_outs_combo = new QComboBox;
  for (int v = 0; v <= 2; ++v)
    m_outs_combo->addItem(QString::number(v), QVariant(v));
  form->addRow(tr("Outs:"), m_outs_combo);

  m_star_chance_combo = new QComboBox;
  m_star_chance_combo->addItem(tr("Off"), QVariant(0));
  m_star_chance_combo->addItem(tr("On"),  QVariant(1));
  form->addRow(tr("Star Chance:"), m_star_chance_combo);

  // Runners
  auto* runners_label = new QLabel(tr("Batting order slot of runners on base"));
  runners_label->setStyleSheet(QStringLiteral("font-style: italic;"));
  form->addRow(runners_label);

  static const char* BASE_LABELS[3] = {"1st Base:", "2nd Base:", "3rd Base:"};
  for (int base = 0; base < 3; ++base)
  {
    auto* row_w   = new QWidget;
    auto* row_lay = new QHBoxLayout(row_w);
    row_lay->setContentsMargins(0, 0, 0, 0);
    row_lay->setSpacing(4);

    m_runner_check[base] = new QCheckBox;
    row_lay->addWidget(m_runner_check[base]);

    m_runner_slot_combo[base] = new QComboBox;
    m_runner_slot_combo[base]->addItem(tr("—"), QVariant());
    for (int slot = 0; slot < 9; ++slot)
      m_runner_slot_combo[base]->addItem(QString::number(slot + 1), QVariant(slot));
    m_runner_slot_combo[base]->setEnabled(false);
    row_lay->addWidget(m_runner_slot_combo[base]);

    m_runner_char_label[base] = new QLabel(QStringLiteral("—"));
    m_runner_char_label[base]->setMinimumWidth(80);
    row_lay->addWidget(m_runner_char_label[base]);
    row_lay->addStretch();

    form->addRow(tr(BASE_LABELS[base]), row_w);
  }
}

void MSBGameStateWidget::UpdateRunnerLabels()
{
  // Top of inning = away team bats; m_away_table always holds the away team's roster
  const bool halfTop     = (m_half_inning_combo->currentData().toInt() == 0);
  QTableWidget* bat_tbl  = halfTop ? m_away_table : m_home_table;

  for (int base = 0; base < 3; ++base)
  {
    if (!m_runner_check[base]->isChecked())
    {
      m_runner_char_label[base]->setText(QStringLiteral("—"));
      continue;
    }
    const QVariant slot_v = m_runner_slot_combo[base]->currentData();
    if (slot_v.isNull())
    {
      m_runner_char_label[base]->setText(QStringLiteral("—"));
      continue;
    }
    const int slot        = slot_v.toInt();
    auto* char_combo      = static_cast<QComboBox*>(bat_tbl->cellWidget(slot, COL_CHARACTER));
    const QVariant char_v = char_combo ? char_combo->currentData() : QVariant();
    m_runner_char_label[base]->setText(char_v.isNull() ? QStringLiteral("—")
                                                       : char_combo->currentText());
  }
}

void MSBGameStateWidget::UpdateP1Side()
{
  const QVariant fb_v = m_first_batter_combo->currentData();
  if (fb_v.isNull())
    return;
  const bool firstBatterIsP1 = (fb_v.toInt() == 0);
  const bool halfTop          = (m_half_inning_combo->currentData().toInt() == 0);
  const bool p1IsAway         = (firstBatterIsP1 == halfTop);
  m_p1_side_combo->setCurrentIndex(p1IsAway ? 0 : 1);
}

// ── Signal wiring ─────────────────────────────────────────────────────────────

void MSBGameStateWidget::ConnectWidgets()
{
  connect(m_enable_check, &QCheckBox::stateChanged, this, [this](int) {
    UpdateEditability();
    if (m_enable_check->isChecked())
    {
      m_state_dirty = true;
    }
    else
    {
      Clear();
      m_state_dirty = false;
      emit ClearRequested();
    }
  });

  connect(m_load_hud_btn, &QPushButton::clicked, this, [this] {
    emit LoadFromHUDRequested();
  });

  connect(m_apply_btn, &QPushButton::clicked, this, [this] {
    m_state_dirty = false;
    emit ApplyRequested(BuildState());
  });

  connect(m_clear_btn, &QPushButton::clicked, this, [this] {
    Clear();
    m_state_dirty = false;
    emit ClearRequested();
  });

  connect(m_show_hand_check, &QCheckBox::stateChanged, this, [this](int state) {
    const bool show = (state == Qt::Checked);
    for (QTableWidget* t : {m_away_table, m_home_table})
    {
      t->setColumnHidden(COL_BAT_HAND, !show);
      t->setColumnHidden(COL_FLD_HAND, !show);
    }
  });

  // P1 side is derived from first batter + half inning
  connect(m_first_batter_combo, &QComboBox::currentIndexChanged,
          this, [this](int) { UpdateP1Side(); UpdateRunnerLabels(); });
  connect(m_half_inning_combo, &QComboBox::currentIndexChanged,
          this, [this](int) { UpdateP1Side(); UpdateRunnerLabels(); });

  // Runner label updates when runner checkbox/slot changes
  for (int base = 0; base < 3; ++base)
  {
    connect(m_runner_check[base], &QCheckBox::stateChanged, this, [this, base](int state) {
      m_runner_slot_combo[base]->setEnabled(state == Qt::Checked);
      UpdateRunnerLabels();
    });
    connect(m_runner_slot_combo[base], &QComboBox::currentIndexChanged,
            this, [this](int) { UpdateRunnerLabels(); });
  }

  // Runner label updates when any roster character changes
  for (QTableWidget* t : {m_away_table, m_home_table})
  {
    for (int row = 0; row < 9; ++row)
    {
      auto* char_combo = static_cast<QComboBox*>(t->cellWidget(row, COL_CHARACTER));
      connect(char_combo, &QComboBox::currentIndexChanged,
              this, [this](int) { UpdateRunnerLabels(); });
    }
  }

  // Mark dirty when any field value changes
  auto markDirty = [this] { m_state_dirty = true; };

  for (QComboBox* c : {m_stadium_combo, m_innings_combo, m_first_batter_combo,
                       m_star_skills_combo, m_mercy_combo, m_inning_combo,
                       m_half_inning_combo, m_balls_combo, m_strikes_combo,
                       m_outs_combo, m_star_chance_combo})
  {
    connect(c, &QComboBox::currentIndexChanged, this, markDirty);
  }
  connect(m_away_score_spin, &QSpinBox::valueChanged, this, markDirty);
  connect(m_home_score_spin, &QSpinBox::valueChanged, this, markDirty);

  for (int base = 0; base < 3; ++base)
  {
    connect(m_runner_check[base],    &QCheckBox::stateChanged,        this, markDirty);
    connect(m_runner_slot_combo[base], &QComboBox::currentIndexChanged, this, markDirty);
  }

  for (QTableWidget* t : {m_away_table, m_home_table})
  {
    for (int row = 0; row < 9; ++row)
    {
      connect(static_cast<QComboBox*>(t->cellWidget(row, COL_CHARACTER)),
              &QComboBox::currentIndexChanged, this, markDirty);
      connect(static_cast<QComboBox*>(t->cellWidget(row, COL_POSITION)),
              &QComboBox::currentIndexChanged, this, markDirty);
      connect(static_cast<QComboBox*>(t->cellWidget(row, COL_BAT_HAND)),
              &QComboBox::currentIndexChanged, this, markDirty);
      connect(static_cast<QComboBox*>(t->cellWidget(row, COL_FLD_HAND)),
              &QComboBox::currentIndexChanged, this, markDirty);
      auto* capt = t->cellWidget(row, COL_CAPTAIN)->findChild<QRadioButton*>();
      auto* ss   = t->cellWidget(row, COL_SUPERSTAR)->findChild<QCheckBox*>();
      connect(capt, &QRadioButton::toggled,    this, markDirty);
      connect(ss,   &QCheckBox::stateChanged,  this, markDirty);
    }
  }
}

void MSBGameStateWidget::UpdateEditability()
{
  const bool editable = m_enable_check->isChecked() && m_is_host;
  m_load_hud_btn->setEnabled(editable);
  m_apply_btn->setEnabled(editable);
  m_clear_btn->setEnabled(editable);
  m_scroll_content->setEnabled(editable);
  m_p1_side_combo->setEnabled(false);  // always read-only — derived from first batter + half inning
}

// ── Public interface ──────────────────────────────────────────────────────────

void MSBGameStateWidget::SetHostMode(bool is_host)
{
  m_is_host = is_host;
  m_enable_check->setEnabled(is_host);
  UpdateEditability();
}

bool MSBGameStateWidget::IsEnabled() const
{
  return m_enable_check->isChecked();
}

bool MSBGameStateWidget::IsDirty() const
{
  return m_state_dirty;
}

void MSBGameStateWidget::ApplyNow()
{
  m_state_dirty = false;
  emit ApplyRequested(BuildState());
}

void MSBGameStateWidget::Clear()
{
  // Pre-game — stadium and innings have no default (Not Set)
  m_stadium_combo->setCurrentIndex(0);
  m_innings_combo->setCurrentIndex(0);
  // First batter, star skills, mercy always have a value — default P2 / On / On
  m_first_batter_combo->setCurrentIndex(m_first_batter_combo->findData(QVariant(1)));
  m_star_skills_combo->setCurrentIndex(m_star_skills_combo->findData(QVariant(1)));
  m_mercy_combo->setCurrentIndex(m_mercy_combo->findData(QVariant(1)));

  // In-game (reset to clean-start defaults)
  m_inning_combo->setCurrentIndex(0);        // Inning 1
  m_half_inning_combo->setCurrentIndex(0);   // Top
  m_away_score_spin->setValue(0);
  m_home_score_spin->setValue(0);
  m_balls_combo->setCurrentIndex(0);
  m_strikes_combo->setCurrentIndex(0);
  m_outs_combo->setCurrentIndex(0);
  m_star_chance_combo->setCurrentIndex(0);   // Off
  for (int base = 0; base < 3; ++base)
  {
    m_runner_check[base]->setChecked(false);
    m_runner_slot_combo[base]->setCurrentIndex(0);
    m_runner_slot_combo[base]->setEnabled(false);
  }
  UpdateRunnerLabels();
  UpdateP1Side();

  // Rosters
  m_p1_side_combo->setCurrentIndex(0);  // will be overwritten by UpdateP1Side above
  for (QTableWidget* table : {m_away_table, m_home_table})
  {
    for (int row = 0; row < 9; ++row)
    {
      static_cast<QComboBox*>(table->cellWidget(row, COL_CHARACTER))->setCurrentIndex(0);
      static_cast<QComboBox*>(table->cellWidget(row, COL_POSITION))->setCurrentIndex(0);
      static_cast<QComboBox*>(table->cellWidget(row, COL_BAT_HAND))->setCurrentIndex(0);
      static_cast<QComboBox*>(table->cellWidget(row, COL_FLD_HAND))->setCurrentIndex(0);

      // Uncheck captain and superstar
      auto* capt_cell = table->cellWidget(row, COL_CAPTAIN);
      auto* capt = capt_cell->findChild<QRadioButton*>();
      if (capt) capt->setAutoExclusive(false), capt->setChecked(false), capt->setAutoExclusive(true);

      auto* ss_cell = table->cellWidget(row, COL_SUPERSTAR);
      auto* ss = ss_cell->findChild<QCheckBox*>();
      if (ss) ss->setChecked(false);
    }
  }
}

// ── PopulateFromState ─────────────────────────────────────────────────────────

void MSBGameStateWidget::PopulateFromState(const MSB_QuickMatchState& state)
{
  // Pre-game
  SetComboByData(m_stadium_combo,      state.GetStadium());
  SetComboByData(m_innings_combo,      state.GetInningsSelected());
  SetComboByData(m_first_batter_combo, state.GetFirstBatter().value_or(1));   // default P2
  SetComboByData(m_star_skills_combo,  state.GetStarSkills().value_or(1));    // default On
  SetComboByData(m_mercy_combo,        state.GetMercy().value_or(1));         // default On

  // P1 side
  const bool p1IsAway = state.GetP1IsAway();
  m_p1_side_combo->setCurrentIndex(p1IsAway ? 0 : 1);

  // Rosters — away/home tables hold the away/home *team*, not necessarily P1/P2
  const MSB_Team& awayTeam = p1IsAway ? state.GetP1() : state.GetP2();
  const MSB_Team& homeTeam = p1IsAway ? state.GetP2() : state.GetP1();
  PopulateTeamFromState(awayTeam, m_away_table, m_away_captain_group);
  PopulateTeamFromState(homeTeam, m_home_table, m_home_captain_group);

  // In-game
  auto setComboFromInt = [](QComboBox* combo, int val) {
    const int idx = combo->findData(QVariant(val));
    combo->setCurrentIndex(idx >= 0 ? idx : 0);
  };

  setComboFromInt(m_inning_combo,      static_cast<int>(state.GetInning().value_or(1)));
  setComboFromInt(m_half_inning_combo, static_cast<int>(state.GetHalfInning().value_or(0)));
  m_away_score_spin->setValue(static_cast<int>(state.GetAwayScore().value_or(0)));
  m_home_score_spin->setValue(static_cast<int>(state.GetHomeScore().value_or(0)));
  setComboFromInt(m_balls_combo,       static_cast<int>(state.GetBalls().value_or(0)));
  setComboFromInt(m_strikes_combo,     static_cast<int>(state.GetStrikes().value_or(0)));
  setComboFromInt(m_outs_combo,        static_cast<int>(state.GetOuts().value_or(0)));
  setComboFromInt(m_star_chance_combo, static_cast<int>(state.GetIsStarChance().value_or(0)));

  // Runners — resolve fielding position → batting slot via the batting team
  const bool halfTop = (state.GetHalfInning().value_or(0) == 0);
  const MSB_Team& battingTeam = (halfTop == p1IsAway) ? state.GetP1() : state.GetP2();
  for (int base = 0; base < 3; ++base)
  {
    const auto fieldingPos = state.GetRunnerFieldingPosition(base);
    if (!fieldingPos.has_value())
    {
      m_runner_check[base]->setChecked(false);
      m_runner_slot_combo[base]->setCurrentIndex(0);
      m_runner_slot_combo[base]->setEnabled(false);
      continue;
    }
    const MSB_Player* p = battingTeam.GetPlayer(static_cast<uint8_t>(fieldingPos.value()));
    if (!p || !p->battingOrderSlot.has_value())
    {
      m_runner_check[base]->setChecked(false);
      m_runner_slot_combo[base]->setCurrentIndex(0);
      m_runner_slot_combo[base]->setEnabled(false);
      continue;
    }
    m_runner_check[base]->setChecked(true);
    m_runner_slot_combo[base]->setEnabled(true);
    setComboFromInt(m_runner_slot_combo[base], static_cast<int>(p->battingOrderSlot.value()));
  }
  UpdateRunnerLabels();
  UpdateP1Side();
}

void MSBGameStateWidget::PopulateTeamFromState(const MSB_Team& team, QTableWidget* table,
                                               QButtonGroup* captain_group) const
{
  const auto captainSlot = team.GetCaptainBattingSlot();

  for (int slot = 0; slot < 9; ++slot)
  {
    const MSB_Player* p = team.GetPlayerByBattingSlot(static_cast<uint8_t>(slot));

    auto* char_combo = static_cast<QComboBox*>(table->cellWidget(slot, COL_CHARACTER));
    auto* pos_combo  = static_cast<QComboBox*>(table->cellWidget(slot, COL_POSITION));
    auto* bat_combo  = static_cast<QComboBox*>(table->cellWidget(slot, COL_BAT_HAND));
    auto* fld_combo  = static_cast<QComboBox*>(table->cellWidget(slot, COL_FLD_HAND));
    auto* capt_cell  = table->cellWidget(slot, COL_CAPTAIN);
    auto* ss_cell    = table->cellWidget(slot, COL_SUPERSTAR);
    auto* capt       = capt_cell  ? capt_cell->findChild<QRadioButton*>()  : nullptr;
    auto* ss         = ss_cell    ? ss_cell->findChild<QCheckBox*>()        : nullptr;

    if (p && p->IsSet())
    {
      SetComboByData(char_combo, p->charID);
      SetComboByData(pos_combo,  p->position);
      SetComboByData(bat_combo,  p->battingHand);
      SetComboByData(fld_combo,  p->fieldingHand);
      if (ss) ss->setChecked(p->superstar.value_or(0) == 1);
    }
    else
    {
      char_combo->setCurrentIndex(0);
      pos_combo->setCurrentIndex(0);
      bat_combo->setCurrentIndex(0);
      fld_combo->setCurrentIndex(0);
      if (ss) ss->setChecked(false);
    }

    if (capt)
    {
      capt->setAutoExclusive(false);
      capt->setChecked(captainSlot.has_value() && captainSlot.value() == slot);
      capt->setAutoExclusive(true);
    }
  }
}

// ── BuildState ────────────────────────────────────────────────────────────────

MSB_QuickMatchState MSBGameStateWidget::BuildState() const
{
  MSB_QuickMatchState state;

  // Pre-game
  auto applyU8 = [&](QComboBox* combo, auto setter) {
    const QVariant v = combo->currentData();
    if (!v.isNull()) (state.*setter)(static_cast<uint8_t>(v.toInt()));
  };
  applyU8(m_stadium_combo,      &MSB_QuickMatchState::SetStadium);
  applyU8(m_innings_combo,      &MSB_QuickMatchState::SetInningsSelected);
  applyU8(m_first_batter_combo, &MSB_QuickMatchState::SetFirstBatter);
  applyU8(m_star_skills_combo,  &MSB_QuickMatchState::SetStarSkills);
  applyU8(m_mercy_combo,        &MSB_QuickMatchState::SetMercy);

  // P1 side
  const bool p1IsAway = m_p1_side_combo->currentData().toBool();
  state.SetP1IsAway(p1IsAway);

  MSB_Team awayTeam = BuildTeamFromTable(m_away_table, m_away_captain_group);
  MSB_Team homeTeam = BuildTeamFromTable(m_home_table, m_home_captain_group);

  if (p1IsAway)
  {
    state.SetP1(awayTeam);
    state.SetP2(homeTeam);
  }
  else
  {
    state.SetP1(homeTeam);
    state.SetP2(awayTeam);
  }

  // In-game (always written — all fields have defaults)
  state.SetInning(static_cast<uint32_t>(m_inning_combo->currentData().toInt()));
  state.SetHalfInning(static_cast<uint8_t>(m_half_inning_combo->currentData().toInt()));
  state.SetAwayScore(static_cast<uint16_t>(m_away_score_spin->value()));
  state.SetHomeScore(static_cast<uint16_t>(m_home_score_spin->value()));
  state.SetBalls(static_cast<uint32_t>(m_balls_combo->currentData().toInt()));
  state.SetStrikes(static_cast<uint32_t>(m_strikes_combo->currentData().toInt()));
  state.SetOuts(static_cast<uint32_t>(m_outs_combo->currentData().toInt()));
  state.SetIsStarChance(static_cast<uint8_t>(m_star_chance_combo->currentData().toInt()));

  // Runners — batting team is whichever side is up this half inning
  const bool halfTop = (m_half_inning_combo->currentData().toInt() == 0);
  const bool useP1   = (halfTop == p1IsAway);  // top=away bats; p1IsAway → P1 is away
  for (int base = 0; base < 3; ++base)
  {
    if (!m_runner_check[base]->isChecked()) continue;
    const QVariant slot_v = m_runner_slot_combo[base]->currentData();
    if (slot_v.isNull()) continue;
    state.SetRunner(base, static_cast<uint8_t>(slot_v.toInt()), useP1);
  }

  return state;
}

MSB_Team MSBGameStateWidget::BuildTeamFromTable(QTableWidget* table,
                                                const QButtonGroup* captain_group) const
{
  MSB_Team team;
  const int captain_slot = captain_group->checkedId();  // -1 if none checked

  for (int slot = 0; slot < 9; ++slot)
  {
    auto* char_combo = static_cast<QComboBox*>(table->cellWidget(slot, COL_CHARACTER));
    const QVariant char_v = char_combo->currentData();
    if (char_v.isNull()) continue;  // row not set, skip

    auto* pos_combo  = static_cast<QComboBox*>(table->cellWidget(slot, COL_POSITION));
    auto* bat_combo  = static_cast<QComboBox*>(table->cellWidget(slot, COL_BAT_HAND));
    auto* fld_combo  = static_cast<QComboBox*>(table->cellWidget(slot, COL_FLD_HAND));
    auto* ss_cell    = table->cellWidget(slot, COL_SUPERSTAR);
    auto* ss         = ss_cell ? ss_cell->findChild<QCheckBox*>() : nullptr;

    const QVariant pos_v  = pos_combo->currentData();
    if (pos_v.isNull()) continue;  // position required alongside character

    const uint8_t charID   = static_cast<uint8_t>(char_v.toInt());
    const uint8_t position = static_cast<uint8_t>(pos_v.toInt());
    const uint8_t batHand  = bat_combo->currentData().isNull() ? 0
                             : static_cast<uint8_t>(bat_combo->currentData().toInt());
    const uint8_t fldHand  = fld_combo->currentData().isNull() ? 0
                             : static_cast<uint8_t>(fld_combo->currentData().toInt());
    const uint8_t superstar = (ss && ss->isChecked()) ? 1 : 0;

    MSB_Player player(charID, position, static_cast<uint8_t>(slot),
                      batHand, fldHand, superstar);
    team.SetPlayer(position, player);
  }

  if (captain_slot >= 0)
    team.SetCaptainBattingSlot(static_cast<uint8_t>(captain_slot));

  return team;
}
