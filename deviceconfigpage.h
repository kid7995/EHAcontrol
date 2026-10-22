#ifndef DEVICECONFIGPAGE_H
#define DEVICECONFIGPAGE_H

#include <QVector>
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

class DeviceConfigPage : public QWidget
{
    Q_OBJECT

public:
    struct Field {
        quint16 parameter = 0;
        QString label;
        QString unit;
        QString hint;
        double factor = 1.0;
        int decimals = 0;
        double defaultValue = 0.0;
        QStringList choices;
    };

    DeviceConfigPage(const QString &title, const QString &introduction,
                     const QVector<Field> &fields, QWidget *parent = nullptr);

    void setConnected(bool connected);

public slots:
    void setParameterValue(quint16 parameter, quint16 value);

signals:
    void readAllRequested();
    void writeRequested(quint16 parameter, quint16 value);
    void statusMessage(const QString &message);

private:
    struct FieldWidget {
        Field definition;
        QDoubleSpinBox *spin = nullptr;
        QComboBox *combo = nullptr;
        quint16 baseline = 0;
        bool loaded = false;
    };

    quint16 currentRawValue(const FieldWidget &field) const;
    void setWidgetValue(FieldWidget &field, quint16 rawValue);
    void applyChanges();
    void restoreLoadedValues();
    void updateDirtyState();

    QVector<FieldWidget> m_fields;
    QPushButton *m_readButton = nullptr;
    QPushButton *m_writeButton = nullptr;
    QPushButton *m_restoreButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    bool m_connected = false;
};

#endif // DEVICECONFIGPAGE_H
