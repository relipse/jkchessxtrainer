#include "herotrainingwidget.h"
#include "ui_herotrainingwidget.h"

HeroTrainingWidget::HeroTrainingWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::HeroTrainingWidget)
{
    ui->setupUi(this);
}

HeroTrainingWidget::~HeroTrainingWidget()
{
    delete ui;
}
