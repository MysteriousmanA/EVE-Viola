#pragma once

#include <QColorDialog>
#include <QDialog>
#include <QFileDialog>
#include <QInputDialog>

namespace EditorDialogs {

// Block only the editor's owner. Independent replica windows must remain
// clickable so the user can switch back to a game while editing.
class Dialog : public QDialog {
public:
    explicit Dialog(QWidget* parent) : QDialog(parent)
    {
        setWindowModality(Qt::WindowModal);
    }
};

inline QString getText(QWidget* parent, const QString& title, const QString& label,
    QLineEdit::EchoMode mode, const QString& text, bool* accepted)
{
    QInputDialog dialog(parent);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setWindowTitle(title);
    dialog.setLabelText(label);
    dialog.setTextEchoMode(mode);
    dialog.setTextValue(text);
    const bool ok = dialog.exec() == QDialog::Accepted;
    if (accepted) { *accepted = ok; }
    return ok ? dialog.textValue() : QString{};
}

inline int getInt(QWidget* parent, const QString& title, const QString& label,
    int value, int minimum, int maximum, int step, bool* accepted)
{
    QInputDialog dialog(parent);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setWindowTitle(title);
    dialog.setLabelText(label);
    dialog.setInputMode(QInputDialog::IntInput);
    dialog.setIntRange(minimum, maximum);
    dialog.setIntStep(step);
    dialog.setIntValue(value);
    const bool ok = dialog.exec() == QDialog::Accepted;
    if (accepted) { *accepted = ok; }
    return ok ? dialog.intValue() : value;
}

inline QColor getColor(const QColor& initial, QWidget* parent, const QString& title)
{
    QColorDialog dialog(initial, parent);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setWindowTitle(title);
    return dialog.exec() == QDialog::Accepted ? dialog.selectedColor() : QColor{};
}

inline QString getOpenFileName(QWidget* parent, const QString& title,
    const QString& directory, const QString& filter)
{
    QFileDialog dialog(parent, title, directory, filter);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    return dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty()
        ? dialog.selectedFiles().first() : QString{};
}

} // namespace EditorDialogs
