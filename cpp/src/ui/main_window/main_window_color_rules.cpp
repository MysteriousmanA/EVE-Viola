#include "ui/main_window.h"

#include "ui/pages/color_rule/color_rule_page.h"
#include "ui/pages/advanced_logic/advanced_logic_page.h"
#include "features/advanced_logic/advanced_logic_service.h"
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
    advancedLogicService_->setConfiguration(config_.advancedLogic, config_.colorRules);
    advancedLogicPage_->setConfiguration(config_.advancedLogic, config_.colorRules);
    colorRulePage_->setRules(config_.colorRules);
    desktopPetPage_->setColorRules(config_.colorRules);
    for (auto* window : windows_) {
        window->setColorRules(config_.colorRules);
    }
    for (auto* row : rows_) {
        row->setColorRules(config_.colorRules);
    }
}

QWidget* MainWindow::buildAdvancedLogicPage()
{
    advancedLogicPage_ = new AdvancedLogicPage;
    connect(advancedLogicPage_, &AdvancedLogicPage::configurationChanged, this, [this] {
        config_.advancedLogic = advancedLogicPage_->configuration();
        advancedLogicService_->setConfiguration(config_.advancedLogic, config_.colorRules);
        persistConfiguration();
    });
    connect(advancedLogicPage_, &AdvancedLogicPage::priorityChanged, this,
        [this](int number, ColorRulePriority priority) {
            for (auto& rule : config_.colorRules) {
                if (rule.number == number) { rule.priority = priority; break; }
            }
            refreshColorRules();
            persistConfiguration();
        });
    connect(advancedLogicService_, &AdvancedLogicService::eventResolved,
        advancedLogicPage_, &AdvancedLogicPage::showDecision);
    return advancedLogicPage_;
}
