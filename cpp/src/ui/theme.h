#pragma once

#include <QString>

inline const QString kAppStyle = QStringLiteral(R"(
QWidget {
    color: #E8F0F5;
    font-family: "Microsoft YaHei UI", "Segoe UI Variable", sans-serif;
    font-size: 13px;
}
QMainWindow, QWidget#managerRoot { background: #0C151D; }
QLabel#brand {
    color: #7FC9EA;
    font-size: 12px;
    font-weight: 700;
    letter-spacing: 3px;
}
QLabel#title { color: #F4F8FA; font-size: 25px; font-weight: 650; }
QLabel#sectionTitle, QLabel#softwareOptionsTitle, QLabel#presetSectionTitle {
    color: #F4F8FA;
    font-size: 19px;
    font-weight: 650;
}
QLabel#muted, QLabel#count, QLabel#source { color: #8FA5B2; }
QLabel#redPixelPercentage { color: #FF8D8D; }
QLabel#whitePixelPercentage { color: #E8F0F5; }
QPushButton#createButton {
    background: #1782AD;
    color: white;
    border: 1px solid #35A5D0;
    border-radius: 7px;
    padding: 11px 18px;
    font-size: 14px;
    font-weight: 650;
}
QPushButton#createButton:hover { background: #2094BF; }
QPushButton#createButton:pressed { background: #126D92; }
QPushButton#createButton:focus { border: 2px solid #A9E6FF; }
QPushButton#closeAllButton {
    background: #40252A;
    color: #FFC9C9;
    border: 1px solid #A95B61;
    border-radius: 7px;
    padding: 11px 18px;
    font-size: 14px;
    font-weight: 650;
}
QPushButton#closeAllButton:hover { background: #51303A; }
QPushButton#closeAllButton:pressed { background: #331C22; }
QWidget#bottomBar {
    border-top: 1px solid #263D49;
    background: transparent;
}
QPushButton#forceQuitButton {
    background: #4A1E24;
    color: #FFC9C9;
    border: 1px solid #C05B63;
    border-radius: 6px;
    padding: 8px 22px;
    font-size: 13px;
    font-weight: 700;
}
QPushButton#forceQuitButton:hover { background: #5C2730; }
QPushButton#forceQuitButton:pressed { background: #3A161B; }
QPushButton {
    background: #152631;
    color: #C9D7DE;
    border: 1px solid #344B56;
    border-radius: 5px;
    padding: 7px 12px;
}
QPushButton:hover { background: #1B3442; border-color: #4D849C; color: #E8F0F5; }
QPushButton:pressed { background: #10202A; }
QPushButton:disabled { color: #60747F; background: #101C24; border-color: #263944; }
QFrame#softwareOptionsPanel {
    background: #101E27;
    border: 1px solid #2B4654;
    border-left: 3px solid #35A5D0;
    border-radius: 7px;
}
QFrame#softwareOptionsPanel QDoubleSpinBox {
    background: #0B171F;
    color: #F4F8FA;
    border: 1px solid #36505D;
    border-radius: 5px;
    padding: 5px 7px;
    min-width: 68px;
}
QFrame#softwareOptionsPanel QDoubleSpinBox:focus { border-color: #5AB6D9; }
QFrame#softwareOptionsPanel QDoubleSpinBox:disabled {
    color: #60747F;
    background: #0E1820;
    border-color: #263944;
}
QPushButton[accent="true"] { background: #176F94; border-color: #35A5D0; color: white; font-weight: 650; }
QPushButton[accent="true"]:hover { background: #2086AD; }
QGroupBox#expeditionOptionsPanel, QGroupBox#expeditionTimePanel,
QGroupBox#pagePartitionSettingsPanel {
    background: #101E27;
    border: 1px solid #2B4654;
    border-radius: 7px;
    margin-top: 10px;
    padding-top: 4px;
}
QFrame#desktopPetEditorPanel {
    background: #101E27;
    border: 1px solid #2B4654;
    border-radius: 8px;
}
QLabel#petSectionTitle {
    color: #F4F8FA;
    font-size: 16px;
    font-weight: 650;
}
QLabel#desktopPetImageName {
    color: #8FA5B2;
}
QLabel#desktopPetZoomValue, QLabel#desktopPetSizeValue {
    color: #A9D4E8;
    font-family: "Cascadia Mono", "Microsoft YaHei UI", monospace;
    font-weight: 650;
}
QLabel#desktopPetStatus {
    color: #A9D4E8;
    background: #132B37;
    border-left: 3px solid #62C4E8;
    border-radius: 4px;
    padding: 8px 10px;
}
QRadioButton#desktopPetCircleCrop, QRadioButton#desktopPetSquareCrop {
    color: #C9D7DE;
    spacing: 7px;
    padding: 4px 8px;
}
QRadioButton#desktopPetCircleCrop::indicator,
QRadioButton#desktopPetSquareCrop::indicator {
    width: 15px;
    height: 15px;
}
QGroupBox#expeditionOptionsPanel::title, QGroupBox#expeditionTimePanel::title,
QGroupBox#pagePartitionSettingsPanel::title {
    color: #8DD5F2;
    subcontrol-origin: margin;
    left: 12px;
    padding: 0 5px;
    font-weight: 650;
}
QGroupBox#expeditionTimePanel QDateTimeEdit,
QGroupBox#expeditionTimePanel QSpinBox,
QGroupBox#pagePartitionSettingsPanel QSpinBox {
    background: #0B171F;
    color: #F4F8FA;
    border: 1px solid #36505D;
    border-radius: 5px;
    padding: 6px 8px;
    selection-background-color: #1D6886;
}
QGroupBox#expeditionTimePanel QDateTimeEdit:focus,
QGroupBox#expeditionTimePanel QSpinBox:focus { border-color: #5AB6D9; }
QFrame#expeditionResultCard {
    background: #132B37;
    border: 1px solid #3F7185;
    border-left: 4px solid #62C4E8;
    border-radius: 7px;
}
QLabel#expeditionResultKicker {
    color: #7FC9EA;
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 2px;
}
QLabel#expeditionResultValue {
    color: #F5FBFD;
    font-family: "Cascadia Mono", "Microsoft YaHei UI", monospace;
    font-size: 19px;
    font-weight: 650;
}
QFrame#replicaRow {
    background: #13232E;
    border: 1px solid #263D49;
    border-radius: 7px;
}
QWidget#sourceWindowCell {
    background: #0F1C25;
    border-radius: 6px;
}
QWidget#sourceWindowCell[grouped="true"] {
    background: #102631;
}
QLabel#sourceWindowLabel {
    color: #8DD5F2;
    font-weight: 700;
}
QLabel#sourceGroupCount {
    color: #7FC9EA;
    font-size: 12px;
}
QLabel#replicaPerformanceEstimate {
    color: #A9C8D6;
    font-size: 12px;
}
QFrame#sourceGroupBar {
    background: #35A5D0;
    border: none;
    border-radius: 2px;
}
QLabel#replicaBadge {
    background: #1C3543;
    color: #8DD5F2;
    border-radius: 5px;
    padding: 5px 8px;
    font-family: "Cascadia Mono", monospace;
    font-weight: 700;
}
QPushButton#closeButton {
    background: transparent;
    color: #9EB0B9;
    border: 1px solid #344B56;
    border-radius: 5px;
    padding: 6px 10px;
}
QPushButton#closeButton:hover {
    color: #FFD8D8;
    border-color: #A95B61;
    background: #40252A;
}
QComboBox#statisticsInterval {
    background: #0F1B24;
    color: #C9D7DE;
    border: 1px solid #344B56;
    border-radius: 4px;
    padding: 4px 7px;
    min-width: 58px;
}
QComboBox#statisticsInterval:hover { border-color: #4D849C; }
QLineEdit {
    background: #0F1B24;
    color: #E8F0F5;
    border: 1px solid #344B56;
    border-radius: 4px;
    padding: 7px 8px;
    selection-background-color: #1D6886;
}
QLineEdit:focus { border-color: #5AB6D9; }
QTableWidget {
    background: #0F1B24;
    alternate-background-color: #11212B;
    border: 1px solid #263D49;
    border-radius: 6px;
    gridline-color: #213641;
    selection-background-color: #1D5C76;
    selection-color: #F4F8FA;
}
QTableWidget::item { padding: 6px 8px; }
QHeaderView::section {
    background: #162A36;
    color: #8DD5F2;
    border: 0;
    border-right: 1px solid #263D49;
    border-bottom: 1px solid #36505D;
    padding: 8px;
    font-weight: 650;
}
QScrollArea { border: 0; background: transparent; }
QScrollArea > QWidget > QWidget { background: transparent; }
QMenu { background: #13232E; border: 1px solid #36505D; padding: 5px; }
QMenu::item { padding: 7px 24px 7px 10px; border-radius: 4px; }
QMenu::item:selected { background: #1D6886; }
QMenu::item:disabled { color: #60747F; }
QTabWidget::pane {
    border: 1px solid #263D49;
    border-radius: 7px;
    top: -1px;
    background: #0C151D;
}
QTabBar::tab {
    background: #0F1B24;
    color: #8FA5B2;
    border: 1px solid #263D49;
    border-bottom: 0;
    padding: 9px 20px;
    margin-right: 2px;
    border-top-left-radius: 6px;
    border-top-right-radius: 6px;
    font-weight: 600;
}
QTabBar::tab:selected {
    background: #13232E;
    color: #7FC9EA;
    border-color: #36505D;
}
QTabBar::tab:hover:!selected { background: #16232E; color: #A9D4E8; }
)");
