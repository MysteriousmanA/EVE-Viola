#pragma once
#include <functional>

class QObject;
class ReplicaController;
class ReplicaWindow;
class ReplicaRow;

class ReplicaBinding final {
public:
    static void attach(ReplicaController& controller, ReplicaWindow& window,
        ReplicaRow& row, QObject* lifetime, std::function<bool()> restoring);
};
