#include "ui/main_window.h"

#include "ui/pages/color_rule/color_rule_page.h"
#include "ui/pages/color_rule/color_rule_controller.h"
#include "ui/desktop_pet_page.h"
#include "ui/replica_row.h"
#include "ui/replica_window.h"

QWidget* MainWindow::buildColorRulePage()
{
    colorRulePage_ = new ColorRulePage;
    colorRuleController_ = new ColorRuleController(config_, this);
    connect(colorRuleController_, &ColorRuleController::rulesChanged, this, &MainWindow::refreshColorRules);
    connect(colorRuleController_, &ColorRuleController::persistRequested, this, &MainWindow::persistConfiguration);
    connect(colorRulePage_, &ColorRulePage::addRequested, colorRuleController_, &ColorRuleController::createColorRule);
    connect(
        colorRulePage_, &ColorRulePage::editRequested,
        colorRuleController_, &ColorRuleController::editColorRuleNumber);
    connect(
        colorRulePage_, &ColorRulePage::deleteRequested,
        colorRuleController_, &ColorRuleController::deleteColorRuleNumber);
    return colorRulePage_;
}

void MainWindow::refreshColorRules()
{
    colorRulePage_->setRules(config_.colorRules);
    desktopPetPage_->setColorRules(config_.colorRules);
    for (auto* window : windows_) {
        window->setColorRules(config_.colorRules);
    }
    for (auto* row : rows_) {
        row->setColorRules(config_.colorRules);
    }
}
