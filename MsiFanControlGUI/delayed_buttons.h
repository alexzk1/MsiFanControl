#pragma once

#include <QObject>
#include <QPushButton>
#include <QTimer>

// If this object is attached to the button it delays clicks.

inline void AttachDelay(QPushButton *button, int delayMs)
{
    QObject::connect(button, &QPushButton::clicked, [delayMs, button]() {
        button->setEnabled(false);
        QTimer::singleShot(delayMs, button, [button]() {
            button->setEnabled(true);
        });
    });
}
