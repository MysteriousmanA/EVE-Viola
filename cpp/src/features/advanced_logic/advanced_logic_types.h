#pragma once

#include <QList>
#include <QString>

enum class WhitelistNodeEffect { Keep, Suppress };

struct WhitelistNode {
    int ruleNumber{};
    WhitelistNodeEffect effect{WhitelistNodeEffect::Keep};
    QString sourceTitle;
    bool operator==(const WhitelistNode&) const = default;
};

struct WhitelistSequence {
    QString id;
    QString name;
    bool enabled{true};
    int timeoutMs{30000};
    bool sameSource{true};
    bool ignoreUnrelated{true};
    QList<WhitelistNode> nodes;
    bool operator==(const WhitelistSequence&) const = default;
};

struct AdvancedLogicConfig {
    bool enabled{false};
    QList<WhitelistSequence> sequences;
    bool operator==(const AdvancedLogicConfig&) const = default;
};
