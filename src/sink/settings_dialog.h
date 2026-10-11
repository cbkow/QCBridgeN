// SPDX-License-Identifier: MIT
// The sink's few settings, from the tray: source name, raster, content,
// layers. Applying restarts the attach so the new request reaches QCView.

#pragma once

#include "bus_sink.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const SinkConfig &current, QWidget *parent = nullptr);
    SinkConfig config() const;

private:
    QLineEdit *m_name   = nullptr;
    QComboBox *m_raster = nullptr;
    QComboBox *m_content = nullptr;
    QCheckBox *m_guides = nullptr;
    QCheckBox *m_notes  = nullptr;
    std::string m_id;
};
