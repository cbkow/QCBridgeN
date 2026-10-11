// SPDX-License-Identifier: MIT
#include "settings_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>

SettingsDialog::SettingsDialog(const SinkConfig &current, QWidget *parent)
    : QDialog(parent), m_id(current.id)
{
    setWindowTitle(QStringLiteral("QCBridgeN Settings"));
    setModal(true);
    auto *form = new QFormLayout(this);

    m_name = new QLineEdit(QString::fromStdString(current.sourceName), this);
    m_name->setToolTip(QStringLiteral("How the source is named to NDI receivers."));
    form->addRow(QStringLiteral("NDI source name"), m_name);

    m_raster = new QComboBox(this);
    m_raster->setEditable(true);
    for (const char *r : { "1920x1080", "1280x720", "3840x2160", "2560x1440", "1080x1920" }) m_raster->addItem(QString::fromLatin1(r));
    const QString cur = QStringLiteral("%1x%2").arg(current.width).arg(current.height);
    if (m_raster->findText(cur) < 0) m_raster->addItem(cur);
    m_raster->setCurrentText(cur);
    m_raster->setToolTip(QStringLiteral("The raster QCView renders for this output; the picture is letterboxed into it. Up to 3840x2160."));
    form->addRow(QStringLiteral("Raster"), m_raster);

    m_content = new QComboBox(this);
    m_content->addItem(QStringLiteral("The viewport (composite)"), static_cast<int>(qcbae::OutputContent::Composite));
    m_content->addItem(QStringLiteral("Side A only"), static_cast<int>(qcbae::OutputContent::SideA));
    m_content->addItem(QStringLiteral("Side B only"), static_cast<int>(qcbae::OutputContent::SideB));
    m_content->setCurrentIndex(m_content->findData(static_cast<int>(current.content)));
    form->addRow(QStringLiteral("Picture"), m_content);

    m_guides = new QCheckBox(QStringLiteral("Safety guides"), this);
    m_guides->setChecked(current.layers & qcbae::kOutputLayerGuides);
    m_notes = new QCheckBox(QStringLiteral("Note strokes"), this);
    m_notes->setChecked(current.layers & qcbae::kOutputLayerAnnotations);
    form->addRow(QStringLiteral("Include"), m_guides);
    form->addRow(QString(), m_notes);

    auto *note = new QLabel(QStringLiteral("QCView's own Output settings can still withhold a layer. "
                                           "Applying re-requests the stream."), this);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: palette(mid);"));
    form->addRow(note);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    form->addRow(buttons);
}

SinkConfig SettingsDialog::config() const
{
    SinkConfig c;
    c.id         = m_id;
    c.sourceName = m_name->text().trimmed().isEmpty() ? std::string("QCView") : m_name->text().trimmed().toStdString();
    const QStringList wh = m_raster->currentText().trimmed().split(QLatin1Char('x'));
    c.width  = wh.size() == 2 ? wh[0].toUInt() : 1920;
    c.height = wh.size() == 2 ? wh[1].toUInt() : 1080;
    if (c.width == 0 || c.height == 0) { c.width = 1920; c.height = 1080; }
    c.content = static_cast<qcbae::OutputContent>(m_content->currentData().toInt());
    c.layers  = (m_guides->isChecked() ? qcbae::kOutputLayerGuides : 0u)
              | (m_notes->isChecked() ? qcbae::kOutputLayerAnnotations : 0u);
    return c;
}
