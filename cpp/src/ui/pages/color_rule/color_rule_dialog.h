#pragma once
#include <optional>
#include "config/config_types.h"
class QWidget;
std::optional<ColorRule> promptColorRule(QWidget *parent, const ColorRule &initial);
