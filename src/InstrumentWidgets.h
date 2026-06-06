#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

class AttitudeIndicatorWidget : public QWidget {
public:
    explicit AttitudeIndicatorWidget(QWidget *parent = nullptr);

    void setAttitude(double rollDeg, double pitchDeg);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double m_rollDeg = 0.0;
    double m_pitchDeg = 0.0;
};

class CircularGaugeWidget : public QWidget {
public:
    explicit CircularGaugeWidget(const QString &title,
                                 const QString &unit,
                                 double minimum,
                                 double maximum,
                                 QWidget *parent = nullptr);

    void setValue(double value);
    void setAccentColor(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_title;
    QString m_unit;
    double m_minimum = 0.0;
    double m_maximum = 100.0;
    double m_value = 0.0;
    QColor m_accent = QColor(35, 192, 164);
};

class CompassWidget : public QWidget {
public:
    explicit CompassWidget(QWidget *parent = nullptr);

    void setHeading(double headingDeg);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double m_headingDeg = 0.0;
};
