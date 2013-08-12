#ifndef HEROTRAININGWIDGET_H
#define HEROTRAININGWIDGET_H

#include <QWidget>

namespace Ui {
class HeroTrainingWidget;
}

class HeroTrainingWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit HeroTrainingWidget(QWidget *parent = 0);
    ~HeroTrainingWidget();
    
public:
    Ui::HeroTrainingWidget *ui;
};

#endif // HEROTRAININGWIDGET_H
