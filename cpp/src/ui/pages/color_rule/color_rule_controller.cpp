#include "ui/pages/color_rule/color_rule_controller.h"
#include "features/color_rule/color_rule_config.h"
#include "ui/pages/color_rule/color_rule_dialog.h"
#include <QMessageBox>
ColorRuleController::ColorRuleController(AppConfig &config, QWidget *host)
    : QObject(host), config_(config), host_(host)
{
}
void ColorRuleController::createColorRule()
{
    const auto rule = promptColorRule(host_, {});
    if (!rule.has_value()) {
        return;
    }
    static_cast<void>(addColorRule(config_, *rule));
    emit rulesChanged();
    emit persistRequested();
}
void ColorRuleController::editColorRuleNumber(int number)
{
    for (const auto &current : config_.colorRules) {
        if (current.number == number) {
            const auto edited = promptColorRule(host_, current);
            if (edited.has_value() && updateColorRule(config_, number, *edited)) {
                emit rulesChanged();
                emit persistRequested();
            }
            return;
        }
    }
}
void ColorRuleController::deleteColorRuleNumber(int number)
{
    if (QMessageBox::question(host_, QStringLiteral("删除图色规则"),
                              QStringLiteral("确定删除规则 #%1？").arg(number)) !=
        QMessageBox::Yes) {
        return;
    }
    if (removeColorRule(config_, number)) {
        emit rulesChanged();
        emit persistRequested();
    }
}
